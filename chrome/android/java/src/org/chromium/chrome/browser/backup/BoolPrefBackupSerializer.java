// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.backup;

import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.internal.SyncPrefNames;

import java.util.Set;

/** PrefBackupSerializer responsible for serializing/deserializing native boolean prefs. */
class BoolPrefBackupSerializer extends PrefBackupSerializer {
    public BoolPrefBackupSerializer() {
        // Bools were the first backed up prefs, thus the generic string.
        super(/* uniqueEncodingPrefix= */ "native.");
    }

    @Override
    public Set<String> getAllowlistedPrefs() {
        return Set.of(
                SyncPrefNames.SYNC_KEEP_EVERYTHING_SYNCED,
                SyncPrefNames.SYNC_APPS,
                SyncPrefNames.SYNC_AUTOFILL,
                SyncPrefNames.SYNC_BOOKMARKS,
                SyncPrefNames.SYNC_HISTORY,
                SyncPrefNames.SYNC_PASSWORDS,
                SyncPrefNames.SYNC_PAYMENTS,
                SyncPrefNames.SYNC_PREFERENCES,
                SyncPrefNames.SYNC_PRODUCT_COMPARISON,
                SyncPrefNames.SYNC_READING_LIST,
                SyncPrefNames.SYNC_SAVED_TAB_GROUPS,
                SyncPrefNames.SYNC_SHARED_TAB_GROUP_DATA,
                SyncPrefNames.SYNC_TABS);
    }

    @Override
    protected byte[] serializePrefValueAsBytes(PrefService prefService, String prefName) {
        return prefService.getBoolean(prefName) ? new byte[] {1} : new byte[] {0};
    }

    @Override
    protected void setPrefValueFromSerializedBytes(
            PrefService prefService, String prefName, byte[] bytes) {
        prefService.setBoolean(prefName, bytes[0] != 0);
    }
}
