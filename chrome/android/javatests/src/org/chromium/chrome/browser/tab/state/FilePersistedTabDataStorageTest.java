// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.util.ByteBufferTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.Semaphore;

/**
 * Tests relating to  {@link FilePersistedTabDataStorage}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class FilePersistedTabDataStorageTest {
    private static final int TAB_ID_1 = 1;
    private static final String DATA_ID_1 = "DataId1";
    private static final int TAB_ID_2 = 2;
    private static final String DATA_ID_2 = "DataId2";
    private static final int TAB_ID_3 = 3;
    private static final String DATA_ID_3 = "DataId3";
    private static final int TAB_ID_4 = 4;
    private static final String DATA_ID_4 = "DataId4";

    private static final byte[] DATA_A = {13, 14};
    private static final byte[] DATA_B = {9, 10};

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        FilePersistedTabDataStorage.deleteFilesForTesting();
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
            persistedTabDataStorage.save(TAB_ID_1, DATA_ID_1,
                    () -> { return ByteBuffer.wrap(DATA_A); }, semaphore::release);
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
            persistedTabDataStorage.save(TAB_ID_1, DATA_ID_1,
                    () -> { return ByteBuffer.wrap(DATA_A); }, semaphore::release);
        });
        semaphore.acquire();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            persistedTabDataStorage.restore(TAB_ID_1, DATA_ID_1, (res) -> {
                ByteBufferTestUtils.verifyByteBuffer(DATA_A, res);
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
                        -> { return ByteBuffer.wrap(DATA_A); },
                (res) -> {
                    Assert.fail(
                            "First request should not have been executed as there is a subsequent "
                            + "request in the queue with the same Tab ID/Data ID combination");
                }));
        storage.addSaveRequest(storage.new FileSaveRequest(TAB_ID_2, DATA_ID_2,
                () -> { return ByteBuffer.wrap(DATA_A); }, semaphore::release));
        storage.addSaveRequest(storage.new FileSaveRequest(TAB_ID_1, DATA_ID_1,
                () -> { return ByteBuffer.wrap(DATA_B); }, semaphore::release));
        ThreadUtils.runOnUiThreadBlocking(() -> {
            storage.processNextItemOnQueue();
            storage.processNextItemOnQueue();
        });
        semaphore.acquire();
        ThreadUtils.runOnUiThreadBlocking(() -> { storage.processNextItemOnQueue(); });
        semaphore.acquire();
        Assert.assertTrue(storage.mQueue.isEmpty());
    }

    @Test
    @SmallTest
    public void testOutOfMemoryError() throws InterruptedException {
        // Ensure no data for Tab ID 1 / Data ID 1 (could be cross talk from other batch tests)
        File file = FilePersistedTabDataStorage.getFile(TAB_ID_1, DATA_ID_1);
        file.delete();
        Assert.assertFalse(file.exists());
        FilePersistedTabDataStorage storage = new FilePersistedTabDataStorage();
        final Semaphore semaphore = new Semaphore(0);
        storage.addSaveRequest(storage.new FileSaveRequest(TAB_ID_1, DATA_ID_1, () -> {
            // OutOfMemory error on ByteBuffer supplier.
            throw new OutOfMemoryError("OutOfMemoryError mock");
        }, semaphore::release));
        ThreadUtils.runOnUiThreadBlocking(() -> { storage.processNextItemOnQueue(); });
        semaphore.acquire();
        Assert.assertFalse(file.exists());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShutdown() {
        FilePersistedTabDataStorage storage =
                PersistedTabDataConfiguration.getFilePersistedTabDataStorage();

        Assert.assertFalse(FilePersistedTabDataStorage.getFile(TAB_ID_1, DATA_ID_1).exists());
        Assert.assertFalse(FilePersistedTabDataStorage.getFile(TAB_ID_2, DATA_ID_2).exists());

        storage.addSaveRequest(storage.new FileSaveRequest(
                TAB_ID_1, DATA_ID_1, () -> { return ByteBuffer.wrap(DATA_B); }, (res) -> {}));
        storage.addSaveRequest(storage.new FileSaveRequest(
                TAB_ID_2, DATA_ID_2, () -> { return ByteBuffer.wrap(DATA_A); }, (res) -> {}));
        Assert.assertEquals(2, storage.getStorageRequestQueueForTesting().size());
        PersistedTabData.onShutdown();
        Assert.assertEquals(0, storage.getStorageRequestQueueForTesting().size());

        Assert.assertTrue(FilePersistedTabDataStorage.getFile(TAB_ID_1, DATA_ID_1).exists());
        Assert.assertTrue(FilePersistedTabDataStorage.getFile(TAB_ID_2, DATA_ID_2).exists());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShutdownEncrypted() {
        FilePersistedTabDataStorage encryptedStorage =
                PersistedTabDataConfiguration.getEncryptedFilePersistedTabDataStorage();
        Assert.assertFalse(FilePersistedTabDataStorage.getFile(TAB_ID_3, DATA_ID_3).exists());
        Assert.assertFalse(FilePersistedTabDataStorage.getFile(TAB_ID_4, DATA_ID_4).exists());
        encryptedStorage.addSaveRequest(encryptedStorage.new FileSaveRequest(
                TAB_ID_3, DATA_ID_3, () -> { return ByteBuffer.wrap(DATA_B); }, (res) -> {}));
        encryptedStorage.addSaveRequest(encryptedStorage.new FileSaveRequest(
                TAB_ID_4, DATA_ID_4, () -> { return ByteBuffer.wrap(DATA_A); }, (res) -> {}));
        Assert.assertEquals(2, encryptedStorage.getStorageRequestQueueForTesting().size());
        PersistedTabData.onShutdown();
        Assert.assertEquals(0, encryptedStorage.getStorageRequestQueueForTesting().size());

        Assert.assertTrue(FilePersistedTabDataStorage.getFile(TAB_ID_3, DATA_ID_3).exists());
        Assert.assertTrue(FilePersistedTabDataStorage.getFile(TAB_ID_4, DATA_ID_4).exists());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testExecutingRequestInRightQueuePosition() throws IOException {
        FilePersistedTabDataStorage storage =
                PersistedTabDataConfiguration.getFilePersistedTabDataStorage();
        storage.setExecutingSaveRequestForTesting(storage.new FileSaveRequest(
                TAB_ID_1, DATA_ID_1, () -> { return ByteBuffer.wrap(DATA_A); }, (res) -> {}));
        storage.addSaveRequest(storage.new FileSaveRequest(
                TAB_ID_1, DATA_ID_1, () -> { return ByteBuffer.wrap(DATA_B); }, (res) -> {}));
        PersistedTabData.onShutdown();
        // Check second save request was the last to execute. mExecutingSaveRequest should
        // have been inserted at the front of the queue.
        ByteBufferTestUtils.verifyByteBuffer(DATA_B,
                ByteBuffer.wrap(FileUtils.readStream(new FileInputStream(
                        FilePersistedTabDataStorage.getFile(TAB_ID_1, DATA_ID_1)))));
    }
}
