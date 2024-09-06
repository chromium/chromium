// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.backup;

import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.internal.SyncPrefNames;

import java.nio.ByteBuffer;
import java.util.Set;

/** PrefBackupSerializer responsible for serializing/deserializing native integer prefs. */
class IntPrefBackupSerializer extends PrefBackupSerializer {
    public IntPrefBackupSerializer() {
        super(/* uniqueEncodingPrefix= */ "NativeIntegerPref.");
    }

    @Override
    public Set<String> getAllowlistedPrefs() {
        return Set.of(SyncPrefNames.SYNC_TO_SIGNIN_MIGRATION_STATE);
    }

    @Override
    protected byte[] serializePrefValueAsBytes(PrefService prefService, String prefName) {
        ByteBuffer buffer = ByteBuffer.allocate(Integer.BYTES);
        buffer.putInt(prefService.getInteger(prefName));
        return buffer.array();
    }

    @Override
    protected void setPrefValueFromSerializedBytes(
            PrefService prefService, String prefName, byte[] bytes) {
        prefService.setInteger(prefName, ByteBuffer.wrap(bytes).getInt());
    }
}
