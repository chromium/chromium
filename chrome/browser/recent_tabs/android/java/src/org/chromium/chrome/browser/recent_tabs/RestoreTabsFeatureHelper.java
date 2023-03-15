// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * An interface of helper methods that assist in the restore tabs workflow.
 */
public interface RestoreTabsFeatureHelper {
    /**
     * Configure data for feature engagement on first run.
     *
     * @param profile The current user profile.
     */
    public void configureOnFirstRun(Profile profile);
}