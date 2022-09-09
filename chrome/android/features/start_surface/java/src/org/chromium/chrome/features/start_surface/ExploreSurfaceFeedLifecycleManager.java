// Copyright 2019 The Chromium Authors
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
    /**
     * The constructor.
     * @param activity The activity the {@link FeedSurfaceCoordinator} associates with.
     * @param coordinator The coordinator for which this manages the feed lifecycle of.
     */
    ExploreSurfaceFeedLifecycleManager(Activity activity, FeedSurfaceCoordinator coordinator) {
        super(activity, coordinator);
        start();
    }

    @Override
    protected boolean canShow() {
        return super.canShow() && shouldShowFeed();
    }

    private boolean shouldShowFeed() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile())
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
