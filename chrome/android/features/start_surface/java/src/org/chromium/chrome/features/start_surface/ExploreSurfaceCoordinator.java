// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;
import android.app.Activity;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.feed.FeedActionDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.FeedSurfaceDelegate;
import org.chromium.chrome.browser.feed.FeedSurfaceLifecycleManager;
import org.chromium.chrome.browser.feed.FeedSwipeRefreshLayout;
import org.chromium.chrome.browser.feed.ScrollableContainerDelegate;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger.SurfaceType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/**
 * Coordinates the Explore surface as it appears on the Start surface. This is a think wrapper
 * around FeedSurfaceCoordinator.
 */
public class ExploreSurfaceCoordinator {
    @VisibleForTesting
    public static final String FEED_STREAM_CREATED_TIME_MS_UMA = "FeedStreamCreatedTime";
    @VisibleForTesting
    public static final String FEED_CONTENT_FIRST_LOADED_TIME_MS_UMA = "FeedContentFirstLoadedTime";

    private final Activity mActivity;
    private final FeedSurfaceCoordinator mFeedSurfaceCoordinator;
    private final ExploreSurfaceNavigationDelegate mExploreSurfaceNavigationDelegate;
    private final boolean mIsPlaceholderShownInitially;

    private long mContentFirstAvailableTimeMs;
    // Whether missing a histogram record when onOverviewShownAtLaunch() is called. It is possible
    // that Feed content is still loading at that time and the {@link mContentFirstAvailableTimeMs}
    // hasn't been set yet.
    private boolean mHasPendingUmaRecording;
    private long mActivityCreationTimeMs;
    private long mStreamCreatedTimeMs;

    public ExploreSurfaceCoordinator(Profile profile, Activity activity, boolean isInNightMode,
            boolean isPlaceholderShown, BottomSheetController bottomSheetController,
            ScrollableContainerDelegate scrollableContainerDelegate,
            @NewTabPageLaunchOrigin int launchOrigin, @NonNull Supplier<Toolbar> toolbarSupplier,
            long embeddingSurfaceConstructedTimeNs, FeedSwipeRefreshLayout swipeRefreshLayout,
            ViewGroup parentView, Supplier<Tab> parentTabSupplier, SnackbarManager snackbarManager,
            Supplier<ShareDelegate> shareDelegateSupplier, WindowAndroid windowAndroid,
            TabModelSelector tabModelSelector) {
        mActivity = activity;
        mExploreSurfaceNavigationDelegate = new ExploreSurfaceNavigationDelegate(parentTabSupplier);
        mIsPlaceholderShownInitially = isPlaceholderShown;

        mFeedSurfaceCoordinator = new FeedSurfaceCoordinator(mActivity, snackbarManager,
                windowAndroid, /*snapScrollHelper=*/null, /*ntpHeader=*/null,
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow),
                isInNightMode, /*delegate=*/new ExploreFeedSurfaceDelegate(), profile,
                isPlaceholderShown, bottomSheetController, shareDelegateSupplier,
                scrollableContainerDelegate, launchOrigin,
                PrivacyPreferencesManagerImpl.getInstance(), toolbarSupplier,
                SurfaceType.START_SURFACE, embeddingSurfaceConstructedTimeNs, swipeRefreshLayout,
                /*overScrollDisabled=*/true, parentView,
                new ExploreSurfaceActionDelegate(
                        snackbarManager, BookmarkModel.getForProfile(profile), tabModelSelector),
                HelpAndFeedbackLauncherImpl.getForProfile(profile), tabModelSelector);

        mFeedSurfaceCoordinator.getView().setId(R.id.start_surface_explore_view);
        // TODO(crbug.com/982018): Customize surface background for incognito and dark mode.
        // TODO(crbug.com/982018): Hide signin promo UI in incognito mode.
    }

    public void destroy() {
        mFeedSurfaceCoordinator.destroy();
    }

    public View getView() {
        return mFeedSurfaceCoordinator.getView();
    }

    public RecyclerView getRecyclerView() {
        return mFeedSurfaceCoordinator.getRecyclerView();
    }

    public void onOverviewShownAtLaunch(long activityCreationTimeMs) {
        assert mActivityCreationTimeMs == 0;
        mActivityCreationTimeMs = activityCreationTimeMs;

        if (!maybeRecordContentLoadingTime() && mFeedSurfaceCoordinator.isLoadingFeed()) {
            mHasPendingUmaRecording = true;
        }
        StartSurfaceConfiguration.recordHistogram(FEED_STREAM_CREATED_TIME_MS_UMA,
                mStreamCreatedTimeMs - activityCreationTimeMs, mIsPlaceholderShownInitially);
    }

    private boolean maybeRecordContentLoadingTime() {
        if (mActivityCreationTimeMs == 0 || mContentFirstAvailableTimeMs == 0) return false;

        StartSurfaceConfiguration.recordHistogram(FEED_CONTENT_FIRST_LOADED_TIME_MS_UMA,
                mContentFirstAvailableTimeMs - mActivityCreationTimeMs,
                mIsPlaceholderShownInitially);
        return true;
    }

    public void setTabIdFromLaunchOrigin(@NewTabPageLaunchOrigin int launchOrigin) {
        mFeedSurfaceCoordinator.setTabIdFromLaunchOrigin(launchOrigin);
    }

    public void enableSwipeRefresh(boolean isVisible) {
        mFeedSurfaceCoordinator.enableSwipeRefresh(isVisible);
    }

    public FeedReliabilityLogger getFeedReliabilityLogger() {
        return mFeedSurfaceCoordinator.getReliabilityLogger();
    }

    private class ExploreSurfaceActionDelegate extends FeedActionDelegateImpl {
        ExploreSurfaceActionDelegate(SnackbarManager snackbarManager, BookmarkModel bookmarkModel,
                TabModelSelector tabModelSelector) {
            super(mActivity, snackbarManager, mExploreSurfaceNavigationDelegate, bookmarkModel,
                    BrowserUiUtils.HostSurface.START_SURFACE, tabModelSelector);
        }

        @Override
        public void onContentsChanged() {
            if (mContentFirstAvailableTimeMs == 0) {
                mContentFirstAvailableTimeMs = SystemClock.elapsedRealtime();
                if (mHasPendingUmaRecording) {
                    maybeRecordContentLoadingTime();
                    mHasPendingUmaRecording = false;
                }
            }
        }
        @Override
        public void onStreamCreated() {
            mStreamCreatedTimeMs = SystemClock.elapsedRealtime();
        }
    }

    private class ExploreFeedSurfaceDelegate implements FeedSurfaceDelegate {
        @Override
        public FeedSurfaceLifecycleManager createStreamLifecycleManager(
                Activity activity, SurfaceCoordinator coordinator) {
            return new ExploreSurfaceFeedLifecycleManager(
                    activity, (FeedSurfaceCoordinator) coordinator);
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent ev) {
            return false;
        }
    }

    public FeedActionDelegate getFeedActionDelegateForTesting() {
        return mFeedSurfaceCoordinator.getActionDelegateForTesting(); // IN-TEST
    }
}
