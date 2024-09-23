// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests related to {@link MerchantTrustSignalsEventStorage}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class MerchantTrustSignalsEventStorageTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final String KEY_1 = "www.amazon.com";
    private static final String KEY_2 = "www.costco.com";
    private static final String KEY_3 = "www.cvs.com";
    private static final String PREFIX_1 = "www.";
    private static final String PREFIX_2 = "www.c";

    private static final long TIMESTAMP_1 = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(2);
    private static final long TIMESTAMP_2 = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1);
    private static final long TIMESTAMP_3 = System.currentTimeMillis();

    private MerchantTrustSignalsEventStorage mStorage;
    private MerchantTrustSignalsEvent mEvent1;
    private MerchantTrustSignalsEvent mEvent2;
    private MerchantTrustSignalsEvent mEvent3;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStorage =
                            new MerchantTrustSignalsEventStorage(
                                    ProfileManager.getLastUsedRegularProfile());
                });

        mEvent1 = new MerchantTrustSignalsEvent(KEY_1, TIMESTAMP_1);
        mEvent2 = new MerchantTrustSignalsEvent(KEY_2, TIMESTAMP_2);
        mEvent3 = new MerchantTrustSignalsEvent(KEY_3, TIMESTAMP_3);
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStorage.deleteAll();
                });
    }

    @MediumTest
    @Test
    public void testSaveLoadDelete() throws TimeoutException {
        save(mEvent1);
        loadSingleAndCheckResult(KEY_1, mEvent1);
        save(mEvent2);
        loadSingleAndCheckResult(KEY_2, mEvent2);
        delete(mEvent1);
        loadSingleAndCheckResult(KEY_1, null);
        loadSingleAndCheckResult(KEY_2, mEvent2);
    }

    @MediumTest
    @Test
    public void testLoadWithPrefix() throws TimeoutException {
        save(mEvent1);
        loadSingleAndCheckResult(KEY_1, mEvent1);
        save(mEvent2);
        loadSingleAndCheckResult(KEY_2, mEvent2);
        save(mEvent3);
        loadSingleAndCheckResult(KEY_3, mEvent3);
        loadPrefixAndCheckResult(
                PREFIX_1, new ArrayList<>(Arrays.asList(mEvent1, mEvent2, mEvent3)));
        loadPrefixAndCheckResult(PREFIX_2, new ArrayList<>(Arrays.asList(mEvent2, mEvent3)));
    }

    @MediumTest
    @Test
    public void testDeleteAll() throws TimeoutException {
        save(mEvent1);
        loadSingleAndCheckResult(KEY_1, mEvent1);
        save(mEvent2);
        loadSingleAndCheckResult(KEY_2, mEvent2);
        deleteAll();
        loadSingleAndCheckResult(KEY_1, null);
        loadSingleAndCheckResult(KEY_2, null);
    }

    private void save(MerchantTrustSignalsEvent event) throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStorage.saveWithCallback(
                            event,
                            new Runnable() {
                                @Override
                                public void run() {
                                    ch.notifyCalled();
                                }
                            });
                });
        ch.waitForCallback(chCount);
    }

    private void delete(MerchantTrustSignalsEvent event) throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStorage.deleteForTesting(
                            event,
                            new Runnable() {
                                @Override
                                public void run() {
                                    ch.notifyCalled();
                                }
                            });
                });
        ch.waitForCallback(chCount);
    }

    private void deleteAll() throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStorage.deleteAllForTesting(
                            new Runnable() {
                                @Override
                                public void run() {
                                    ch.notifyCalled();
                                }
                            });
                });
        ch.waitForCallback(chCount);
    }

    private void loadSingleAndCheckResult(String key, MerchantTrustSignalsEvent expected)
            throws TimeoutException {
        MerchantTrustSignalsEventLoadCallbackHelper ch =
                new MerchantTrustSignalsEventLoadCallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> mStorage.load(key, (res) -> ch.notifyCalled(res)));
        ch.waitForCallback(chCount);
        MerchantTrustSignalsEvent actual = ch.getSingleResult();
        if (expected == null) {
            assertNull(actual);
            return;
        }
        assertNotNull(actual);
        assertEquals(expected, actual);
    }

    private void loadPrefixAndCheckResult(String prefix, List<MerchantTrustSignalsEvent> expected)
            throws TimeoutException {
        MerchantTrustSignalsEventLoadCallbackHelper ch =
                new MerchantTrustSignalsEventLoadCallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mStorage.loadWithPrefix(prefix, (res) -> ch.notifyCalled(res)));
        ch.waitForCallback(chCount);
        List<MerchantTrustSignalsEvent> actual = ch.getResultList();
        assertNotNull(actual);
        assertEquals(expected.size(), actual.size());
        for (int i = 0; i < expected.size(); i++) {
            assertEquals(expected.get(i), actual.get(i));
        }
    }
}
