// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.support.annotation.Nullable;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.ui.modelutil.PropertyModel;

/** The dummy coordinator when feed is not enabled ('src/components/feed/features.gni'). */
class ExploreSurfaceCoordinator {
    ExploreSurfaceCoordinator(ChromeActivity activity, ViewGroup parentView,
            @Nullable View headerView, PropertyModel containerPropertyModel) {}

    /**
     * Gets the {@link FeedSurfaceCreator}.
     * @return the {@link FeedSurfaceCreator}.
     */
    FeedSurfaceCreator getFeedSurfaceCreator() {
        return null;
    }

    /** Interface to create {@link FeedSurfaceCoordinator} */
    interface FeedSurfaceCreator {
        /**
         * Creates the {@link FeedSurfaceCoordinator} for the specified mode.
         * @return The {@link FeedSurfaceCoordinator}.
         */
        FeedSurfaceCoordinator createFeedSurfaceCoordinator();
    }
}