// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.tab_group_sync.messaging.PersistentMessage;

public class TabLabelUtils {
    /** No instantiation. */
    private TabLabelUtils() {}

    public static int extractTabId(@Nullable PersistentMessage message) {
        return message == null
                        || message.attribution == null
                        || message.attribution.tabMetadata == null
                ? Tab.INVALID_TAB_ID
                : message.attribution.tabMetadata.localTabId;
    }

    public static @Nullable Token extractTabGroupId(@Nullable PersistentMessage message) {
        return message == null
                        || message.attribution == null
                        || message.attribution.tabGroupMetadata == null
                        || message.attribution.tabGroupMetadata.localTabGroupId == null
                ? null
                : message.attribution.tabGroupMetadata.localTabGroupId.tabGroupId;
    }
}
