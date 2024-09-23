// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ByteBufferTestUtils;

import java.nio.ByteBuffer;
import java.util.concurrent.TimeoutException;

/** Tests relating to {@link LevelDBPersistedDataStorage} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class LevelDBPersistedDataStorageTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String KEY_1 = "key1";
    private static final String KEY_2 = "key2";

    private static final byte[] DATA_A = {13, 14};
    private static final byte[] DATA_B = {9, 10};
    private static final byte[] EMPTY_BYTE_ARRAY = {};

    private static final String NAMESPACES[] = {"namespace1", "namesapce2"};

    private LevelDBPersistedDataStorage[] mPersistedDataStorage =
            new LevelDBPersistedDataStorage[2];

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int i = 0; i < mPersistedDataStorage.length; i++) {
                        mPersistedDataStorage[i] =
                                new LevelDBPersistedDataStorage(
                                        ProfileManager.getLastUsedRegularProfile(), NAMESPACES[i]);
                    }
                });
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Both PersistedDataStorage are associated with the same BrowserContext so
                    // calling destroy() on the first one will free the same SessionProtoDB for
                    // all of them.
                    // Calling on both would cause call destroy() on a freed SessionProtoDB.
                    mPersistedDataStorage[0].destroy();
                });
    }

    @SmallTest
    @Test
    public void testSaveLoadDelete() throws TimeoutException {
        save0(KEY_1, DATA_A);
        loadAndCheckResult0(KEY_1, DATA_A);
        delete0(KEY_1);
        loadAndCheckResult0(KEY_1, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testOverwriteDelete() throws TimeoutException {
        save0(KEY_1, DATA_A);
        save0(KEY_1, DATA_B);
        loadAndCheckResult0(KEY_1, DATA_B);
        delete0(KEY_1);
        loadAndCheckResult0(KEY_1, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testMultipleKeys() throws TimeoutException {
        save0(KEY_1, DATA_A);
        save0(KEY_2, DATA_B);
        loadAndCheckResult0(KEY_1, DATA_A);
        loadAndCheckResult0(KEY_2, DATA_B);
        delete0(KEY_1);
        loadAndCheckResult0(KEY_1, EMPTY_BYTE_ARRAY);
        delete0(KEY_2);
        loadAndCheckResult0(KEY_2, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testSameData() throws TimeoutException {
        save0(KEY_1, DATA_A);
        save0(KEY_2, DATA_A);
        loadAndCheckResult0(KEY_1, DATA_A);
        loadAndCheckResult0(KEY_2, DATA_A);
        delete0(KEY_1);
        loadAndCheckResult0(KEY_1, EMPTY_BYTE_ARRAY);
        delete0(KEY_2);
        loadAndCheckResult0(KEY_2, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testSaveLoadAcrossNamespaces1() throws TimeoutException {
        save0(KEY_1, DATA_A);
        loadAndCheckResult0(KEY_1, DATA_A);
        loadAndCheckResult1(KEY_1, EMPTY_BYTE_ARRAY);
        delete0(KEY_1);
        loadAndCheckResult0(KEY_1, EMPTY_BYTE_ARRAY);
        loadAndCheckResult1(KEY_1, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testSaveLoadAcrossNamespaces2() throws TimeoutException {
        save1(KEY_1, DATA_A);
        loadAndCheckResult0(KEY_1, EMPTY_BYTE_ARRAY);
        loadAndCheckResult1(KEY_1, DATA_A);
        delete1(KEY_1);
        loadAndCheckResult0(KEY_1, EMPTY_BYTE_ARRAY);
        loadAndCheckResult1(KEY_1, EMPTY_BYTE_ARRAY);
    }

    /** Functions for first namespace */
    private void save0(String key, byte[] data) throws TimeoutException {
        save(key, data, mPersistedDataStorage[0]);
    }

    private void loadAndCheckResult0(String key, byte[] expected) throws TimeoutException {
        loadAndCheckResult(key, expected, mPersistedDataStorage[0]);
    }

    private void delete0(String key) throws TimeoutException {
        delete(key, mPersistedDataStorage[0]);
    }

    /** Functions for second namespace */
    private void save1(String key, byte[] data) throws TimeoutException {
        save(key, data, mPersistedDataStorage[1]);
    }

    private void loadAndCheckResult1(String key, byte[] expected) throws TimeoutException {
        loadAndCheckResult(key, expected, mPersistedDataStorage[1]);
    }

    private void delete1(String key) throws TimeoutException {
        delete(key, mPersistedDataStorage[1]);
    }

    private void save(String key, byte[] data, LevelDBPersistedDataStorage persistedDataStorage)
            throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    persistedDataStorage.saveForTesting(
                            key,
                            data,
                            new Runnable() {
                                @Override
                                public void run() {
                                    ch.notifyCalled();
                                }
                            });
                });
        ch.waitForCallback(chCount);
    }

    private void loadAndCheckResult(
            String key, byte[] expected, LevelDBPersistedDataStorage persistedDataStorage)
            throws TimeoutException {
        LoadCallbackHelper ch = new LoadCallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    persistedDataStorage.load(
                            key,
                            (res) -> {
                                ch.notifyCalled(ByteBuffer.wrap(res));
                            });
                });
        ch.waitForCallback(chCount);
        ByteBufferTestUtils.verifyByteBuffer(expected, ch.getRes());
    }

    private void delete(String key, LevelDBPersistedDataStorage persistedDataStorage)
            throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    persistedDataStorage.deleteForTesting(
                            key,
                            new Runnable() {
                                @Override
                                public void run() {
                                    ch.notifyCalled();
                                }
                            });
                });
        ch.waitForCallback(chCount);
    }
}
