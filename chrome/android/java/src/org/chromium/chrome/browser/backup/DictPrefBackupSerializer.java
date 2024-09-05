// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.backup;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.sync.internal.SyncPrefNames;

import java.util.Set;

/** PrefBackupSerializer responsible for serializing/deserializing native dictionary prefs. */
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
        if (prefName.equals(SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT)
                && !SigninFeatureMap.isEnabled(
                        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP)) {
            // In this case the pref is not restored and this is not exposed to tryDeserialize()
            // but that doesn't affect the restore in any significant way and this code is going
            // away when the restore flag is cleaned up.
            return;
        }
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
