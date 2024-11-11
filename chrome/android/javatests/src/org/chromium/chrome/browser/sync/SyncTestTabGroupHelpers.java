// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Utility class for tab group functionalities in native Sync browser tests. */
@JNINamespace("sync_test_utils_android")
final class SyncTestTabGroupHelpers {

    /** Creates a tab group with a single tab and given visual data. */
    @CalledByNative
    private static Token createGroupFromTab(Tab tab, String title, @TabGroupColorId int color) {
        TabGroupModelFilter tabGroupModelFilter = TabModelUtils.getTabGroupModelFilterByTab(tab);
        tabGroupModelFilter.createSingleTabGroup(tab, false);

        int rootId = tabGroupModelFilter.getRootIdFromStableId(tab.getTabGroupId());
        tabGroupModelFilter.setTabGroupColor(rootId, color);
        tabGroupModelFilter.setTabGroupTitle(rootId, title);

        return tab.getTabGroupId();
    }
}
