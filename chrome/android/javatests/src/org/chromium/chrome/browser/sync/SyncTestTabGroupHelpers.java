// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.Nullable;

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
    private static Token createGroupFromTab(Tab tab) {
        TabGroupModelFilter tabGroupModelFilter = TabModelUtils.getTabGroupModelFilterByTab(tab);
        tabGroupModelFilter.createSingleTabGroup(tab, false);

        return tab.getTabGroupId();
    }

    /** Returns the local tab group ID if the `tab` is in a group. */
    @CalledByNative
    private static @Nullable Token getGroupIdForTab(Tab tab) {
        return tab.getTabGroupId();
    }

    /** Updates title and color of the group of the given tab. */
    @CalledByNative
    private static void updateGroupVisualData(
            Tab tabInGroup, String title, @TabGroupColorId int color) {
        TabGroupModelFilter tabGroupModelFilter =
                TabModelUtils.getTabGroupModelFilterByTab(tabInGroup);
        int rootId = tabGroupModelFilter.getRootIdFromStableId(tabInGroup.getTabGroupId());
        tabGroupModelFilter.setTabGroupColor(rootId, color);
        tabGroupModelFilter.setTabGroupTitle(rootId, title);
    }
}
