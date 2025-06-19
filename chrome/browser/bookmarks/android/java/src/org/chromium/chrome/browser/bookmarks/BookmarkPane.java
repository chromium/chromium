// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_FADE_DURATION_MS;

import android.app.Activity;
import android.content.ComponentName;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
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
public class BookmarkPane implements Pane {

    // Below are dependencies of the pane itself.
    private final DoubleConsumer mOnToolbarAlphaChange;
    private final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<FullButtonData> mEmptyActionButtonSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHairlineVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<View> mHubOverlayViewSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mHubSearchEnabledStateSupplier =
            new ObservableSupplierImpl<>();

    // FrameLayout which has HistoryManager's root view as the only child.
    private final FrameLayout mRootView;
    // Below are dependencies to create the HistoryManger.
    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;

    private BookmarkManagerCoordinator mBookmarkManager;
    private BookmarkOpener mBookmarkOpener;

    /**
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param activity Used as a dependency to BookmarkManager.
     * @param snackbarManager Used as a dependency to BookmarkManager.
     * @param profileProviderSupplier Used as a dependency to BookmarkManager.
     */
    public BookmarkPane(
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull Activity activity,
            @NonNull SnackbarManager snackbarManager,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier) {
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
    public void setPaneHubController(@Nullable PaneHubController paneHubController) {}

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        if (loadHint == LoadHint.HOT && mBookmarkManager == null) {
            ComponentName componentName = mActivity.getComponentName();
            Profile originalProfile = mProfileProviderSupplier.get().getOriginalProfile();
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
                            PriceDropNotificationManagerFactory.create(originalProfile));
            mBookmarkManager.updateForUrl(UrlConstants.BOOKMARKS_URL);
            mRootView.addView(mBookmarkManager.getView());
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

    @Override
    public ObservableSupplier<View> getHubOverlayViewSupplier() {
        return mHubOverlayViewSupplier;
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

    private void destroyManagerAndRemoveView() {
        if (mBookmarkManager != null) {
            mBookmarkOpener = null;
            mBookmarkManager.onDestroyed();
            mBookmarkManager = null;
        }
        mRootView.removeAllViews();
    }
}
