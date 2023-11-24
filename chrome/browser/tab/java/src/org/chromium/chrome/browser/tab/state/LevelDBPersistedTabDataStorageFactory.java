// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Creates {@link LevelDBPersistedTabDataStorage} instances per profile */
public class LevelDBPersistedTabDataStorageFactory
        implements PersistedTabDataStorageFactory<LevelDBPersistedTabDataStorage> {
    private static ProfileKeyedMap<LevelDBPersistedTabDataStorage> sProfileToLevelDBStorageMap;

    LevelDBPersistedTabDataStorageFactory() {
        if (sProfileToLevelDBStorageMap == null) {
            sProfileToLevelDBStorageMap = ProfileKeyedMap.createMapOfDestroyables();
        }
    }

    @Override
    public LevelDBPersistedTabDataStorage create() {
        Profile profile = Profile.getLastUsedRegularProfile();
        return sProfileToLevelDBStorageMap.getForProfile(
                profile, () -> new LevelDBPersistedTabDataStorage(profile));
    }
}
