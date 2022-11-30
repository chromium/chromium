// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.json.JSONException;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.ArrayList;

/**
 * Tests for {@link CommerceSubscriptionsServiceProxy}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class CommerceSubscriptionsServiceProxyUnitTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final String EXPECTED_CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String[] EXPECTED_OAUTH_SCOPES =
            new String[] {"https://www.googleapis.com/auth/chromememex"};
    private static final String HTTP_GET = "GET";
    private static final String HTTP_POST = "POST";
    private static final String EMPTY_RESPONSE = "{}";
    private static final String FAKE_GET_RESPONSE =
            "{ \"subscriptions\": [ { \"type\": \"PRICE_TRACK\","
            + " \"managementType\": \"CHROME_MANAGED\","
            + "\"identifierType\": \"OFFER_ID\", \"identifier\": \"190190\","
            + "\"eventTimestampMicros\": \"200\" } ] }";

    private static final String DEFAULT_ENDPOINT = "https://memex-pa.googleapis.com/v1/annotations";
    private static final String ENDPOINT_OVERRIDE = "my-endpoint.com";
    private static final CommerceSubscription FAKE_SUBSCRIPTION =
            new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                    "190190", CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                    CommerceSubscription.TrackingIdType.OFFER_ID);

    @Mock
    EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    private Profile mProfile;

    private CommerceSubscriptionsServiceProxy mServiceProxy;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        doReturn(false).when(mProfile).isOffTheRecord();
        Profile.setLastUsedProfileForTesting(mProfile);
        mServiceProxy = new CommerceSubscriptionsServiceProxy(mProfile);
    }

    @After
    public void tearDown() throws Exception {
        Profile.setLastUsedProfileForTesting(null);
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testGetSubscriptions() {
        mockEndpointResponse(FAKE_GET_RESPONSE);
        mServiceProxy.get(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK, (result) -> {
            assertNotNull(result);
            Assert.assertEquals(1, result.size());

            CommerceSubscription subscription = result.get(0);
            Assert.assertEquals(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                    subscription.getType());
            Assert.assertEquals(CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                    subscription.getManagementType());
            Assert.assertEquals(
                    CommerceSubscription.TrackingIdType.OFFER_ID, subscription.getTrackingIdType());
            Assert.assertEquals(200L, subscription.getTimestamp());
            Assert.assertEquals("190190", subscription.getTrackingId());
        });
    }

    @UiThreadTest
    @Test
    @SmallTest
    public void testGetSubscriptions_EmptyResponse() {
        mockEndpointResponse(EMPTY_RESPONSE);
        mServiceProxy.get(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                (result) -> { Assert.assertTrue(result.isEmpty()); });
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:subscriptions_service_base_url/my-endpoint.com"})
    public void testGetSubscriptions_ValidRequest() {
        mServiceProxy.get(
                CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK, (result) -> {});
        verifyEndpointFetcherCalled(
                1, "my-endpoint.com?requestParams.subscriptionType=PRICE_TRACK", "GET", "");
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:subscriptions_service_base_url/my-endpoint.com"})
    public void testCreateSubscriptions_ValidRequest() throws JSONException {
        String expectedJsonPayload = "";
        mServiceProxy.create(new ArrayList<CommerceSubscription>() {
            { add(FAKE_SUBSCRIPTION); }
        }, (success) -> {});
        verifyEndpointFetcherCalled(1, "my-endpoint.com", "POST",
                "{\"createShoppingSubscriptionsParams\":"
                        + "{\"subscriptions\":[{\"type\":\"PRICE_TRACK\","
                        + "\"managementType\":\"CHROME_MANAGED\","
                        + "\"identifierType\":\"OFFER_ID\",\"identifier\":\"190190\"}]}}");
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:subscriptions_service_base_url/my-endpoint.com"})
    public void testCreateSubscriptions_InvalidResponse() throws JSONException {
        mockEndpointResponse(EMPTY_RESPONSE);
        mServiceProxy.create(
                new ArrayList<CommerceSubscription>() {
                    { add(FAKE_SUBSCRIPTION); }
                },
                (success) -> { Assert.assertFalse(success); });
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:subscriptions_service_base_url/my-endpoint.com"})
    public void testCreateSubscriptions() throws JSONException {
        mockEndpointResponse("{ \"status\": { \"code\": 0 } }");
        mServiceProxy.create(
                new ArrayList<CommerceSubscription>() {
                    { add(FAKE_SUBSCRIPTION); }
                },
                (success) -> { Assert.assertTrue(success); });
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:subscriptions_service_base_url/my-endpoint.com"})
    public void testDeleteSubscriptions_ValidRequest() throws JSONException {
        CommerceSubscription fakeSubscriptionToDelete =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        "190190", CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                        CommerceSubscription.TrackingIdType.OFFER_ID, 1617309553897712L);

        mServiceProxy.delete(new ArrayList<CommerceSubscription>() {
            { add(fakeSubscriptionToDelete); }
        }, (success) -> {});
        verifyEndpointFetcherCalled(1, "my-endpoint.com", "POST",
                "{\"removeShoppingSubscriptionsParams\":"
                        + "{\"eventTimestampMicros\":[1617309553897712]}}");
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:subscriptions_service_base_url/my-endpoint.com"})
    public void testDeleteSubscriptions_InvalidResponse() throws JSONException {
        mockEndpointResponse(EMPTY_RESPONSE);
        mServiceProxy.delete(
                new ArrayList<CommerceSubscription>() {
                    { add(FAKE_SUBSCRIPTION); }
                },
                (success) -> { Assert.assertFalse(success); });
    }

    @UiThreadTest
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:subscriptions_service_base_url/my-endpoint.com"})
    public void testDeleteSubscriptions() throws JSONException {
        mockEndpointResponse("{ \"status\": { \"code\": 0 } }");
        mServiceProxy.delete(
                new ArrayList<CommerceSubscription>() {
                    { add(FAKE_SUBSCRIPTION); }
                },
                (success) -> { Assert.assertTrue(success); });
    }

    private void verifyEndpointFetcherCalled(
            int numTimes, String expectedUrl, String expectedMethod, String expectedPayload) {
        verify(mEndpointFetcherJniMock, times(numTimes))
                .nativeFetchOAuth(any(Profile.class), any(String.class), eq(expectedUrl),
                        eq(expectedMethod), eq(EXPECTED_CONTENT_TYPE), eq(EXPECTED_OAUTH_SCOPES),
                        eq(expectedPayload), anyLong(), anyInt(), any(Callback.class));
    }

    private void mockEndpointResponse(String response) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[9];
                callback.onResult(new EndpointResponse(response));
                return null;
            }
        })
                .when(mEndpointFetcherJniMock)
                .nativeFetchOAuth(any(Profile.class), any(String.class), any(String.class),
                        any(String.class), eq(EXPECTED_CONTENT_TYPE), eq(EXPECTED_OAUTH_SCOPES),
                        any(String.class), anyLong(), anyInt(), any(Callback.class));
    }
}
