// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import androidx.test.filters.MediumTest;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests related to {@link CommerceSubscriptionsStorage}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class CommerceSubscriptionsStorageTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String OFFER_ID_1 = "offer_id_1";
    private static final String OFFER_ID_2 = "offer_id_2";
    private static final String OFFER_ID_3 = "offer_id_3";
    private static final String PRODUCT_CLUSTER_ID = "product_cluster_id";
    private static final String KEY_1 = "PRICE_TRACK_OFFER_ID_offer_id_1";
    private static final String KEY_2 = "PRICE_TRACK_OFFER_ID_offer_id_2";
    private static final String KEY_3 = "PRICE_TRACK_IDENTIFIER_TYPE_UNSPECIFIED_offer_id_3";

    private CommerceSubscriptionsStorage mStorage;
    private CommerceSubscription mSubscription1;
    private CommerceSubscription mSubscription2;
    private CommerceSubscription mSubscription3;
    private CommerceSubscription mSubscription4;

    @Before
    public void setUp() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mStorage = new CommerceSubscriptionsStorage(Profile.getLastUsedRegularProfile());
        });

        mSubscription1 =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        OFFER_ID_1, CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                        CommerceSubscription.TrackingIdType.OFFER_ID);
        mSubscription2 =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        OFFER_ID_2, CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                        CommerceSubscription.TrackingIdType.OFFER_ID);
        mSubscription3 =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        OFFER_ID_3, CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                        CommerceSubscription.TrackingIdType.IDENTIFIER_TYPE_UNSPECIFIED);
        mSubscription4 = new CommerceSubscription(
                CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK, PRODUCT_CLUSTER_ID,
                CommerceSubscription.SubscriptionManagementType.USER_MANAGED,
                CommerceSubscription.TrackingIdType.PRODUCT_CLUSTER_ID);
    }

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mStorage.deleteAll();
            mStorage.destroy();
        });
    }

    @SmallTest
    @Test
    public void testGenerateStorageID() throws TimeoutException {
        assertEquals(KEY_1, CommerceSubscriptionsStorage.getKey(mSubscription1));
        assertEquals(KEY_2, CommerceSubscriptionsStorage.getKey(mSubscription2));
        assertEquals(KEY_3, CommerceSubscriptionsStorage.getKey(mSubscription3));
    }

    @MediumTest
    @Test
    public void testSaveLoadDelete() throws TimeoutException {
        save(mSubscription1);
        loadSingleAndCheckResult(
                CommerceSubscriptionsStorage.getKey(mSubscription1), mSubscription1);
        save(mSubscription2);
        loadSingleAndCheckResult(
                CommerceSubscriptionsStorage.getKey(mSubscription2), mSubscription2);
        delete(mSubscription1);
        loadSingleAndCheckResult(CommerceSubscriptionsStorage.getKey(mSubscription1), null);
        loadSingleAndCheckResult(
                CommerceSubscriptionsStorage.getKey(mSubscription2), mSubscription2);
    }

    @MediumTest
    @Test
    public void testLoadWithPrefix() throws TimeoutException {
        save(mSubscription1);
        loadSingleAndCheckResult(
                CommerceSubscriptionsStorage.getKey(mSubscription1), mSubscription1);
        save(mSubscription2);
        loadSingleAndCheckResult(
                CommerceSubscriptionsStorage.getKey(mSubscription2), mSubscription2);
        save(mSubscription3);
        loadSingleAndCheckResult(
                CommerceSubscriptionsStorage.getKey(mSubscription3), mSubscription3);
        save(mSubscription4);
        loadSingleAndCheckResult(
                CommerceSubscriptionsStorage.getKey(mSubscription4), mSubscription4);
        String prefix1 =
                String.format("%s_%s", CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        CommerceSubscription.TrackingIdType.OFFER_ID);
        loadPrefixAndCheckResult(
                prefix1, new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2)));
        String prefix2 = CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK;
        loadPrefixAndCheckResult(prefix2,
                new ArrayList<>(Arrays.asList(
                        mSubscription3, mSubscription1, mSubscription2, mSubscription4)));
    }

    @MediumTest
    @Test
    public void testDeleteAll() throws TimeoutException {
        save(mSubscription1);
        loadSingleAndCheckResult(
                CommerceSubscriptionsStorage.getKey(mSubscription1), mSubscription1);
        save(mSubscription2);
        loadSingleAndCheckResult(
                CommerceSubscriptionsStorage.getKey(mSubscription2), mSubscription2);
        deleteAll();
        loadSingleAndCheckResult(CommerceSubscriptionsStorage.getKey(mSubscription1), null);
        loadSingleAndCheckResult(CommerceSubscriptionsStorage.getKey(mSubscription2), null);
    }

    private void save(CommerceSubscription subscription) throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mStorage.saveWithCallback(subscription, new Runnable() {
                @Override
                public void run() {
                    ch.notifyCalled();
                }
            });
        });
        ch.waitForCallback(chCount);
    }

    private void delete(CommerceSubscription subscription) throws TimeoutException {
        CallbackHelper ch = new CallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mStorage.deleteForTesting(subscription, new Runnable() {
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
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mStorage.deleteAllForTesting(new Runnable() {
                @Override
                public void run() {
                    ch.notifyCalled();
                }
            });
        });
        ch.waitForCallback(chCount);
    }

    private void loadSingleAndCheckResult(String key, CommerceSubscription expected)
            throws TimeoutException {
        SubscriptionsLoadCallbackHelper ch = new SubscriptionsLoadCallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> mStorage.load(key, (res) -> ch.notifyCalled(res)));
        ch.waitForCallback(chCount);
        CommerceSubscription actual = ch.getSingleResult();
        if (expected == null) {
            assertNull(actual);
            return;
        }
        assertNotNull(actual);
        assertEquals(expected, actual);
    }

    private void loadPrefixAndCheckResult(String prefix, List<CommerceSubscription> expected)
            throws TimeoutException {
        SubscriptionsLoadCallbackHelper ch = new SubscriptionsLoadCallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mStorage.loadWithPrefix(prefix, (res) -> ch.notifyCalled(res)));
        ch.waitForCallback(chCount);
        List<CommerceSubscription> actual = ch.getResultList();
        assertNotNull(actual);
        assertEquals(expected.size(), actual.size());
        for (int i = 0; i < expected.size(); i++) {
            assertEquals(expected.get(i), actual.get(i));
        }
    }
}
