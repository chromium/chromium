// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.backup;

import android.util.Pair;

import org.chromium.components.prefs.PrefService;

import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Base class for translating entries from the PrefService to the format used by Android backups,
 * and vice-versa. Each derived class is responsible for a certain type (bool, dictionary, etc).
 */
abstract class PrefBackupSerializer {
    private final String mUniqueEncodingPrefix;

    /**
     * @param uniqueEncodingPrefix Used as part of the serialization process. Each implementation
     *     must provide a different value.
     */
    public PrefBackupSerializer(String uniqueEncodingPrefix) {
        mUniqueEncodingPrefix = uniqueEncodingPrefix;
    }

    /**
     * Reads the getAllowlistedPrefs() from prefService and returns the serialized name/value pairs.
     *
     * @param prefService The pref service to read from.
     * @return The serialized pref (name, value) pairs.
     */
    public List<Pair<String, byte[]>> serializeAllowlistedPrefs(PrefService prefService) {
        return getAllowlistedPrefs().stream()
                .map(
                        prefName ->
                                new Pair<>(
                                        mUniqueEncodingPrefix + prefName,
                                        serializePrefValueAsBytes(prefService, prefName)))
                .collect(Collectors.toList());
    }

    /**
     * Tries to decode serializedPrefName and serializedPrefValue and writes them to prefService if
     * successful.
     *
     * @param prefService The pref service to write to.
     * @param serializedPrefName The serialized pref name.
     * @param serializedPrefValue The serialized pref value.
     * @return Whether deserialization was successful.
     */
    public boolean tryDeserialize(
            PrefService prefService, String serializedPrefName, byte[] serializedPrefValue) {
        if (!serializedPrefName.startsWith(mUniqueEncodingPrefix)) {
            return false;
        }

        String prefName = serializedPrefName.substring(mUniqueEncodingPrefix.length());
        if (!getAllowlistedPrefs().contains(prefName)) {
            return false;
        }

        setPrefValueFromSerializedBytes(prefService, prefName, serializedPrefValue);
        return true;
    }

    /**
     * The prefs to back up & restore. Allowlisting prevents writing prefs that were removed from
     * the code since the backup, which causes PrefService crashes.
     */
    public abstract Set<String> getAllowlistedPrefs();

    /**
     * The type-specific implementation for querying prefService and serializing the value.
     *
     * @param prefService The pref service to read from.
     * @param prefName The pref to read.
     * @return A serialized version of the pref value.
     */
    protected abstract byte[] serializePrefValueAsBytes(PrefService prefService, String prefName);

    /**
     * The type-specific implementation for deserializing a value and writing it to prefService.
     *
     * @param prefService The pref service to write to.
     * @param prefName The pref to write.
     * @param bytes The value to deserialize and write.
     */
    protected abstract void setPrefValueFromSerializedBytes(
            PrefService prefService, String prefName, byte[] bytes);
}
