// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.android_share_sheet;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;

/** Controller for extra action - sharing as tab group. */
@NullMarked
public interface TabGroupSharingController {
    /** Returns true if the share as tab group action is available for the tab. */
    boolean isAvailableForTab(Tab tab);

    /**
     * Shares the tab as a tab group.
     *
     * @param activity The current tabbed activity.
     * @param chromeOptionShareCallback The options callback for access to the share delegate.
     * @param tab The tab to be shared as a tab group.
     */
    void shareAsTabGroup(
            Activity activity, ChromeOptionShareCallback chromeOptionShareCallback, Tab tab);
}
