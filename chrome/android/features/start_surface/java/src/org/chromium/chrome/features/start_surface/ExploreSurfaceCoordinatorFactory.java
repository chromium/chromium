// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.FeedSwipeRefreshLayout;
import org.chromium.chrome.browser.feed.ScrollableContainerDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator to control the explore surface. */
class ExploreSurfaceCoordinatorFactory {
    private final Activity mActivity;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final Supplier<Tab> mParentTabSupplier;
    private final SnackbarManager mSnackbarManager;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final WindowAndroid mWindowAndroid;
    private final JankTracker mJankTracker;
    private final TabModelSelector mTabModelSelector;
    private final BottomSheetController mBottomSheetController;
    private final ScrollableContainerDelegate mScrollableContainerDelegate;
    private final Supplier<Toolbar> mToolbarSupplier;
    private final long mEmbeddingSurfaceConstructedTimeNs;
    @Nullable private final FeedSwipeRefreshLayout mSwipeRefreshLayout;
    @NonNull private final ViewGroup mParentView;
    private ExploreSurfaceFeedLifecycleManager mExploreSurfaceFeedLifecycleManager;

    /**
     * @param activity The current {@link Activity}.
     * @param parentView The parent {@link ViewGroup} for the start surface.
     * @param containerPropertyModel The {@link PropertyModel} for the container.
     * @param bottomSheetController Controls the state of the bottom sheet.
     * @param parentTabSupplier Supplies the current {@link Tab}.
     * @param scrollableContainerDelegate Delegate for the scrollable container.
     * @param snackbarManager Manages the snackbar.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate}.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param jankTracker tracks jank.
     * @param tabModelSelector The current {@link TabModelSelector}.
     * @param toolbarSupplier Supplies the {@link Toolbar}.
     * @param embeddingSurfaceConstructedTimeNs Timestamp taken when the caller was constructed.
     * @param swipeRefreshLayout The layout to support pull-to-refresg.
     */
    ExploreSurfaceCoordinatorFactory(
            @NonNull Activity activity,
            @NonNull ViewGroup parentView,
            @NonNull PropertyModel containerPropertyModel,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull Supplier<Tab> parentTabSupplier,
            @NonNull ScrollableContainerDelegate scrollableContainerDelegate,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull WindowAndroid windowAndroid,
            @NonNull JankTracker jankTracker,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Supplier<Toolbar> toolbarSupplier,
            long embeddingSurfaceConstructedTimeNs,
            @Nullable FeedSwipeRefreshLayout swipeRefreshLayout) {
        mActivity = activity;
        mParentView = parentView;
        mParentTabSupplier = parentTabSupplier;
        mSnackbarManager = snackbarManager;
        mShareDelegateSupplier = shareDelegateSupplier;
        mWindowAndroid = windowAndroid;
        mJankTracker = jankTracker;
        mTabModelSelector = tabModelSelector;
        mBottomSheetController = bottomSheetController;
        mScrollableContainerDelegate = scrollableContainerDelegate;
        mToolbarSupplier = toolbarSupplier;
        mEmbeddingSurfaceConstructedTimeNs = embeddingSurfaceConstructedTimeNs;
        mSwipeRefreshLayout = swipeRefreshLayout;
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        containerPropertyModel, parentView, ExploreSurfaceViewBinder::bind);
    }

    /**
     * Creates the {@link ExploreSurfaceCoordinator} for the specified mode.
     * @param isInNightMode Whether or not the feed surface is going to display in night mode.
     * @param launchOrigin Where the feed was launched from.
     * @return The {@link ExploreSurfaceCoordinator}.
     */
    ExploreSurfaceCoordinator create(
            boolean isInNightMode,
            boolean isPlaceholderShown,
            @NewTabPageLaunchOrigin int launchOrigin) {
        Profile profile = Profile.getLastUsedRegularProfile();

        return new ExploreSurfaceCoordinator(
                profile,
                mActivity,
                isInNightMode,
                isPlaceholderShown,
                mBottomSheetController,
                mScrollableContainerDelegate,
                launchOrigin,
                mToolbarSupplier,
                mEmbeddingSurfaceConstructedTimeNs,
                mSwipeRefreshLayout,
                mParentView,
                mParentTabSupplier,
                mSnackbarManager,
                mShareDelegateSupplier,
                mWindowAndroid,
                mJankTracker,
                mTabModelSelector);
    }
}
