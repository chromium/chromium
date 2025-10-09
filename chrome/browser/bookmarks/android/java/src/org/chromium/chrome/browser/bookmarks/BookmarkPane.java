// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_FADE_DURATION_MS;

import android.app.Activity;
import android.content.ComponentName;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.function.DoubleConsumer;

/** A {@link Pane} representing history. */
@NullMarked
public class BookmarkPane implements Pane {

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

    private @Nullable BookmarkManagerCoordinator mBookmarkManager;
    private @Nullable BookmarkOpener mBookmarkOpener;

    /**
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param activity Used as a dependency to BookmarkManager.
     * @param snackbarManager Used as a dependency to BookmarkManager.
     * @param profileProviderSupplier Used as a dependency to BookmarkManager.
     */
    public BookmarkPane(
            DoubleConsumer onToolbarAlphaChange,
            Activity activity,
            SnackbarManager snackbarManager,
            OneshotSupplier<ProfileProvider> profileProviderSupplier) {
        mOnToolbarAlphaChange = onToolbarAlphaChange;
        mReferenceButtonSupplier.set(
                new ResourceButtonData(
                        R.string.menu_bookmarks,
                        R.string.menu_bookmarks,
                        R.drawable.star_outline_24dp));

        mRootView = new FrameLayout(activity);
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mProfileProviderSupplier = profileProviderSupplier;
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.BOOKMARKS;
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
    public void setPaneHubController(@Nullable PaneHubController paneHubController) {}

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        if (loadHint == LoadHint.HOT && mBookmarkManager == null) {
            ComponentName componentName = mActivity.getComponentName();
            Profile originalProfile =
                    assumeNonNull(mProfileProviderSupplier.get()).getOriginalProfile();
            mBookmarkOpener =
                    new BookmarkOpenerImpl(
                            () -> BookmarkModel.getForProfile(originalProfile),
                            mActivity,
                            componentName);
            mBookmarkManager =
                    new BookmarkManagerCoordinator(
                            mActivity,
                            /* isDialogUi= */ false,
                            mSnackbarManager,
                            originalProfile,
                            new BookmarkUiPrefs(ChromeSharedPreferences.getInstance()),
                            mBookmarkOpener,
                            new BookmarkManagerOpenerImpl(),
                            PriceDropNotificationManagerFactory.create(originalProfile),
                            // TODO(crbug.com/427776544): make bookmark pane support edge to edge.
                            /* edgeToEdgePadAdjusterGenerator= */ null,
                            /* backPressManager= */ null);
            mBookmarkManager.updateForUrl(UrlConstants.BOOKMARKS_NATIVE_URL);
            mRootView.addView(mBookmarkManager.getView());
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

    private void destroyManagerAndRemoveView() {
        if (mBookmarkManager != null) {
            mBookmarkOpener = null;
            mBookmarkManager.onDestroyed();
            mBookmarkManager = null;
        }
        mRootView.removeAllViews();
    }
}
