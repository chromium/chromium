// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.DisabledTest;
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
 * Tests related to {@link SubscriptionsManagerImpl}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisabledTest(message = "crbug.com/1194736 Enable this test if the bug is resolved")
public class SubscriptionsManagerImplTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String OFFER_ID_1 = "offer_id_1";
    private static final String OFFER_ID_2 = "offer_id_2";
    private static final String OFFER_ID_3 = "offer_id_3";
    private static final String OFFER_ID_4 = "offer_id_4";

    private CommerceSubscriptionsStorage mStorage;
    private SubscriptionsManagerImpl mSubscriptionsManager;
    private CommerceSubscription mSubscription1;
    private CommerceSubscription mSubscription2;
    private CommerceSubscription mSubscription3;
    private CommerceSubscription mSubscription4;

    @Before
    public void setUp() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mStorage = new CommerceSubscriptionsStorage(Profile.getLastUsedRegularProfile());
            mSubscriptionsManager = new SubscriptionsManagerImpl();
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
                        CommerceSubscription.TrackingIdType.OFFER_ID);
        mSubscription4 =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        OFFER_ID_4, CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                        CommerceSubscription.TrackingIdType.OFFER_ID);
    }

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mStorage.deleteAll();
            mStorage.destroy();
            mSubscriptionsManager.setRemoteSubscriptionsForTesting(null);
        });
    }

    @MediumTest
    @Test
    public void testSubscribeSingle() throws TimeoutException {
        // Since remoteSubscriptions reflect the latest subscriptions from server-side, it should
        // contain newSubscription.
        CommerceSubscription newSubscription = mSubscription4;
        List<CommerceSubscription> remoteSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2, newSubscription));
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription3));
        List<CommerceSubscription> expectedSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2, newSubscription));
        // Simulate subscription state in local database and remote server.
        for (CommerceSubscription subscription : localSubscriptions) {
            save(subscription);
            loadSingleAndCheckResult(
                    CommerceSubscriptionsStorage.getKey(subscription), subscription);
        }
        mSubscriptionsManager.setRemoteSubscriptionsForTesting(remoteSubscriptions);
        // Test local cache is updated after single subscription.
        ThreadUtils.runOnUiThreadBlocking(() -> mSubscriptionsManager.subscribe(newSubscription));
        loadSingleAndCheckResult(CommerceSubscriptionsStorage.getKey(mSubscription3), null);
        loadPrefixAndCheckResult(
                CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK, expectedSubscriptions);
    }

    @MediumTest
    @Test
    public void testSubscribeList() throws TimeoutException {
        // Since remoteSubscriptions reflect the latest subscriptions from server-side, it should
        // contain all subscriptions from newSubscriptions.
        List<CommerceSubscription> newSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2));
        List<CommerceSubscription> remoteSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2, mSubscription4));
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription3));
        List<CommerceSubscription> expectedSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2, mSubscription4));
        // Simulate subscription state in local database and remote server.
        for (CommerceSubscription subscription : localSubscriptions) {
            save(subscription);
            loadSingleAndCheckResult(
                    CommerceSubscriptionsStorage.getKey(subscription), subscription);
        }
        mSubscriptionsManager.setRemoteSubscriptionsForTesting(remoteSubscriptions);
        // Test local cache is updated after subscribing a list of subscriptions.
        ThreadUtils.runOnUiThreadBlocking(() -> mSubscriptionsManager.subscribe(newSubscriptions));
        loadSingleAndCheckResult(CommerceSubscriptionsStorage.getKey(mSubscription3), null);
        loadPrefixAndCheckResult(
                CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK, expectedSubscriptions);
    }

    @MediumTest
    @Test
    public void testUnsubscribe() throws TimeoutException {
        // Since remoteSubscriptions reflect the latest subscriptions from server-side, it should
        // not contain removedSubscription.
        CommerceSubscription removedSubscription = mSubscription3;
        List<CommerceSubscription> remoteSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription4));
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, removedSubscription, mSubscription4));
        List<CommerceSubscription> expectedSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription4));
        // Simulate subscription state in local database and remote server.
        for (CommerceSubscription subscription : localSubscriptions) {
            save(subscription);
            loadSingleAndCheckResult(
                    CommerceSubscriptionsStorage.getKey(subscription), subscription);
        }
        mSubscriptionsManager.setRemoteSubscriptionsForTesting(remoteSubscriptions);
        // Test local cache is updated after unsubscription.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSubscriptionsManager.unsubscribe(removedSubscription));
        loadSingleAndCheckResult(CommerceSubscriptionsStorage.getKey(removedSubscription), null);
        loadPrefixAndCheckResult(
                CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK, expectedSubscriptions);
    }

    @MediumTest
    @Test
    public void testGetLocalSubscriptions() throws TimeoutException {
        List<CommerceSubscription> subscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2));
        for (CommerceSubscription subscription : subscriptions) {
            save(subscription);
            loadSingleAndCheckResult(
                    CommerceSubscriptionsStorage.getKey(subscription), subscription);
        }
        SubscriptionsLoadCallbackHelper ch = new SubscriptionsLoadCallbackHelper();
        int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mSubscriptionsManager.getSubscriptions(
                                CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK, false,
                                (res) -> ch.notifyCalled(res)));
        ch.waitForCallback(chCount);
        List<CommerceSubscription> results = ch.getResultList();
        assertNotNull(results);
        assertEquals(subscriptions.size(), results.size());
        for (int i = 0; i < subscriptions.size(); i++) {
            assertEquals(subscriptions.get(i), results.get(i));
        }
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
