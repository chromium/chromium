// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for defining features and params related to tab group sync. */
public final class TabGroupSyncFeatures {

    /** Whether tab group sync is enabled. */
    @CalledByNative
    public static boolean isTabGroupSyncEnabled() {
        return ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID);
    }
}
