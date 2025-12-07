// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.backup;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.internal.SyncPrefNames;

import java.util.Set;

/** PrefBackupSerializer responsible for serializing/deserializing native dictionary prefs. */
@NullMarked
class DictPrefBackupSerializer extends PrefBackupSerializer {
    public DictPrefBackupSerializer() {
        super(/* uniqueEncodingPrefix= */ "NativeJsonDict.");
    }

    @Override
    public Set<String> getAllowlistedPrefs() {
        return Set.of(SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT);
    }

    @Override
    protected byte[] serializePrefValueAsBytes(PrefService prefService, String prefName) {
        return DictPrefBackupSerializerJni.get()
                .getSerializedDict(prefService, prefName)
                .getBytes();
    }

    @Override
    protected void setPrefValueFromSerializedBytes(
            PrefService prefService, String prefName, byte[] bytes) {
        DictPrefBackupSerializerJni.get().setDict(prefService, prefName, new String(bytes));
    }

    @NativeMethods
    interface Natives {
        // Returns a serialized version of PrefService::GetDict(), which can be stored in backups.
        @JniType("std::string")
        String getSerializedDict(
                @JniType("PrefService*") PrefService prefService,
                @JniType("std::string") String prefName);

        // If `serializedDict` was obtained from `getSerializedDict(prefService, prefName)`,
        // deserializes and passes the result to PrefService::SetDict(). If deserialization fails,
        // does nothing.
        void setDict(
                @JniType("PrefService*") PrefService prefService,
                @JniType("std::string") String prefName,
                @JniType("std::string") String serializedDict);
    }
}
