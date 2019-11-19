// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the start surface properties. */
class StartSurfaceProperties {
    public static interface BottomBarClickListener {
        /**
         * Called when clicks on the home button.
         * */
        void onHomeButtonClicked();

        /**
         * Called when clicks on the explore button.
         * */
        void onExploreButtonClicked();
    }
    public static final PropertyModel
            .WritableObjectPropertyKey<BottomBarClickListener> BOTTOM_BAR_CLICKLISTENER =
            new PropertyModel.WritableObjectPropertyKey<BottomBarClickListener>();
    public static final PropertyModel.WritableIntPropertyKey BOTTOM_BAR_HEIGHT =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey BOTTOM_BAR_SELECTED_TAB_POSITION =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_BOTTOM_BAR_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_EXPLORE_SURFACE_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_SECONDARY_SURFACE_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_SHOWING_OVERVIEW =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<FeedSurfaceCoordinator> FEED_SURFACE_COORDINATOR =
            new PropertyModel.WritableObjectPropertyKey<FeedSurfaceCoordinator>();
    public static final PropertyModel.WritableIntPropertyKey TOP_BAR_HEIGHT =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {BOTTOM_BAR_CLICKLISTENER,
            BOTTOM_BAR_HEIGHT, BOTTOM_BAR_SELECTED_TAB_POSITION, IS_BOTTOM_BAR_VISIBLE,
            IS_EXPLORE_SURFACE_VISIBLE, IS_SECONDARY_SURFACE_VISIBLE, IS_SHOWING_OVERVIEW,
            FEED_SURFACE_COORDINATOR, TOP_BAR_HEIGHT};
}
