// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.profiles.ProfileManager;

/** Creates {@link LevelDBPersistedTabDataStorage} instances per profile */
@NullMarked
public class LevelDBPersistedTabDataStorageFactory
        implements PersistedTabDataStorageFactory<LevelDBPersistedTabDataStorage> {
    private static final ProfileKeyedMap<LevelDBPersistedTabDataStorage>
            sProfileToLevelDBStorageMap = ProfileKeyedMap.createMapOfDestroyables();

    @Override
    public LevelDBPersistedTabDataStorage create() {
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        return sProfileToLevelDBStorageMap.getForProfile(
                profile, LevelDBPersistedTabDataStorage::new);
    }
}
