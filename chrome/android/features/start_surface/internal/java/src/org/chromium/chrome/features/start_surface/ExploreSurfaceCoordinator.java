// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.ViewGroup;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.StreamLifecycleManager;
import org.chromium.chrome.browser.feed.shared.FeedSurfaceDelegate;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.ScrollableContainerDelegate;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderView;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.start_surface.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator to control the explore surface. */
class ExploreSurfaceCoordinator implements FeedSurfaceDelegate {
    private final ChromeActivity mActivity;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final FeedSurfaceCreator mFeedSurfaceCreator;
    private final Supplier<Tab> mParentTabSupplier;
    private final boolean mHasHeader;

    // mExploreSurfaceNavigationDelegate is lightweight, we keep it across FeedSurfaceCoordinators
    // after creating it during the first show.
    private ExploreSurfaceNavigationDelegate mExploreSurfaceNavigationDelegate;

    /** Interface to create {@link FeedSurfaceCoordinator} */
    interface FeedSurfaceCreator {
        /**
         * Creates the {@link FeedSurfaceCoordinator} for the specified mode.
         * @param isInNightMode Whether or not the feed surface is going to display in night mode.
         * @return The {@link FeedSurfaceCoordinator}.
         */
        FeedSurfaceCoordinator createFeedSurfaceCoordinator(
                boolean isInNightMode, boolean isPlaceholderShown);
    }

    ExploreSurfaceCoordinator(ChromeActivity activity, ViewGroup parentView,
            PropertyModel containerPropertyModel, boolean hasHeader,
            BottomSheetController bottomSheetController, Supplier<Tab> parentTabSupplier,
            ScrollableContainerDelegate scrollableContainerDelegate) {
        mActivity = activity;
        mHasHeader = hasHeader;
        mParentTabSupplier = parentTabSupplier;

        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                containerPropertyModel, parentView, ExploreSurfaceViewBinder::bind);
        mFeedSurfaceCreator = new FeedSurfaceCreator() {
            @Override
            public FeedSurfaceCoordinator createFeedSurfaceCoordinator(
                    boolean isInNightMode, boolean isPlaceholderShown) {
                return internalCreateFeedSurfaceCoordinator(mHasHeader, isInNightMode,
                        isPlaceholderShown, bottomSheetController, scrollableContainerDelegate);
            }
        };
    }

    /**
     * Gets the {@link FeedSurfaceCreator}.
     * @return the {@link FeedSurfaceCreator}.
     */
    FeedSurfaceCreator getFeedSurfaceCreator() {
        return mFeedSurfaceCreator;
    }

    // Implements FeedSurfaceDelegate.
    @Override
    public StreamLifecycleManager createStreamLifecycleManager(Stream stream, Activity activity) {
        return new ExploreSurfaceStreamLifecycleManager(stream, activity, mHasHeader);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return false;
    }

    private FeedSurfaceCoordinator internalCreateFeedSurfaceCoordinator(boolean hasHeader,
            boolean isInNightMode, boolean isPlaceholderShown,
            BottomSheetController bottomSheetController,
            ScrollableContainerDelegate scrollableContainerDelegate) {
        if (mExploreSurfaceNavigationDelegate == null) {
            mExploreSurfaceNavigationDelegate =
                    new ExploreSurfaceNavigationDelegate(mParentTabSupplier);
        }
        Profile profile = Profile.getLastUsedRegularProfile();

        SectionHeaderView sectionHeaderView = null;
        if (hasHeader) {
            LayoutInflater inflater = LayoutInflater.from(mActivity);
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_FEED)) {
                sectionHeaderView = (SectionHeaderView) inflater.inflate(
                        org.chromium.chrome.R.layout.new_tab_page_multi_feed_header, null, false);
            } else {
                sectionHeaderView = (SectionHeaderView) inflater.inflate(
                        org.chromium.chrome.R.layout.new_tab_page_feed_v2_expandable_header, null,
                        false);
            }
        }

        FeedSurfaceCoordinator feedSurfaceCoordinator = new FeedSurfaceCoordinator(mActivity,
                mActivity.getSnackbarManager(), mActivity.getWindowAndroid(), null, null,
                sectionHeaderView, isInNightMode, this, mExploreSurfaceNavigationDelegate, profile,
                isPlaceholderShown, bottomSheetController, mActivity.getShareDelegateSupplier(),
                scrollableContainerDelegate);
        feedSurfaceCoordinator.getView().setId(R.id.start_surface_explore_view);
        return feedSurfaceCoordinator;
        // TODO(crbug.com/982018): Customize surface background for incognito and dark mode.
        // TODO(crbug.com/982018): Hide signin promo UI in incognito mode.
    }
}
