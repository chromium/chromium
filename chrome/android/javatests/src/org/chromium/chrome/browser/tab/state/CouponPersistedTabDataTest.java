// Copyright 2022 The Chromium Authors
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
import org.chromium.base.test.UiThreadTest;
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
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Test relating to {@link CouponPersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class CouponPersistedTabDataTest {
    @Rule
    public JniMocker mMocker = new JniMocker();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Mock
    private EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    private Profile mProfileMock;

    @Mock
    protected NavigationHandle mNavigationHandle;

    private static final String SERIALIZE_DESERIALIZE_NAME = "$25 Off";
    private static final String SERIALIZE_DESERIALIZE_CODE = "DISCOUNT25";
    private static final String SERIALIZE_DESERIALIZE_CURRENCY_CODE = "USD";
    private static final CouponPersistedTabData.Coupon
            .DiscountType SERIALIZE_DESERIALIZE_DISCOUNT_TYPE =
            CouponPersistedTabData.Coupon.DiscountType.AMOUNT_OFF;
    private static final long SERIALIZE_DESERIALIZE_DISCOUNT_UNITS = 25;

    private static final String MOCK_ENDPOINT_RESPONSE_STRING_AMOUNT =
            "{\"discounts\":[{\"amountOff\":{\"currencyCode\":\"USD\",\"units\":\"20\"},"
            + "\"freeListingDiscountInfo\":{\"promoDocid\":\"16137933697037590414\","
            + "\"longTitle\":\"$20 off on dining set\",\"couponCode\":\"DIN20\"},"
            + "\"isMerchantLevelDiscount\":false}]}";
    private static final String EXPECTED_NAME_GENERAL_CASE_AMOUNT = "$20 off on dining set";
    private static final String EXPECTED_CODE_GENERAL_CASE_AMOUNT = "DIN20";
    private static final String EXPECTED_TYPE_GENERAL_CASE_AMOUNT = "USD";
    private static final long EXPECTED_UNITS_GENERAL_CASE_AMOUNT = 20;
    private static final String EXPECTED_ANNOTATION_GENERAL_CASE_AMOUNT = "$20 Off";

    private static final String MOCK_ENDPOINT_RESPONSE_STRING_PERCENT =
            "{\"discounts\":[{\"percentOff\":30,\"freeListingDiscountInfo\":"
            + "{\"promoDocid\":\"11828199513928538057\",\"longTitle\":"
            + "\"Save 30% on MyPillow Bath Robes w/ Promo Code\",\"couponCode\":\"G46\"},"
            + "\"isMerchantLevelDiscount\":false}]}";
    private static final String EXPECTED_NAME_GENERAL_CASE_PERCENT =
            "Save 30% on MyPillow Bath Robes w/ Promo Code";
    private static final String EXPECTED_CODE_GENERAL_CASE_PERCENT = "G46";
    private static final String EXPECTED_ANNOTATION_GENERAL_CASE_PERCENT = "30% Off";

    private static final String MOCK_ENDPOINT_RESPONSE_STRING_AMOUNT_NO_DISCOUNT_INFO =
            "{\"discounts\":[{\"amountOff\":{\"currencyCode\":\"\",\"units\":\"20\"},"
            + "\"freeListingDiscountInfo\":{\"promoDocid\":\"16137933697037590414\","
            + "\"longTitle\":\"$20 off on dining set\",\"couponCode\":\"DIN20\"},"
            + "\"isMerchantLevelDiscount\":false}]}";
    private static final String EXPECTED_ANNOTATION_NO_DISCOUNT_INFO_CASE = "Coupon Available";

    private static final String EMPTY_ENDPOINT_RESPONSE = "";
    private static final String MALFORMED_ENDPOINT_RESPONSE = "malformed response";
    private static final GURL VALID_URL_1 = new GURL("https://foo.com");
    private static final GURL VALID_URL_2 = new GURL("https://bar.com");
    private static final GURL INVALID_URL = new GURL("httpz://foo.com");

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

    @UiThreadTest
    @SmallTest
    @Test
    public void testSerializeDeserialize() {
        Tab tab = new MockTab(1, false);
        CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab,
                new CouponPersistedTabData.Coupon(SERIALIZE_DESERIALIZE_NAME,
                        SERIALIZE_DESERIALIZE_CODE, SERIALIZE_DESERIALIZE_CURRENCY_CODE,
                        SERIALIZE_DESERIALIZE_DISCOUNT_UNITS, SERIALIZE_DESERIALIZE_DISCOUNT_TYPE));
        ByteBuffer serialized = couponPersistedTabData.getSerializer().get();
        CouponPersistedTabData deserialized = new CouponPersistedTabData(tab);
        Assert.assertTrue(deserialized.deserialize(serialized));
        Assert.assertEquals(SERIALIZE_DESERIALIZE_NAME, deserialized.getCoupon().couponName);
        Assert.assertEquals(SERIALIZE_DESERIALIZE_CODE, deserialized.getCoupon().promoCode);
        Assert.assertEquals(
                SERIALIZE_DESERIALIZE_CURRENCY_CODE, deserialized.getCoupon().currencyCode);
        Assert.assertEquals(
                SERIALIZE_DESERIALIZE_DISCOUNT_UNITS, deserialized.getCoupon().discountUnits);
        Assert.assertEquals(
                SERIALIZE_DESERIALIZE_DISCOUNT_TYPE, deserialized.getCoupon().discountType);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSerializeDeserializeNull() {
        Tab tab = new MockTab(1, false);
        CouponPersistedTabData deserialized = new CouponPersistedTabData(tab, null);
        Assert.assertFalse(deserialized.deserialize(null));
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSerializeDeserializeNoRemainingBytes() {
        Tab tab = new MockTab(1, false);
        CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab, null);
        ByteBuffer serialized = couponPersistedTabData.getSerializer().get();
        CouponPersistedTabData deserialized = new CouponPersistedTabData(tab);
        Assert.assertFalse(serialized.hasRemaining());
        Assert.assertFalse(deserialized.deserialize(serialized));
    }

    @SmallTest
    @Test
    public void testCOPTDUponSuccessfulResponsePercentOff()
            throws TimeoutException, ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        mockEndpointResponse(MOCK_ENDPOINT_RESPONSE_STRING_PERCENT);
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(tab, (res) -> {
                Assert.assertEquals(EXPECTED_NAME_GENERAL_CASE_PERCENT, res.getCoupon().couponName);
                Assert.assertEquals(EXPECTED_CODE_GENERAL_CASE_PERCENT, res.getCoupon().promoCode);
                Assert.assertEquals(
                        EXPECTED_ANNOTATION_GENERAL_CASE_PERCENT, res.getCouponAnnotationText());
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testCOPTDUponSuccessfulResponseAmountOff()
            throws TimeoutException, ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        mockEndpointResponse(MOCK_ENDPOINT_RESPONSE_STRING_AMOUNT);
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(tab, (res) -> {
                Assert.assertEquals(EXPECTED_NAME_GENERAL_CASE_AMOUNT, res.getCoupon().couponName);
                Assert.assertEquals(EXPECTED_CODE_GENERAL_CASE_AMOUNT, res.getCoupon().promoCode);
                Assert.assertEquals(
                        EXPECTED_ANNOTATION_GENERAL_CASE_AMOUNT, res.getCouponAnnotationText());
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testCOPTDUponSuccessfulResponseAmountOffNoDiscountInfo()
            throws TimeoutException, ExecutionException {
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        mockEndpointResponse(MOCK_ENDPOINT_RESPONSE_STRING_AMOUNT_NO_DISCOUNT_INFO);
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(tab, (res) -> {
                Assert.assertEquals(EXPECTED_NAME_GENERAL_CASE_AMOUNT, res.getCoupon().couponName);
                Assert.assertEquals(EXPECTED_CODE_GENERAL_CASE_AMOUNT, res.getCoupon().promoCode);
                Assert.assertEquals(
                        EXPECTED_ANNOTATION_NO_DISCOUNT_INFO_CASE, res.getCouponAnnotationText());
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

    @UiThreadTest
    @SmallTest
    @Test
    public void testResetCOPTDStartNavigationNewUrl() {
        MockTab tab = new MockTab(1, false);
        mockEndpointResponse(EMPTY_ENDPOINT_RESPONSE);
        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        for (boolean isSameDocument : new boolean[] {false, true}) {
            CouponPersistedTabData.Coupon coupon = new CouponPersistedTabData.Coupon(
                    EXPECTED_NAME_GENERAL_CASE_AMOUNT, EXPECTED_NAME_GENERAL_CASE_AMOUNT,
                    EXPECTED_TYPE_GENERAL_CASE_AMOUNT, EXPECTED_UNITS_GENERAL_CASE_AMOUNT,
                    SERIALIZE_DESERIALIZE_DISCOUNT_TYPE);
            CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab, coupon);
            Assert.assertNotNull(couponPersistedTabData.getCoupon());
            doReturn(isSameDocument).when(navigationHandle).isSameDocument();
            couponPersistedTabData.getUrlUpdatedObserverForTesting()
                    .onDidStartNavigationInPrimaryMainFrame(tab, navigationHandle);
            if (!isSameDocument) {
                Assert.assertNull(couponPersistedTabData.getCoupon());
            } else {
                Assert.assertNotNull(couponPersistedTabData.getCoupon());
            }
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testResetCOPTDFinishNavigationValidURL() {
        MockTab tab = new MockTab(1, false);
        mockEndpointResponse(EMPTY_ENDPOINT_RESPONSE);
        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        doReturn(VALID_URL_1).when(navigationHandle).getUrl();
        CouponPersistedTabData.Coupon coupon =
                new CouponPersistedTabData.Coupon(EXPECTED_NAME_GENERAL_CASE_AMOUNT,
                        EXPECTED_NAME_GENERAL_CASE_AMOUNT, EXPECTED_TYPE_GENERAL_CASE_AMOUNT,
                        EXPECTED_UNITS_GENERAL_CASE_AMOUNT, SERIALIZE_DESERIALIZE_DISCOUNT_TYPE);
        CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab, coupon);
        Assert.assertNotNull(couponPersistedTabData.getCoupon());
        couponPersistedTabData.getUrlUpdatedObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(tab, navigationHandle);
        Assert.assertNull(couponPersistedTabData.getCoupon());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testDontResetCOPTDFinishNavigationInvalidURL() {
        MockTab tab = new MockTab(1, false);
        mockEndpointResponse(EMPTY_ENDPOINT_RESPONSE);
        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        doReturn(INVALID_URL).when(navigationHandle).getUrl();
        CouponPersistedTabData.Coupon coupon =
                new CouponPersistedTabData.Coupon(EXPECTED_NAME_GENERAL_CASE_AMOUNT,
                        EXPECTED_NAME_GENERAL_CASE_AMOUNT, EXPECTED_TYPE_GENERAL_CASE_AMOUNT,
                        EXPECTED_UNITS_GENERAL_CASE_AMOUNT, SERIALIZE_DESERIALIZE_DISCOUNT_TYPE);
        CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab, coupon);
        Assert.assertNotNull(couponPersistedTabData.getCoupon());
        couponPersistedTabData.getUrlUpdatedObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(tab, navigationHandle);
        Assert.assertNotNull(couponPersistedTabData.getCoupon());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testDontResetCOPTDOnRefresh() {
        MockTab tab = new MockTab(1, false);
        mockEndpointResponse(EMPTY_ENDPOINT_RESPONSE);
        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        doReturn(true).when(navigationHandle).isInPrimaryMainFrame();
        doReturn(false).when(navigationHandle).isSameDocument();
        tab.setGurlOverrideForTesting(VALID_URL_1);
        doReturn(VALID_URL_1).when(navigationHandle).getUrl();
        CouponPersistedTabData.Coupon coupon =
                new CouponPersistedTabData.Coupon(EXPECTED_NAME_GENERAL_CASE_AMOUNT,
                        EXPECTED_NAME_GENERAL_CASE_AMOUNT, EXPECTED_TYPE_GENERAL_CASE_AMOUNT,
                        EXPECTED_UNITS_GENERAL_CASE_AMOUNT, SERIALIZE_DESERIALIZE_DISCOUNT_TYPE);
        CouponPersistedTabData couponPersistedTabData = new CouponPersistedTabData(tab, coupon);
        Assert.assertNotNull(couponPersistedTabData.getCoupon());
        couponPersistedTabData.getUrlUpdatedObserverForTesting()
                .onDidStartNavigationInPrimaryMainFrame(tab, navigationHandle);
        Assert.assertNotNull(couponPersistedTabData.getCoupon());
        doReturn(VALID_URL_2).when(navigationHandle).getUrl();
        couponPersistedTabData.getUrlUpdatedObserverForTesting()
                .onDidStartNavigationInPrimaryMainFrame(tab, navigationHandle);
        Assert.assertNull(couponPersistedTabData.getCoupon());
    }

    @SmallTest
    @Test
    public void testCOPTDSavingEnabledUponSuccessfulResponse()
            throws ExecutionException, TimeoutException {
        MockTab tab =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(1, false); });
        mockEndpointResponse(MOCK_ENDPOINT_RESPONSE_STRING_PERCENT);

        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CouponPersistedTabData.from(tab, (couponPersistedTabData) -> {
                Assert.assertTrue(couponPersistedTabData.mIsTabSaveEnabledSupplier.get());
                helper.notifyCalled();
            });
        });
        helper.waitForCallback(count);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testCOPTDPrefetchingSuccessfulResponse() {
        MockTab tab = new MockTab(1, false);
        mockEndpointResponse(MOCK_ENDPOINT_RESPONSE_STRING_AMOUNT);
        tab.setIsInitialized(true);
        tab.setGurlOverrideForTesting(VALID_URL_1);

        CouponPersistedTabData coptd = new CouponPersistedTabData(tab);
        coptd.prefetchOnNewNavigation(tab);

        Assert.assertEquals(EXPECTED_NAME_GENERAL_CASE_AMOUNT, coptd.getCoupon().couponName);
        Assert.assertEquals(EXPECTED_CODE_GENERAL_CASE_AMOUNT, coptd.getCoupon().promoCode);
        Assert.assertEquals(
                EXPECTED_ANNOTATION_GENERAL_CASE_AMOUNT, coptd.getCouponAnnotationText());
        Assert.assertTrue(coptd.mIsTabSaveEnabledSupplier.get());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testCOPTDPrefetchingNoResponse() {
        MockTab tab = new MockTab(1, false);
        mockEndpointResponse(EMPTY_ENDPOINT_RESPONSE);
        tab.setIsInitialized(true);
        tab.setGurlOverrideForTesting(VALID_URL_1);

        CouponPersistedTabData coptd = new CouponPersistedTabData(tab);
        coptd.prefetchOnNewNavigation(tab);

        Assert.assertNull(coptd.getCoupon());
        Assert.assertFalse(coptd.mIsTabSaveEnabledSupplier.get());
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