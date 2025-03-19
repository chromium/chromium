// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_FADE_DURATION_MS;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FadeHubLayoutAnimationFactory;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubColorScheme;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimationListener;
import org.chromium.chrome.browser.hub.HubLayoutAnimatorProvider;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;

import java.util.function.DoubleConsumer;

/** A {@link Pane} representing history. */
public class HistoryPane implements Pane {

    // Below are dependencies of the pane itself.
    private final DoubleConsumer mOnToolbarAlphaChange;
    private final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<FullButtonData> mEmptyActionButtonSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHairlineVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHubSearchEnabledStateSupplier =
            new ObservableSupplierImpl<>();

    // FrameLayout which has HistoryManager's root view as the only child.
    private final FrameLayout mRootView;
    // Below are dependencies to create the HistoryManger.
    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final Supplier<BottomSheetController> mBottomSheetController;
    private final Supplier<Tab> mTabSupplier;

    private HistoryManager mHistoryManager;
    private PaneHubController mPaneHubController;

    /**
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param activity Used as a dependency to HistoryManager.
     * @param snackbarManager Used as a dependency to HistoryManager.
     * @param profileProviderSupplier Used as a dependency to HistoryManager.
     * @param bottomSheetController Used as a dependency to HistoryManager.
     * @param tabSupplier Used as a dependency to HistoryManager.
     */
    public HistoryPane(
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull Activity activity,
            @NonNull SnackbarManager snackbarManager,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull Supplier<BottomSheetController> bottomSheetController,
            @NonNull Supplier<Tab> tabSupplier) {
        mOnToolbarAlphaChange = onToolbarAlphaChange;
        mReferenceButtonSupplier.set(
                new ResourceButtonData(
                        R.string.menu_history, R.string.menu_history, R.drawable.ic_history_24dp));

        mRootView = new FrameLayout(activity);
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mProfileProviderSupplier = profileProviderSupplier;
        mBottomSheetController = bottomSheetController;
        mTabSupplier = tabSupplier;
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.HISTORY;
    }

    @NonNull
    @Override
    public ViewGroup getRootView() {
        return mRootView;
    }

    @Nullable
    @Override
    public MenuOrKeyboardActionHandler getMenuOrKeyboardActionHandler() {
        return null;
    }

    @Override
    public boolean getMenuButtonVisible() {
        return false;
    }

    @Override
    public @HubColorScheme int getColorScheme() {
        return HubColorScheme.DEFAULT;
    }

    @Override
    public void destroy() {
        destroyManagerAndRemoveView();
    }

    @Override
    public void setPaneHubController(@Nullable PaneHubController paneHubController) {
        mPaneHubController = paneHubController;
    }

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        if (loadHint == LoadHint.HOT && mHistoryManager == null) {
            mHistoryManager =
                    new HistoryManager(
                            mActivity,
                            /* isSeparateActivity= */ false,
                            mSnackbarManager,
                            mProfileProviderSupplier.get().getOriginalProfile(),
                            mBottomSheetController,
                            mTabSupplier,
                            new BrowsingHistoryBridge(
                                    mProfileProviderSupplier.get().getOriginalProfile()),
                            new HistoryUmaRecorder(),
                            /* clientPackageName= */ null,
                            /* shouldShowClearData= */ true,
                            /* launchedForApp= */ false,
                            /* showAppFilter= */ true,
                            this::onHistoryItemOpened);
            mRootView.addView(mHistoryManager.getView());
        } else if (loadHint == LoadHint.COLD) {
            destroyManagerAndRemoveView();
        }
    }

    @NonNull
    @Override
    public ObservableSupplier<FullButtonData> getActionButtonDataSupplier() {
        return mEmptyActionButtonSupplier;
    }

    @NonNull
    @Override
    public ObservableSupplier<DisplayButtonData> getReferenceButtonDataSupplier() {
        return mReferenceButtonSupplier;
    }

    @NonNull
    @Override
    public ObservableSupplier<Boolean> getHairlineVisibilitySupplier() {
        return mHairlineVisibilitySupplier;
    }

    @Nullable
    @Override
    public HubLayoutAnimationListener getHubLayoutAnimationListener() {
        return null;
    }

    @NonNull
    @Override
    public HubLayoutAnimatorProvider createShowHubLayoutAnimatorProvider(
            @NonNull HubContainerView hubContainerView) {
        return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                hubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
    }

    @NonNull
    @Override
    public HubLayoutAnimatorProvider createHideHubLayoutAnimatorProvider(
            @NonNull HubContainerView hubContainerView) {
        return FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                hubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
    }

    @NonNull
    @Override
    public ObservableSupplier<Boolean> getHubSearchEnabledStateSupplier() {
        return mHubSearchEnabledStateSupplier;
    }

    private void onHistoryItemOpened() {
        assert mPaneHubController != null;
        mPaneHubController.selectTabAndHideHub(Tab.INVALID_TAB_ID);
    }

    private void destroyManagerAndRemoveView() {
        if (mHistoryManager != null) {
            mHistoryManager.onDestroyed();
            mHistoryManager = null;
        }
        mRootView.removeAllViews();
    }
}
