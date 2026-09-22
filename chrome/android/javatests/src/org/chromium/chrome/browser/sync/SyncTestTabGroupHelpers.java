// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.Token;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Utility class for tab group functionalities in native Sync browser tests. */
@JNINamespace("sync_test_utils_android")
final class SyncTestTabGroupHelpers {

    /** Creates a tab group with a single tab and given visual data. */
    @CalledByNative
    private static @JniType("base::Token") Token createGroupFromTab(
            @JniType("TabAndroid*") Tab tab) {
        TabModel tabModel = TabModelUtils.getTabModelByTab(tab);
        tabModel.createSingleTabGroup(tab);

        return tab.getTabGroupId();
    }

    /** Returns the local tab group ID if the `tab` is in a group. */
    @CalledByNative
    private static @JniType("std::optional<base::Token>") @Nullable Token getGroupIdForTab(
            @JniType("TabAndroid*") Tab tab) {
        return tab.getTabGroupId();
    }

    /** Updates title and color of the group of the given tab. */
    @CalledByNative
    private static void updateGroupVisualData(
            @JniType("TabAndroid*") Tab tabInGroup,
            @JniType("std::string") String title,
            @TabGroupColorId int color) {
        TabModel tabModel = TabModelUtils.getTabModelByTab(tabInGroup);
        Token tabGroupId = tabInGroup.getTabGroupId();
        tabModel.setTabGroupColor(tabGroupId, color);
        tabModel.setTabGroupTitle(tabGroupId, title);
    }
}
