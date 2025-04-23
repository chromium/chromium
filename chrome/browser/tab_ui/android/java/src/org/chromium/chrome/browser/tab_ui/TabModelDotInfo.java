// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import org.chromium.build.annotations.NullMarked;

/**
 * Simple data class that holds information to show or not show a notification dot for the whole tab
 * model.
 */
@NullMarked
public class TabModelDotInfo {
    public static final TabModelDotInfo HIDE = new TabModelDotInfo(false, "");

    public final boolean showDot;

    // When showDot is true, this field will hold the title of one of the tab groups that
    // warranted the dot to show.
    public final String tabGroupTitle;

    public TabModelDotInfo(boolean showDot, String tabGroupTitle) {
        this.showDot = showDot;
        this.tabGroupTitle = tabGroupTitle;
    }
}
