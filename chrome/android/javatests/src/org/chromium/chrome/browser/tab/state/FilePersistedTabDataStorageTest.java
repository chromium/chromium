// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.state.FilePersistedTabDataStorage.FileSaveRequest;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.io.File;
import java.util.concurrent.Semaphore;

/**
 * Tests relating to  {@link FilePersistedTabDataStorage}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class FilePersistedTabDataStorageTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final int TAB_ID_1 = 1;
    private static final String DATA_ID_1 = "DataId1";
    private static final int TAB_ID_2 = 2;
    private static final String DATA_ID_2 = "DataId2";

    private static final byte[] DATA_A = {13, 14};
    private static final byte[] DATA_B = {9, 10};

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
    }

    @SmallTest
    @Test
    public void testFilePersistedDataStorageNonEncrypted() throws InterruptedException {
        testFilePersistedDataStorage(new FilePersistedTabDataStorage());
    }

    @SmallTest
    @Test
    public void testFilePersistedDataStorageEncrypted() throws InterruptedException {
        testFilePersistedDataStorage(new EncryptedFilePersistedTabDataStorage());
    }

    @SmallTest
    @Test
    public void testUnsavedKeys() throws InterruptedException {
        FilePersistedTabDataStorage persistedTabDataStorage =
                new EncryptedFilePersistedTabDataStorage();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            persistedTabDataStorage.save(
                    TAB_ID_1, DATA_ID_1, () -> { return DATA_A; }, semaphore::release);
        });
        semaphore.acquire();
        // Simulate closing the app without saving the keys and reopening
        CipherFactory.resetInstanceForTesting();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            persistedTabDataStorage.restore(TAB_ID_1, DATA_ID_1, (res) -> {
                Assert.assertNull(res);
                semaphore.release();
            });
        });
        semaphore.acquire();
    }

    private void testFilePersistedDataStorage(FilePersistedTabDataStorage persistedTabDataStorage)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            persistedTabDataStorage.save(
                    TAB_ID_1, DATA_ID_1, () -> { return DATA_A; }, semaphore::release);
        });
        semaphore.acquire();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            persistedTabDataStorage.restore(TAB_ID_1, DATA_ID_1, (res) -> {
                Assert.assertEquals(res.length, 2);
                Assert.assertArrayEquals(res, DATA_A);
                semaphore.release();
            });
        });
        semaphore.acquire();

        File file = FilePersistedTabDataStorage.getFile(TAB_ID_1, DATA_ID_1);
        Assert.assertTrue(file.exists());

        ThreadUtils.runOnUiThreadBlocking(
                () -> { persistedTabDataStorage.delete(TAB_ID_1, DATA_ID_1, semaphore::release); });
        semaphore.acquire();
        Assert.assertFalse(file.exists());
    }

    @Test
    @SmallTest
    public void testRedundantSaveDropped() throws InterruptedException {
        FilePersistedTabDataStorage storage = new FilePersistedTabDataStorage();
        final Semaphore semaphore = new Semaphore(0);
        storage.addSaveRequest(storage.new FileSaveRequest(TAB_ID_1, DATA_ID_1,
                ()
                        -> { return DATA_A; },
                (res) -> {
                    Assert.fail(
                            "First request should not have been executed as there is a subsequent "
                            + "request in the queue with the same Tab ID/Data ID combination");
                }));
        storage.addSaveRequest(storage.new FileSaveRequest(
                TAB_ID_2, DATA_ID_2, () -> { return DATA_A; }, semaphore::release));
        storage.addSaveRequest(storage.new FileSaveRequest(
                TAB_ID_1, DATA_ID_1, () -> { return DATA_B; }, semaphore::release));
        ThreadUtils.runOnUiThreadBlocking(() -> {
            storage.processNextItemOnQueue();
            storage.processNextItemOnQueue();
        });
        semaphore.acquire();
        ThreadUtils.runOnUiThreadBlocking(() -> { storage.processNextItemOnQueue(); });
        semaphore.acquire();
        Assert.assertTrue(storage.mQueue.isEmpty());
    }
}
