// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Records UMA stats for the actions that the user takes on the feed in the NTP.
 */
public class FeedUma {
    // Possible actions taken by the user to control the feed. These values are also defined in
    // enums.xml as FeedControlsActions.
    // WARNING: these values must stay in sync with enums.xml.
    public static final int CONTROLS_ACTION_CLICKED_MY_ACTIVITY = 0;
    public static final int CONTROLS_ACTION_CLICKED_MANAGE_INTERESTS = 1;
    public static final int CONTROLS_ACTION_CLICKED_LEARN_MORE = 2;
    public static final int CONTROLS_ACTION_TOGGLED_FEED = 3;
    public static final int CONTROLS_ACTION_CLICKED_FEED_HEADER_MENU = 4;
    public static final int CONTROLS_ACTION_CLICKED_MANAGE_AUTOPLAY = 5;
    public static final int NUM_CONTROLS_ACTIONS = 6;

    public static void recordFeedControlsAction(int action) {
        assert action >= 0;
        assert action < NUM_CONTROLS_ACTIONS;
        RecordHistogram.recordEnumeratedHistogram(
                "ContentSuggestions.Feed.Controls.Actions", action, NUM_CONTROLS_ACTIONS);
    }
}
