// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;

import com.google.android.libraries.feed.api.client.stream.Stream;

import org.chromium.chrome.browser.feed.StreamLifecycleManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

/** Explore surface feed stream lifecycle manager. */
class ExploreSurfaceStreamLifecycleManager extends StreamLifecycleManager {
    private final boolean mHasHeader;
    /**
     * The constructor.
     * @param stream The {@link Stream} this manager manages.
     * @param activity The activity the {@link Stream} associates with.
     */
    ExploreSurfaceStreamLifecycleManager(Stream stream, Activity activity, boolean hasHeader) {
        super(stream, activity);
        mHasHeader = hasHeader;
        start();
    }

    @Override
    protected boolean canShow() {
        return super.canShow() && shouldShowFeed();
    }

    @Override
    protected boolean canActivate() {
        return super.canShow() && shouldShowFeed();
    }

    private boolean shouldShowFeed() {
        // If there is a header to opt out from article suggestions, we don't call
        // Stream#onShow to prevent feed services from being warmed up if the user
        // has opted out during the previous session.
        return (!mHasHeader
                || PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE));
    }
    // TODO(crbug.com/982018): Save and restore instance state when opening the feeds in normal
    // Tabs.
}
