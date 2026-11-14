// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.nio.ByteBuffer;

/** Unit tests for {@link LevelDBPersistedTabDataStorageFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LevelDBPersistedTabDataStorageFactoryUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile1;
    @Mock private Profile mProfile2;
    @Mock private Profile mOtrProfile1;
    @Mock private LevelDBPersistedDataStorage.Natives mLevelDBPersistedTabDataStorage;

    @Before
    public void setUp() {
        LevelDBPersistedDataStorageJni.setInstanceForTesting(mLevelDBPersistedTabDataStorage);
        when(mLevelDBPersistedTabDataStorage.init(any(BrowserContextHandle.class))).thenReturn(1L);
        doNothing().when(mLevelDBPersistedTabDataStorage).destroy(anyLong());
        doReturn(false).when(mProfile1).isOffTheRecord();
        doReturn(mProfile1).when(mProfile1).getOriginalProfile();
        doReturn(false).when(mProfile2).isOffTheRecord();
        doReturn(mProfile2).when(mProfile2).getOriginalProfile();
        doReturn(true).when(mOtrProfile1).isOffTheRecord();
        doReturn(mProfile1).when(mOtrProfile1).getOriginalProfile();
    }

    @Test
    public void testFactoryMethod() {
        LevelDBPersistedTabDataStorageFactory factory = new LevelDBPersistedTabDataStorageFactory();
        LevelDBPersistedTabDataStorage profile1Storage = factory.create(mProfile1);
        LevelDBPersistedTabDataStorage profile2Storage = factory.create(mProfile2);
        LevelDBPersistedTabDataStorage profile1StorageAgain = factory.create(mProfile1);
        LevelDBPersistedTabDataStorage otrProfile1Storage = factory.create(mOtrProfile1);
        Assert.assertEquals(profile1Storage, profile1StorageAgain);
        Assert.assertEquals(profile1Storage, otrProfile1Storage);
        Assert.assertNotEquals(profile1Storage, profile2Storage);
    }

    @Test
    public void testStorageDestroyedWhenProfileDestroyed() {
        LevelDBPersistedTabDataStorageFactory factory = new LevelDBPersistedTabDataStorageFactory();
        LevelDBPersistedTabDataStorage storage = factory.create(mProfile1);
        ProfileManager.onProfileDestroyed(mProfile1);
        Assert.assertTrue(storage.isDestroyed());
    }

    @Test
    public void testSaveByteBufferConversion() {
        LevelDBPersistedTabDataStorageFactory factory = new LevelDBPersistedTabDataStorageFactory();
        LevelDBPersistedTabDataStorage storage = factory.create(mProfile1);

        byte[] full = new byte[] {1, 2, 3, 4};
        ArgumentCaptor<byte[]> captor = ArgumentCaptor.forClass(byte[].class);

        // Full zero-offset heap array reuses the backing byte[] without copying.
        storage.save(1, "full", () -> ByteBuffer.wrap(full));
        verify(mLevelDBPersistedTabDataStorage)
                .save(eq(1L), eq("_1-full"), captor.capture(), isNull());
        Assert.assertSame(full, captor.getValue());

        // Offset/length sub-range copies only [position, limit).
        storage.save(1, "subrange", () -> ByteBuffer.wrap(full, 1, 2));
        verify(mLevelDBPersistedTabDataStorage)
                .save(eq(1L), eq("_1-subrange"), captor.capture(), isNull());
        Assert.assertArrayEquals(new byte[] {2, 3}, captor.getValue());

        // Non-zero arrayOffset slice copies only the slice bytes.
        storage.save(1, "slice", () -> ByteBuffer.wrap(full, 1, 2).slice());
        verify(mLevelDBPersistedTabDataStorage)
                .save(eq(1L), eq("_1-slice"), captor.capture(), isNull());
        Assert.assertArrayEquals(new byte[] {2, 3}, captor.getValue());

        // Non-array-backed buffer falls back to copying remaining bytes.
        storage.save(1, "readonly", () -> ByteBuffer.wrap(full).asReadOnlyBuffer());
        verify(mLevelDBPersistedTabDataStorage)
                .save(eq(1L), eq("_1-readonly"), captor.capture(), isNull());
        Assert.assertNotSame(full, captor.getValue());
        Assert.assertArrayEquals(full, captor.getValue());
    }
}
