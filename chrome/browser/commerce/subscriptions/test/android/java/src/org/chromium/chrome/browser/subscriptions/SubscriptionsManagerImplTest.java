// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * Tests related to {@link SubscriptionsManagerImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SubscriptionsManagerImplTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final String OFFER_ID_1 = "offer_id_1";
    private static final String OFFER_ID_2 = "offer_id_2";
    private static final String OFFER_ID_3 = "offer_id_3";
    private static final String OFFER_ID_4 = "offer_id_4";

    @Mock
    private Profile mProfile;
    @Mock
    private CommerceSubscriptionsStorage.Natives mCommerceSubscriptionsStorageJni;
    @Mock
    private CommerceSubscriptionsStorage mStorage;
    @Mock
    private CommerceSubscriptionsServiceProxy mProxy;

    private SubscriptionsManagerImpl mSubscriptionsManager;
    private CommerceSubscription mSubscription1;
    private CommerceSubscription mSubscription2;
    private CommerceSubscription mSubscription3;
    private CommerceSubscription mSubscription4;
    private FeatureList.TestValues mTestValues;

    private final class SubscriptionsComparator implements Comparator<CommerceSubscription> {
        @Override
        public int compare(CommerceSubscription s1, CommerceSubscription s2) {
            return s1.getTrackingId().compareTo(s2.getTrackingId());
        }
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mTestValues = new FeatureList.TestValues();
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING, true);
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                PriceTrackingFeatures.PRICE_NOTIFICATION_PARAM, "true");
        FeatureList.setTestValues(mTestValues);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        PriceDropNotificationManager priceDropNotificationManager =
                PriceDropNotificationManagerFactory.create();
        mMocker.mock(CommerceSubscriptionsStorageJni.TEST_HOOKS, mCommerceSubscriptionsStorageJni);
        mSubscriptionsManager = new SubscriptionsManagerImpl(
                mProfile, mStorage, mProxy, priceDropNotificationManager);

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
    public void testSubscribeDeferred() {
        List<CommerceSubscription> remoteSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2));

        Callback<Integer> subsCallback = Mockito.mock(Callback.class);

        // Call subscribe before the service is ready.
        mSubscriptionsManager.subscribe(mSubscription1, subsCallback);
        verify(subsCallback, never()).onResult(any(Integer.class));

        // Capture the getSubscriptions callback but don't resolve it.
        ArgumentCaptor<Callback<List<CommerceSubscription>>> getSubscriptionsCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mProxy, times(1))
                .get(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        getSubscriptionsCallbackCaptor.capture());

        setLoadWithPrefixMockResponse(new ArrayList<>());
        setMockProxyCreateResponse(true);

        // Resolve the original callback to getSubscriptions started through initTypes.
        getSubscriptionsCallbackCaptor.getValue().onResult(remoteSubscriptions);

        // Resolve the callback for getSubscriptions started through subscribe.
        verify(mProxy, times(2))
                .get(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        getSubscriptionsCallbackCaptor.capture());
        getSubscriptionsCallbackCaptor.getValue().onResult(remoteSubscriptions);

        verify(subsCallback, times(1)).onResult(eq(SubscriptionsManager.StatusCode.OK));
    }

    @MediumTest
    @Test
    public void testSubscribeDeferredInternalError() {
        List<CommerceSubscription> remoteSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2));

        Callback<Integer> subsCallback = Mockito.mock(Callback.class);

        // Call subscribe before the service is ready.
        mSubscriptionsManager.subscribe(mSubscription1, subsCallback);
        verify(subsCallback, never()).onResult(any(Integer.class));

        ArgumentCaptor<Callback<List<CommerceSubscription>>> getSubscriptionsCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mProxy, times(1))
                .get(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        getSubscriptionsCallbackCaptor.capture());

        setLoadWithPrefixMockResponse(new ArrayList<>());
        setMockProxyCreateResponse(false);

        // Resolve the original callback to getSubscriptions started through initTypes.
        getSubscriptionsCallbackCaptor.getValue().onResult(remoteSubscriptions);
        verify(subsCallback, times(1)).onResult(eq(SubscriptionsManager.StatusCode.NETWORK_ERROR));
    }

    @MediumTest
    @Test
    public void testSubscribeSingle() {
        CommerceSubscription newSubscription = mSubscription4;
        List<CommerceSubscription> remoteSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2, newSubscription));
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription3));

        mSubscriptionsManager.setCanHandlerequests(true);

        setMockProxyCreateResponse(true);
        setLoadWithPrefixMockResponse(localSubscriptions);

        mSubscriptionsManager.setRemoteSubscriptionsForTesting(remoteSubscriptions);
        Callback<Integer> subsCallback = Mockito.mock(Callback.class);

        // Call subscribe and verify the result.
        mSubscriptionsManager.subscribe(newSubscription, subsCallback);
        verify(subsCallback, times(1)).onResult(SubscriptionsManager.StatusCode.OK);

        ArgumentCaptor<CommerceSubscription> storageSaveCaptor =
                ArgumentCaptor.forClass(CommerceSubscription.class);
        verify(mStorage, times(2)).save(storageSaveCaptor.capture());
        System.out.println(storageSaveCaptor.getAllValues());

        ArgumentCaptor<CommerceSubscription> storageDeleteCaptor =
                ArgumentCaptor.forClass(CommerceSubscription.class);
        verify(mStorage, times(1)).delete(storageDeleteCaptor.capture());
        System.out.println(storageDeleteCaptor.getAllValues());

        List<CommerceSubscription> subscriptionsToSave = storageSaveCaptor.getAllValues();
        Collections.sort(subscriptionsToSave, new SubscriptionsComparator());

        List<CommerceSubscription> subscriptionsToDelete = storageDeleteCaptor.getAllValues();
        Collections.sort(subscriptionsToDelete, new SubscriptionsComparator());

        assertEquals(new ArrayList<>(Arrays.asList(mSubscription1, newSubscription)),
                subscriptionsToSave);
        assertEquals(new ArrayList<>(Arrays.asList(mSubscription3)), subscriptionsToDelete);
    }

    @MediumTest
    @Test
    public void testSubscribeInvalidSubscription() {
        mSubscriptionsManager.setCanHandlerequests(true);
        // Null subscription.
        CommerceSubscription newSubscription = null;
        Callback<Integer> subsCallback = Mockito.mock(Callback.class);
        mSubscriptionsManager.subscribe(newSubscription, subsCallback);
        verify(subsCallback, times(1)).onResult(SubscriptionsManager.StatusCode.INVALID_ARGUMENT);

        // Invalid type.
        newSubscription = new CommerceSubscription(
                CommerceSubscription.CommerceSubscriptionType.TYPE_UNSPECIFIED, OFFER_ID_1,
                CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                CommerceSubscription.TrackingIdType.OFFER_ID);
        Callback<Integer> subsCallback2 = Mockito.mock(Callback.class);
        mSubscriptionsManager.subscribe(newSubscription, subsCallback2);
        verify(subsCallback, times(1)).onResult(SubscriptionsManager.StatusCode.INVALID_ARGUMENT);
    }

    @MediumTest
    @Test
    public void testSubscribeDuplicatesNop() {
        mSubscriptionsManager.setCanHandlerequests(true);
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription3));
        List<CommerceSubscription> newSubscriptions = localSubscriptions;

        setLoadWithPrefixMockResponse(localSubscriptions);

        Callback<Integer> subsCallback = Mockito.mock(Callback.class);
        mSubscriptionsManager.subscribe(newSubscriptions, subsCallback);
        verify(subsCallback, times(1)).onResult(SubscriptionsManager.StatusCode.OK);
        verify(mProxy, never()).create(any(List.class), any(Callback.class));
    }

    @MediumTest
    @Test
    public void testSubscribeCreateUnique() {
        mSubscriptionsManager.setCanHandlerequests(true);
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription3));
        List<CommerceSubscription> newSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2));

        setLoadWithPrefixMockResponse(localSubscriptions);
        setMockProxyCreateResponse(true);

        Callback<Integer> subsCallback = Mockito.mock(Callback.class);
        mSubscriptionsManager.subscribe(newSubscriptions, subsCallback);

        ArgumentCaptor<List<CommerceSubscription>> subscriptionsToCreateCaptor =
                ArgumentCaptor.forClass(List.class);
        verify(mProxy, times(1)).create(subscriptionsToCreateCaptor.capture(), any(Callback.class));
        assertEquals(new ArrayList<>(Arrays.asList(mSubscription1)),
                subscriptionsToCreateCaptor.getValue());
    }

    @MediumTest
    @Test
    public void testSubscribeList() {
        mSubscriptionsManager.setCanHandlerequests(true);
        List<CommerceSubscription> newSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2));
        List<CommerceSubscription> remoteSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2, mSubscription4));
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription3));

        setMockProxyCreateResponse(true);
        setLoadWithPrefixMockResponse(localSubscriptions);

        mSubscriptionsManager.setRemoteSubscriptionsForTesting(remoteSubscriptions);
        Callback<Integer> subsCallback = Mockito.mock(Callback.class);

        // Call subscribe and verify the result.
        mSubscriptionsManager.subscribe(newSubscriptions, subsCallback);
        verify(subsCallback, times(1)).onResult(SubscriptionsManager.StatusCode.OK);

        ArgumentCaptor<CommerceSubscription> storageSaveCaptor =
                ArgumentCaptor.forClass(CommerceSubscription.class);
        verify(mStorage, times(2)).save(storageSaveCaptor.capture());

        ArgumentCaptor<CommerceSubscription> storageDeleteCaptor =
                ArgumentCaptor.forClass(CommerceSubscription.class);
        verify(mStorage, times(1)).delete(storageDeleteCaptor.capture());
        System.out.println(storageDeleteCaptor.getAllValues());

        List<CommerceSubscription> subscriptionsToSave = storageSaveCaptor.getAllValues();
        Collections.sort(subscriptionsToSave, new SubscriptionsComparator());

        List<CommerceSubscription> subscriptionsToDelete = storageDeleteCaptor.getAllValues();
        Collections.sort(subscriptionsToDelete, new SubscriptionsComparator());

        assertEquals(new ArrayList<>(Arrays.asList(mSubscription1, mSubscription4)),
                subscriptionsToSave);
        assertEquals(new ArrayList<>(Arrays.asList(mSubscription3)), subscriptionsToDelete);
    }

    @MediumTest
    @Test
    public void testUnsubscribe() {
        mSubscriptionsManager.setCanHandlerequests(true);
        CommerceSubscription removedSubscription = mSubscription3;
        List<CommerceSubscription> remoteSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription4));
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, removedSubscription, mSubscription4));

        setMockProxyDeleteResponse(true);
        setLoadWithPrefixMockResponse(localSubscriptions);

        mSubscriptionsManager.setRemoteSubscriptionsForTesting(remoteSubscriptions);
        Callback<Integer> subsCallback = Mockito.mock(Callback.class);

        // Call unsubscribe and verify the result.
        mSubscriptionsManager.unsubscribe(removedSubscription, subsCallback);
        verify(subsCallback, times(1)).onResult(SubscriptionsManager.StatusCode.OK);

        ArgumentCaptor<CommerceSubscription> storageSaveCaptor =
                ArgumentCaptor.forClass(CommerceSubscription.class);
        verify(mStorage, never()).save(storageSaveCaptor.capture());

        ArgumentCaptor<CommerceSubscription> storageDeleteCaptor =
                ArgumentCaptor.forClass(CommerceSubscription.class);
        verify(mStorage, times(1)).delete(storageDeleteCaptor.capture());
        System.out.println(storageDeleteCaptor.getAllValues());

        List<CommerceSubscription> subscriptionsToDelete = storageDeleteCaptor.getAllValues();
        Collections.sort(subscriptionsToDelete, new SubscriptionsComparator());

        assertEquals(new ArrayList<>(Arrays.asList(removedSubscription)), subscriptionsToDelete);
    }

    @MediumTest
    @Test
    public void testUnsubscribeNop() {
        mSubscriptionsManager.setCanHandlerequests(true);
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription3));

        setLoadWithPrefixMockResponse(localSubscriptions);

        Callback<Integer> subsCallback = Mockito.mock(Callback.class);
        mSubscriptionsManager.unsubscribe(mSubscription4, subsCallback);

        ArgumentCaptor<List<CommerceSubscription>> subscriptionsToDeleteCaptor =
                ArgumentCaptor.forClass(List.class);
        verify(mProxy, never()).delete(subscriptionsToDeleteCaptor.capture(), any(Callback.class));
    }

    @MediumTest
    @Test
    public void testUnsubscribeDeleteIfInCache() {
        mSubscriptionsManager.setCanHandlerequests(true);
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2, mSubscription3));

        setLoadWithPrefixMockResponse(localSubscriptions);

        Callback<Integer> subsCallback = Mockito.mock(Callback.class);
        mSubscriptionsManager.unsubscribe(mSubscription2, subsCallback);

        ArgumentCaptor<List<CommerceSubscription>> subscriptionsToDeleteCaptor =
                ArgumentCaptor.forClass(List.class);
        verify(mProxy, times(1)).delete(subscriptionsToDeleteCaptor.capture(), any(Callback.class));
        assertEquals(new ArrayList<>(Arrays.asList(mSubscription2)),
                subscriptionsToDeleteCaptor.getValue());
    }

    @MediumTest
    @Test
    public void testGetLocalSubscriptions() {
        mSubscriptionsManager.setCanHandlerequests(true);
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2));

        setLoadWithPrefixMockResponse(localSubscriptions);

        Callback<List<CommerceSubscription>> getSubscriptionsCallback =
                Mockito.mock(Callback.class);
        mSubscriptionsManager.getSubscriptions(
                CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK, false,
                getSubscriptionsCallback);

        ArgumentCaptor<List<CommerceSubscription>> resultCaptor =
                ArgumentCaptor.forClass(List.class);
        verify(getSubscriptionsCallback, times(1)).onResult(resultCaptor.capture());

        Collections.sort(resultCaptor.getValue(), new SubscriptionsComparator());
        Collections.sort(localSubscriptions, new SubscriptionsComparator());
        assertEquals(localSubscriptions, resultCaptor.getValue());
    }

    @MediumTest
    @Test
    public void testIsSubscribedDifferentTrackingIdType() {
        mSubscriptionsManager.setCanHandlerequests(true);
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription2));

        setLoadWithPrefixMockResponse(localSubscriptions);

        CommerceSubscription subscriptionToCheck =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        mSubscription1.getTrackingId(), mSubscription1.getType(),
                        CommerceSubscription.TrackingIdType.PRODUCT_CLUSTER_ID);

        Callback<Boolean> isSubscribedCallback = Mockito.mock(Callback.class);
        mSubscriptionsManager.isSubscribed(subscriptionToCheck, isSubscribedCallback);

        ArgumentCaptor<Boolean> resultCaptor = ArgumentCaptor.forClass(Boolean.class);
        verify(isSubscribedCallback, times(1)).onResult(resultCaptor.capture());
        assertEquals(false, resultCaptor.getValue());
    }

    @MediumTest
    @Test
    public void testIsSubscribedDifferentTimestamps() {
        mSubscriptionsManager.setCanHandlerequests(true);
        CommerceSubscription subscription3 =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        mSubscription1.getTrackingId(), mSubscription1.getType(),
                        mSubscription1.getTrackingIdType(), 1234L);
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, subscription3));

        setLoadWithPrefixMockResponse(localSubscriptions);

        CommerceSubscription subscriptionToCheck =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        mSubscription1.getTrackingId(), mSubscription1.getType(),
                        mSubscription1.getTrackingIdType(), 222L);

        Callback<Boolean> isSubscribedCallback = Mockito.mock(Callback.class);
        mSubscriptionsManager.isSubscribed(subscriptionToCheck, isSubscribedCallback);

        ArgumentCaptor<Boolean> resultCaptor = ArgumentCaptor.forClass(Boolean.class);
        verify(isSubscribedCallback, times(1)).onResult(resultCaptor.capture());
        assertEquals(true, resultCaptor.getValue());
    }

    @MediumTest
    @Test
    public void testIsSubscribed() {
        mSubscriptionsManager.setCanHandlerequests(true);
        List<CommerceSubscription> localSubscriptions =
                new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2));

        setLoadWithPrefixMockResponse(localSubscriptions);

        CommerceSubscription subscriptionToCheck =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        mSubscription1.getTrackingId(), mSubscription1.getType(),
                        mSubscription1.getTrackingIdType());

        Callback<Boolean> isSubscribedCallback = Mockito.mock(Callback.class);
        mSubscriptionsManager.isSubscribed(subscriptionToCheck, isSubscribedCallback);

        ArgumentCaptor<Boolean> resultCaptor = ArgumentCaptor.forClass(Boolean.class);
        verify(isSubscribedCallback, times(1)).onResult(resultCaptor.capture());
        assertEquals(true, resultCaptor.getValue());
    }

    @MediumTest
    @Test
    public void testOnIdentityChanged_AccountCleared() {
        // Fetch subscriptions when SubscriptionManager is created.
        verify(mStorage, times(1)).deleteAll();
        verify(mProxy, times(1))
                .get(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        any(Callback.class));

        // Simulate user signs out. We should delete local storage but not fetch data from server.
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(false);
        mSubscriptionsManager.onIdentityChanged();
        verify(mStorage, times(2)).deleteAll();
        verify(mProxy, times(1))
                .get(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        any(Callback.class));
        verify(mProxy, times(0)).queryAndUpdateWaaEnabled();
    }

    @MediumTest
    @Test
    public void testOnIdentityChanged_AccountChanged() {
        // Fetch subscriptions when SubscriptionManager is created.
        verify(mStorage, times(1)).deleteAll();
        verify(mProxy, times(1))
                .get(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        any(Callback.class));

        // Simulate user switches account. We should delete local storage and also fetch new data
        // from server.
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        mSubscriptionsManager.onIdentityChanged();
        verify(mStorage, times(3)).deleteAll();
        verify(mProxy, times(2))
                .get(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        any(Callback.class));
        verify(mProxy, times(1)).queryAndUpdateWaaEnabled();
    }

    private void setLoadWithPrefixMockResponse(List<CommerceSubscription> subscriptions) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[1];
                callback.onResult(subscriptions);
                return null;
            }
        })
                .when(mStorage)
                .loadWithPrefix(any(String.class), any(Callback.class));
    }

    private void setMockProxyCreateResponse(boolean expectedResult) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[1];
                callback.onResult(expectedResult);
                return null;
            }
        })
                .when(mProxy)
                .create(any(List.class), any(Callback.class));
    }

    private void setMockProxyDeleteResponse(boolean expectedResult) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[1];
                callback.onResult(expectedResult);
                return null;
            }
        })
                .when(mProxy)
                .delete(any(List.class), any(Callback.class));
    }

    private void printList(List<CommerceSubscription> list) {
        for (CommerceSubscription ss : list) {
            System.out.println(ss.getTrackingId());
        }
    }
}
