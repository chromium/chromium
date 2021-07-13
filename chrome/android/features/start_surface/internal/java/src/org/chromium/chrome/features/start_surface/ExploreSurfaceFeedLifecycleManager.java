// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.FeedSurfaceLifecycleManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;

/** Explore surface feed lifecycle manager. */
class ExploreSurfaceFeedLifecycleManager extends FeedSurfaceLifecycleManager {
    private final boolean mHasHeader;
    /**
     * The constructor.
     * @param activity The activity the {@link FeedSurfaceCoordinator} associates with.
     * @param hasHeader Whether the feed has a header to work with.
     * @param coordinator The coordinator for which this manages the feed lifecycle of.
     */
    ExploreSurfaceFeedLifecycleManager(
            Activity activity, boolean hasHeader, FeedSurfaceCoordinator coordinator) {
        super(activity, coordinator);
        mHasHeader = hasHeader;
        start();
    }

    @Override
    protected boolean canShow() {
        return super.canShow() && shouldShowFeed();
    }

    private boolean shouldShowFeed() {
        // If there is a header to opt out from article suggestions, we don't call
        // FeedSurfaceCoordinator#onSurfaceOpened to prevent feed services from being warmed up if
        // the user has opted out during the previous session.
        return !mHasHeader
                || UserPrefs.get(Profile.getLastUsedRegularProfile())
                           .getBoolean(Pref.ARTICLES_LIST_VISIBLE);
    }

    @Nullable
    @Override
    protected String restoreInstanceState() {
        return StartSurfaceUserData.getInstance().restoreFeedInstanceState();
    }

    @Override
    protected void saveInstanceState() {
        String state = mCoordinator.getSavedInstanceStateString();
        StartSurfaceUserData.getInstance().saveFeedInstanceState(state);
    }
}