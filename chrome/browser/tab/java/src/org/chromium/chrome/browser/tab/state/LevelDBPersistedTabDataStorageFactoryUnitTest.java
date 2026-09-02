// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.content_public.browser.BrowserContextHandle;

/** Unit tests for {@link LevelDBPersistedTabDataStorageFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LevelDBPersistedTabDataStorageFactoryUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile1;
    @Mock private Profile mProfile2;
    @Mock private LevelDBPersistedDataStorage.Natives mLevelDBPersistedTabDataStorage;

    @Before
    public void setUp() {
        LevelDBPersistedDataStorageJni.setInstanceForTesting(mLevelDBPersistedTabDataStorage);
        doNothing()
                .when(mLevelDBPersistedTabDataStorage)
                .init(any(LevelDBPersistedDataStorage.class), any(BrowserContextHandle.class));
        doNothing().when(mLevelDBPersistedTabDataStorage).destroy(anyLong());
        doReturn(false).when(mProfile1).isOffTheRecord();
        doReturn(false).when(mProfile2).isOffTheRecord();
        LevelDBPersistedDataStorage.setSkipNativeAssertionsForTesting(true);
    }

    @SmallTest
    @Test
    public void testFactoryMethod() {
        LevelDBPersistedTabDataStorageFactory factory = new LevelDBPersistedTabDataStorageFactory();
        ProfileManager.setLastUsedProfileForTesting(mProfile1);
        LevelDBPersistedTabDataStorage profile1Storage = factory.create();
        ProfileManager.setLastUsedProfileForTesting(mProfile2);
        LevelDBPersistedTabDataStorage profile2Storage = factory.create();
        ProfileManager.setLastUsedProfileForTesting(mProfile1);
        LevelDBPersistedTabDataStorage profile1StorageAgain = factory.create();
        Assert.assertEquals(profile1Storage, profile1StorageAgain);
        Assert.assertNotEquals(profile1Storage, profile2Storage);
    }

    @SmallTest
    @Test
    public void testStorageDestroyedWhenProfileDestroyed() {
        LevelDBPersistedTabDataStorageFactory factory = new LevelDBPersistedTabDataStorageFactory();
        ProfileManager.setLastUsedProfileForTesting(mProfile1);
        LevelDBPersistedTabDataStorage storage = factory.create();
        ProfileManager.onProfileDestroyed(mProfile1);
        Assert.assertTrue(storage.isDestroyed());
    }
}
