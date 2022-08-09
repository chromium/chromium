// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.nio.ByteBuffer;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Test relating to {@link CouponPersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CouponPersistedTabDataTest {
    @Rule
    public JniMocker mMocker = new JniMocker();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Mock
    private EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    private Profile mProfileMock;

    private static final String SERIALIZE_DESERIALIZE_NAME = "25% Off";
    private static final String SERIALIZE_DESERIALIZE_CODE = "DISCOUNT25";

    private static final String MOCK_ENDPOINT_RESPONSE_STRING =
            "{\"discounts\":[{\"amountOff\":{\"currencyCode\":\"USD\",\"units\":\"20\"},"
            + "\"freeListingDiscountInfo\":{\"promoDocid\":\"16137933697037590414\","
            + "\"longTitle\":\"$20 off on dining set\",\"couponCode\":\"DIN20\"},"
            + "\"isMerchantLevelDiscount\":false}]}";
    private static final String EXPECTED_NAME_GENERAL_CASE = "$20 off on dining set";
    private static final String EXPECTED_CODE_GENERAL_CASE = "DIN20";

    private static final String EMPTY_ENDPOINT_RESPONSE = "";
    private static final String MALFORMED_ENDPOINT_RESPONSE = "malformed response";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersistedTabDataConfiguration.setUseTestConfig(true));
        Profile.setLastUsedProfileForTesting(mProfileMock);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
        PersistedTabDataConfiguration.setUseTestConfig(false);
    }

    @SmallTest
    @Test
    public void testSerializeDeserialize() throws ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> new MockTab(1, false));
        CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab,
                new CouponPersistedTabData.Coupon(
                        SERIALIZE_DESERIALIZE_NAME, SERIALIZE_DESERIALIZE_CODE));
        ByteBuffer serialized = couponPersistedTabData.getSerializer().get();
        CouponPersistedTabData deserialized = new CouponPersistedTabData(tab);
        Assert.assertTrue(deserialized.deserialize(serialized));
        Assert.assertEquals(SERIALIZE_DESERIALIZE_NAME, deserialized.getCoupon().couponName);
        Assert.assertEquals(SERIALIZE_DESERIALIZE_CODE, deserialized.getCoupon().promoCode);
    }

    @SmallTest
    @Test
    public void testSerializeDeserializeNull() throws ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> new MockTab(1, false));
        CouponPersistedTabData deserialized = new CouponPersistedTabData(tab, null);
        Assert.assertFalse(deserialized.deserialize(null));
    }

    @SmallTest
    @Test
    public void testSerializeDeserializeNoRemainingBytes() throws ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> new MockTab(1, false));
        CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab, null);
        ByteBuffer serialized = couponPersistedTabData.getSerializer().get();
        CouponPersistedTabData deserialized = new CouponPersistedTabData(tab);
        Assert.assertFalse(serialized.hasRemaining());
        Assert.assertFalse(deserialized.deserialize(serialized));
    }

    @SmallTest
    @Test
    public void testCOPTDUponSuccessfulResponse() throws TimeoutException, ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        mockEndpointResponse(MOCK_ENDPOINT_RESPONSE_STRING);
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(tab, (res) -> {
                Assert.assertEquals(EXPECTED_NAME_GENERAL_CASE, res.getCoupon().couponName);
                Assert.assertEquals(EXPECTED_CODE_GENERAL_CASE, res.getCoupon().promoCode);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testCOPTDNullUponEmptyUnsuccessfulResponse()
            throws TimeoutException, ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        mockEndpointResponse(EMPTY_ENDPOINT_RESPONSE);
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(tab, (res) -> {
                Assert.assertNull(res);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testCOPTDNullUponMalformedUnsuccessfulResponse()
            throws TimeoutException, ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        mockEndpointResponse(MALFORMED_ENDPOINT_RESPONSE);
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(tab, (res) -> {
                Assert.assertNull(res);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testIncognitoTabDisabled() throws TimeoutException {
        TabImpl tab = mock(TabImpl.class);
        doReturn(true).when(tab).isIncognito();
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(tab, (res) -> {
                Assert.assertNull(res);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testCustomTabsDisabled() throws TimeoutException {
        TabImpl tab = mock(TabImpl.class);
        doReturn(true).when(tab).isCustomTab();
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(tab, (res) -> {
                Assert.assertNull(res);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testDestroyedTab() throws TimeoutException {
        TabImpl tab = mock(TabImpl.class);
        doReturn(true).when(tab).isDestroyed();
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(tab, (res) -> {
                Assert.assertNull(res);
                helper.notifyCalled();
            });
        });
        helper.waitForCallback(count);
    }

    @SmallTest
    @Test
    public void testNullTab() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(null, (res) -> {
                Assert.assertNull(res);
                helper.notifyCalled();
            });
        });
        helper.waitForCallback(count);
    }

    @SmallTest
    @Test
    public void testNoCOPTDTabDestroyed() throws ExecutionException {
        MockTab tab =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(2, false); });
        // There is no CouponPersistedTabData associated with the Tab, so it will be
        // acquired.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Tab being destroyed should result in the public API from returning null
            tab.destroy();
            CouponPersistedTabData.from(tab, (coptdRes) -> { Assert.assertNull(coptdRes); });
        });
    }

    @SmallTest
    @Test
    public void testCOPTDExistsAndThenTabDestroyed() throws ExecutionException {
        MockTab tab =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(3, false); });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab);
            // CouponPersistedTabData is 0 seconds old so it can be acquired from storage
            // and returned directly, without a refresh.
            couponPersistedTabData.setLastUpdatedMs(System.currentTimeMillis());
            save(couponPersistedTabData);
            // Verify CouponPersistedTabData is acquired from storage as expected.
            CouponPersistedTabData.from(tab, (coptdRes) -> { Assert.assertNotNull(coptdRes); });
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Remove UserData to force acquisition from storage.
            tab.getUserDataHost().removeUserData(CouponPersistedTabData.class);
            // Tab being destroyed should result in the public API from returning null
            tab.destroy();
            CouponPersistedTabData.from(tab, (coptdRes) -> { Assert.assertNull(coptdRes); });
        });
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
                .nativeFetchOAuth(any(Profile.class), anyString(), anyString(), anyString(),
                        anyString(), any(String[].class), anyString(), anyLong(), anyInt(),
                        any(Callback.class));
    }

    private static void save(CouponPersistedTabData couponPersistedTabData) {
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        couponPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
        couponPersistedTabData.save();
    }
}
