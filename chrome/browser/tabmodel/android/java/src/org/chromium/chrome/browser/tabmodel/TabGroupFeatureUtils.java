// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Feature related utilities for tab groups. */
public class TabGroupFeatureUtils {
    /**
     * Returns whether the group creation dialog will be skipped based on current flags.
     *
     * @param shouldShow Whether the creation dialog should show if TabGroupCreationDialogAndroid is
     *     enabled. Currently it should only show for drag and drop merge and bulk selection editor
     *     merge. It should not show for context menu group creations.
     */
    public static boolean shouldSkipGroupCreationDialog(boolean shouldShow) {
        if (ChromeFeatureList.sTabGroupCreationDialogAndroid.isEnabled()) {
            return !shouldShow;
        } else {
            return true;
        }
    }

    /** All statics. */
    private TabGroupFeatureUtils() {}
}
