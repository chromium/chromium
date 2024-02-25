// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

import androidx.annotation.NonNull;

import org.chromium.build.annotations.CheckDiscard;

import java.util.HashSet;
import java.util.List;
import java.util.Locale;

@CheckDiscard("Preference key checking should only happen on build with asserts")
public class PreferenceKeyRegistry {
    private final String mModule;
    public final HashSet<String> mKeysInUse;
    public final HashSet<String> mLegacyFormatKeys;
    public final List<KeyPrefix> mLegacyPrefixes;

    public PreferenceKeyRegistry(
            String module,
            List<String> keysInUse,
            List<String> legacyKeys,
            List<KeyPrefix> legacyPrefixes) {
        mModule = module;
        mKeysInUse = new HashSet<>(keysInUse);
        mLegacyFormatKeys = new HashSet<>(legacyKeys);
        mLegacyPrefixes = legacyPrefixes;
    }

    @NonNull
    public String toDebugString() {
        return String.format(
                Locale.getDefault(),
                "%s (%d in use, %d legacy, %d legacy prefixes)",
                mModule,
                mKeysInUse.size(),
                mLegacyFormatKeys.size(),
                mLegacyPrefixes.size());
    }
}
