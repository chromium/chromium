// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.ViewGroup;

import com.google.android.libraries.feed.api.client.stream.Stream;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.feed.FeedProcessScopeFactory;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.StreamLifecycleManager;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderView;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator to control the explore surface. */
class ExploreSurfaceCoordinator implements FeedSurfaceCoordinator.FeedSurfaceDelegate {
    private final ChromeActivity mActivity;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final FeedSurfaceCreator mFeedSurfaceCreator;
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
        FeedSurfaceCoordinator createFeedSurfaceCoordinator(boolean isInNightMode);
    }

    ExploreSurfaceCoordinator(ChromeActivity activity, ViewGroup parentView,
            PropertyModel containerPropertyModel, boolean hasHeader) {
        mActivity = activity;
        mHasHeader = hasHeader;

        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                containerPropertyModel, parentView, ExploreSurfaceViewBinder::bind);
        mFeedSurfaceCreator = new FeedSurfaceCreator() {
            @Override
            public FeedSurfaceCoordinator createFeedSurfaceCoordinator(boolean isInNightMode) {
                return internalCreateFeedSurfaceCoordinator(mHasHeader, isInNightMode);
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

    // Implements FeedSurfaceCoordinator.FeedSurfaceDelegate.
    @Override
    public StreamLifecycleManager createStreamLifecycleManager(Stream stream, Activity activity) {
        return new ExploreSurfaceStreamLifecycleManager(stream, activity, mHasHeader);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return false;
    }

    private FeedSurfaceCoordinator internalCreateFeedSurfaceCoordinator(
            boolean hasHeader, boolean isInNightMode) {
        if (mExploreSurfaceNavigationDelegate == null) {
            mExploreSurfaceNavigationDelegate = new ExploreSurfaceNavigationDelegate(mActivity);
        }

        ExploreSurfaceActionHandler exploreSurfaceActionHandler =
                new ExploreSurfaceActionHandler(mExploreSurfaceNavigationDelegate,
                        FeedProcessScopeFactory.getFeedConsumptionObserver(),
                        FeedProcessScopeFactory.getFeedOfflineIndicator(),
                        OfflinePageBridge.getForProfile(Profile.getLastUsedProfile()),
                        FeedProcessScopeFactory.getFeedLoggingBridge());

        SectionHeaderView sectionHeaderView = null;
        if (hasHeader) {
            LayoutInflater inflater = LayoutInflater.from(mActivity);
            sectionHeaderView =
                    (SectionHeaderView) inflater.inflate(R.layout.ss_feed_header, null, false);
        }
        return new FeedSurfaceCoordinator(mActivity, null, null, null, sectionHeaderView,
                exploreSurfaceActionHandler, isInNightMode, this);
        // TODO(crbug.com/982018): Customize surface background for incognito and dark mode.
        // TODO(crbug.com/982018): Hide signin promo UI in incognito mode.
    }
}
