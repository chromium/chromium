// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.TabId;

/** Returns key event data relevant to the current tab with Android's View focus. */
@NullMarked
public class TabKeyEventData {
    public final @TabId int tabId;
    public final int keyCode;

    /**
     * @param tabId The ID of the tab the event is for.
     * @param keyCode What the keycode the event is for.
     */
    public TabKeyEventData(@TabId int tabId, int keyCode) {
        this.tabId = tabId;
        this.keyCode = keyCode;
    }
}
