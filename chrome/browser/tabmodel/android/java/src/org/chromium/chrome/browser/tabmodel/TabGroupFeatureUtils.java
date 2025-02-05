// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Feature related utilities for tab groups. */
public class TabGroupFeatureUtils {
    /** Returns whether the group creation dialog will be skipped based on current flags. */
    public static boolean shouldSkipGroupCreationDialog() {
        return !ChromeFeatureList.sTabGroupCreationDialogAndroid.isEnabled();
    }

    /** All statics. */
    private TabGroupFeatureUtils() {}
}
