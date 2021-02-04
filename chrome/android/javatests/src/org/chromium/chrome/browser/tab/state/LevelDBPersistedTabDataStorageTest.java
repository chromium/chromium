// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests relating to {@link LevelDBPersistedTabDataStorage}
 * TODO(crbug.com/1146803) investigate batching tests
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

    private static final byte[] DATA_A = {13, 14};
    private static final byte[] DATA_B = {9, 10};
    private static final byte[] EMPTY_BYTE_ARRAY = {};

    private LevelDBPersistedTabDataStorage mPersistedTabDataStorage;

    @Before
    public void setUp() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPersistedTabDataStorage =
                    new LevelDBPersistedTabDataStorage(Profile.getLastUsedRegularProfile());
        });
    }

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mPersistedTabDataStorage.destroy(); });
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

    private void save(int tabId, String dataId, byte[] data) throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPersistedTabDataStorage.saveForTesting(tabId, dataId, data, new Runnable() {
                @Override
                public void run() {
                    ch.notifyCalled();
                }
            });
        });
        ch.waitForCallback(chCount);
    }

    private void restoreAndCheckResult(int tabId, String dataId, byte[] expected)
            throws TimeoutException {
        LoadCallbackHelper ch = new LoadCallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mPersistedTabDataStorage.restore(tabId, dataId, (res) -> { ch.notifyCalled(res); });
        });
        ch.waitForCallback(chCount);
        Assert.assertArrayEquals(expected, ch.getRes());
    }

    private void delete(int tabId, String dataId) throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mPersistedTabDataStorage.deleteForTesting(tabId, dataId, new Runnable() {
                @Override
                public void run() {
                    ch.notifyCalled();
                }
            });
        });
        ch.waitForCallback(chCount);
    }

}
