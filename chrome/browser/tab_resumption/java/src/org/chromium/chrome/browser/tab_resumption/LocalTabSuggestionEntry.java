// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.chrome.browser.tab.Tab;

/** A single Local Tab suggestion entry in the tab resumption module. */
public class LocalTabSuggestionEntry extends SuggestionEntry {
    public final Tab tab;

    LocalTabSuggestionEntry(Tab tab) {
        super(
                /* sourceName= */ "",
                /* url= */ tab.getUrl(),
                /* title= */ tab.getTitle(),
                /* lastActiveTime= */ tab.getTimestampMillis(),
                /* id= */ tab.getId());
        this.tab = tab;
    }

    // No need to override compareTo().
}
