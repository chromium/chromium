// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Holds objects related to a tab and where it lives. */
@NullMarked
public class TabWindowInfo {
    public final @WindowId int windowId;
    public final TabModelSelector tabModelSelector;
    public final TabModel tabModel;
    public final Tab tab;

    public TabWindowInfo(
            @WindowId int windowId, TabModelSelector tabModelSelector, TabModel tabModel, Tab tab) {
        this.tab = tab;
        this.tabModel = tabModel;
        this.tabModelSelector = tabModelSelector;
        this.windowId = windowId;
    }
}
