// Copyright 2020 The Chromium Authors
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

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests relating to {@link LevelDBPersistedTabDataStorage} TODO(crbug.com/40156392) investigate
 * batching tests
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class LevelDBPersistedTabDataStorageTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(mActivityTestRule, false);

    private static final int TAB_ID_1 = 1;
    private static final String DATA_ID_1 = "DataId1";
    private static final int TAB_ID_2 = 2;
    private static final String DATA_ID_2 = "DataId2";
    private static final int TAB_ID_3 = 3;
    private static final String NON_MATCHING_DATA_ID = "asdf";

    private static final byte[] DATA_A = {13, 14};
    private static final byte[] DATA_B = {9, 10};
    private static final byte[] DATA_C = {11, 2};
    private static final byte[] DATA_D = {42, 11};

    private static final byte[] EMPTY_BYTE_ARRAY = {};

    private LevelDBPersistedTabDataStorage mPersistedTabDataStorage;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPersistedTabDataStorage =
                            new LevelDBPersistedTabDataStorage(
                                    ProfileManager.getLastUsedRegularProfile());
                });
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPersistedTabDataStorage.destroy();
                });
    }

    @SmallTest
    @Test
    public void testSaveRestoreDelete() throws TimeoutException {
        save(TAB_ID_1, DATA_ID_1, DATA_A);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, DATA_A);
        delete(TAB_ID_1, DATA_ID_1);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testOverwriteDelete() throws TimeoutException {
        save(TAB_ID_1, DATA_ID_1, DATA_A);
        save(TAB_ID_1, DATA_ID_1, DATA_B);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, DATA_B);
        delete(TAB_ID_1, DATA_ID_1);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testMultipleTabs() throws TimeoutException {
        save(TAB_ID_1, DATA_ID_1, DATA_A);
        save(TAB_ID_2, DATA_ID_2, DATA_B);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, DATA_A);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_2, DATA_B);
        delete(TAB_ID_1, DATA_ID_1);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, EMPTY_BYTE_ARRAY);
        delete(TAB_ID_2, DATA_ID_2);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_2, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testMultipleTabsSameDataID() throws TimeoutException {
        save(TAB_ID_1, DATA_ID_1, DATA_A);
        save(TAB_ID_2, DATA_ID_1, DATA_B);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, DATA_A);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_1, DATA_B);
        delete(TAB_ID_1, DATA_ID_1);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, EMPTY_BYTE_ARRAY);
        delete(TAB_ID_2, DATA_ID_1);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_1, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testMultipleTabsSameData() throws TimeoutException {
        save(TAB_ID_1, DATA_ID_1, DATA_A);
        save(TAB_ID_2, DATA_ID_2, DATA_A);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, DATA_A);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_2, DATA_A);
        delete(TAB_ID_1, DATA_ID_1);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, EMPTY_BYTE_ARRAY);
        delete(TAB_ID_2, DATA_ID_2);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_2, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testMaintenanceKeepSomeKeys() throws TimeoutException {
        save(TAB_ID_1, DATA_ID_1, DATA_A);
        save(TAB_ID_2, DATA_ID_1, DATA_B);
        save(TAB_ID_3, DATA_ID_1, DATA_C);
        save(TAB_ID_1, DATA_ID_2, DATA_D);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, DATA_A);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_1, DATA_B);
        restoreAndCheckResult(TAB_ID_3, DATA_ID_1, DATA_C);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_2, DATA_D);
        performMaintenance(Arrays.asList(TAB_ID_1, TAB_ID_3), DATA_ID_1);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, DATA_A);
        restoreAndCheckResult(TAB_ID_3, DATA_ID_1, DATA_C);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_2, DATA_D);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_1, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testMaintenanceKeepNoKeys() throws TimeoutException {
        save(TAB_ID_1, DATA_ID_1, DATA_A);
        save(TAB_ID_2, DATA_ID_1, DATA_B);
        save(TAB_ID_3, DATA_ID_1, DATA_C);
        save(TAB_ID_1, DATA_ID_2, DATA_D);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, DATA_A);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_1, DATA_B);
        restoreAndCheckResult(TAB_ID_3, DATA_ID_1, DATA_C);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_2, DATA_D);
        performMaintenance(Collections.emptyList(), DATA_ID_1);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_2, DATA_D);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, EMPTY_BYTE_ARRAY);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_1, EMPTY_BYTE_ARRAY);
        restoreAndCheckResult(TAB_ID_3, DATA_ID_1, EMPTY_BYTE_ARRAY);
    }

    @SmallTest
    @Test
    public void testMaintenanceNonMatchingDataId() throws TimeoutException {
        save(TAB_ID_1, DATA_ID_1, DATA_A);
        save(TAB_ID_2, DATA_ID_1, DATA_B);
        save(TAB_ID_3, DATA_ID_1, DATA_C);
        save(TAB_ID_1, DATA_ID_2, DATA_D);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, DATA_A);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_1, DATA_B);
        restoreAndCheckResult(TAB_ID_3, DATA_ID_1, DATA_C);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_2, DATA_D);
        performMaintenance(Arrays.asList(TAB_ID_1, TAB_ID_3), NON_MATCHING_DATA_ID);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_1, DATA_A);
        restoreAndCheckResult(TAB_ID_2, DATA_ID_1, DATA_B);
        restoreAndCheckResult(TAB_ID_3, DATA_ID_1, DATA_C);
        restoreAndCheckResult(TAB_ID_1, DATA_ID_2, DATA_D);
    }

    private void save(int tabId, String dataId, byte[] data) throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPersistedTabDataStorage.saveForTesting(
                            tabId,
                            dataId,
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

    private void performMaintenance(List<Integer> tabIds, String dataId) throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPersistedTabDataStorage.performMaintenanceForTesting(
                            tabIds,
                            dataId,
                            () -> {
                                ch.notifyCalled();
                            });
                });
        ch.waitForCallback(chCount);
    }

    private void restoreAndCheckResult(int tabId, String dataId, byte[] expected)
            throws TimeoutException {
        LoadCallbackHelper ch = new LoadCallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPersistedTabDataStorage.restore(
                            tabId,
                            dataId,
                            (res) -> {
                                ch.notifyCalled(res);
                            });
                });
        ch.waitForCallback(chCount);
        ByteBufferTestUtils.verifyByteBuffer(expected, ch.getRes());
    }

    private void delete(int tabId, String dataId) throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPersistedTabDataStorage.deleteForTesting(
                            tabId,
                            dataId,
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
