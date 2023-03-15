// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.EventConstants;

/**
 * This class provides helper methods for use during the restore tabs workflow.
 */
public class RestoreTabsFeatureHelperImpl implements RestoreTabsFeatureHelper {
    @Override
    public void configureOnFirstRun(Profile profile) {
        // Send a notifyEvent call to the feature engagement system to begin
        // tracking when to show promo for the restore tabs on first run flow.
        TrackerFactory.getTrackerForProfile(profile).notifyEvent(
                EventConstants.RESTORE_TABS_ON_FIRST_RUN_SHOW_PROMO);
    }
}