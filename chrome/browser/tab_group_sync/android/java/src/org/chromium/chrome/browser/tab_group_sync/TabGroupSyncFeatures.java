// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

/** Utility class for defining features and params related to tab group sync. */
public final class TabGroupSyncFeatures {

    /** Whether tab group sync is enabled. */
    public static boolean isTabGroupSyncEnabled() {
        // TODO(b/336385437): Disabling the feature to do a refactor. We can't have the feature
        // running during the transition since it will break or crash.
        // return ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID);
        return false;
    }
}
