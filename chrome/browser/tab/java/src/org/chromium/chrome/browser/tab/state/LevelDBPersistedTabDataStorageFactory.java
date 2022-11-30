// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

import java.util.HashMap;
import java.util.Map;

/**
 * Creates {@link LevelDBPersistedTabDataStorage} instances per profile
 */
public class LevelDBPersistedTabDataStorageFactory
        implements PersistedTabDataStorageFactory<LevelDBPersistedTabDataStorage> {
    public static final Map<Profile, LevelDBPersistedTabDataStorage> sProfileToLevelDBStorageMap =
            new HashMap<>();
    private static ProfileManager.Observer sProfileManagerObserver;

    LevelDBPersistedTabDataStorageFactory() {
        if (sProfileManagerObserver == null) {
            sProfileManagerObserver = new ProfileManager.Observer() {
                @Override
                public void onProfileAdded(Profile profile) {}

                @Override
                public void onProfileDestroyed(Profile destroyedProfile) {
                    LevelDBPersistedTabDataStorage storageToDestroy =
                            sProfileToLevelDBStorageMap.get(destroyedProfile);
                    if (storageToDestroy != null) {
                        storageToDestroy.destroy();
                        sProfileToLevelDBStorageMap.remove(destroyedProfile);
                    }

                    if (sProfileToLevelDBStorageMap.isEmpty()) {
                        ProfileManager.removeObserver(sProfileManagerObserver);
                        sProfileManagerObserver = null;
                    }
                }
            };
            ProfileManager.addObserver(sProfileManagerObserver);
        }
    }

    @Override
    public LevelDBPersistedTabDataStorage create() {
        Profile profile = Profile.getLastUsedRegularProfile();
        LevelDBPersistedTabDataStorage storage = sProfileToLevelDBStorageMap.get(profile);
        if (storage == null) {
            storage = new LevelDBPersistedTabDataStorage(profile);
            sProfileToLevelDBStorageMap.put(profile, storage);
        }
        return storage;
    }
}
