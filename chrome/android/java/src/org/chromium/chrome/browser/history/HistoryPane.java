// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_FADE_DURATION_MS;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import java.util.function.Supplier;

/** A {@link Pane} representing history. */
@NullMarked
public class HistoryPane implements Pane {

    // Below are dependencies of the pane itself.
    private final DoubleConsumer mOnToolbarAlphaChange;
    private final ObservableSupplierImpl<@Nullable DisplayButtonData> mReferenceButtonSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<FullButtonData> mEmptyActionButtonSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHairlineVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<@Nullable View> mHubOverlayViewSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHubSearchEnabledStateSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHubSearchBoxVisibilitySupplier =
            new ObservableSupplierImpl<>();

    // FrameLayout which has HistoryManager's root view as the only child.
    private final FrameLayout mRootView;
    // Below are dependencies to create the HistoryManger.
    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final Supplier<@Nullable BottomSheetController> mBottomSheetController;
    private final Supplier<@Nullable Tab> mTabSupplier;

    private @Nullable HistoryManager mHistoryManager;
    private @Nullable PaneHubController mPaneHubController;

    /**
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param activity Used as a dependency to HistoryManager.
     * @param snackbarManager Used as a dependency to HistoryManager.
     * @param profileProviderSupplier Used as a dependency to HistoryManager.
     * @param bottomSheetController Used as a dependency to HistoryManager.
     * @param tabSupplier Used as a dependency to HistoryManager.
     */
    public HistoryPane(
            DoubleConsumer onToolbarAlphaChange,
            Activity activity,
            SnackbarManager snackbarManager,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            Supplier<@Nullable BottomSheetController> bottomSheetController,
            Supplier<@Nullable Tab> tabSupplier) {
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

    @Override
    public ViewGroup getRootView() {
        return mRootView;
    }

    @Override
    public @Nullable MenuOrKeyboardActionHandler getMenuOrKeyboardActionHandler() {
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
                            assumeNonNull(mProfileProviderSupplier.get()).getOriginalProfile(),
                            mBottomSheetController,
                            mTabSupplier,
                            new BrowsingHistoryBridge(
                                    mProfileProviderSupplier.get().getOriginalProfile()),
                            new HistoryUmaRecorder(),
                            /* clientPackageName= */ null,
                            /* shouldShowClearData= */ true,
                            /* launchedForApp= */ false,
                            /* showAppFilter= */ true,
                            this::onHistoryItemOpened,
                            // TODO(crbug.com/427776544): make history pane support edge to edge.
                            /* edgeToEdgePadAdjusterGenerator= */ null);
            mRootView.addView(mHistoryManager.getView());
        } else if (loadHint == LoadHint.COLD) {
            destroyManagerAndRemoveView();
        }
    }

    @Override
    public ObservableSupplier<FullButtonData> getActionButtonDataSupplier() {
        return mEmptyActionButtonSupplier;
    }

    @Override
    public ObservableSupplier<@Nullable DisplayButtonData> getReferenceButtonDataSupplier() {
        return mReferenceButtonSupplier;
    }

    @Override
    public ObservableSupplier<Boolean> getHairlineVisibilitySupplier() {
        return mHairlineVisibilitySupplier;
    }

    @Override
    public ObservableSupplier<@Nullable View> getHubOverlayViewSupplier() {
        return mHubOverlayViewSupplier;
    }

    @Override
    public @Nullable HubLayoutAnimationListener getHubLayoutAnimationListener() {
        return null;
    }

    @Override
    public HubLayoutAnimatorProvider createShowHubLayoutAnimatorProvider(
            HubContainerView hubContainerView) {
        return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                hubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
    }

    @Override
    public HubLayoutAnimatorProvider createHideHubLayoutAnimatorProvider(
            HubContainerView hubContainerView) {
        return FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                hubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
    }

    @Override
    public ObservableSupplier<Boolean> getHubSearchEnabledStateSupplier() {
        return mHubSearchEnabledStateSupplier;
    }

    @Override
    public ObservableSupplier<Boolean> getHubSearchBoxVisibilitySupplier() {
        return mHubSearchBoxVisibilitySupplier;
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
