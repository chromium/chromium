// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.support.test.filters.SmallTest;

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
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge.OptimizationGuideCallback;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test relating to {@link ShoppingPersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ShoppingPersistedTabDataTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final int TAB_ID = 1;
    private static final boolean IS_INCOGNITO = false;

    @Mock
    EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    // Tracks if the endpoint fetcher has been called once or not
    private boolean mCalledOnce;

    private static final long PRICE_MICROS = 123456789012345L;
    private static final long UPDATED_PRICE_MICROS = 287000000L;
    private static final long HIGH_PRICE_MICROS = 141000000L;
    private static final long LOW_PRICE_MICROS = 100000000L;
    private static final String HIGH_PRICE_FORMATTED = "$141";
    private static final String LOW_PRICE_FORMATTED = "$100";
    private static final String UNITED_STATES_CURRENCY_CODE = "USD";
    private static final String GREAT_BRITAIN_CURRENCY_CODE = "GBP";
    private static final String JAPAN_CURRENCY_CODE = "JPY";

    private static final String EMPTY_PRICE = "";
    private static final String ENDPOINT_RESPONSE_INITIAL =
            "{\"annotations\":[{\"type\":\"DOCUMENT_INTENT\",\"documentIntent\":"
            + "{\"intent\":\"UNKNOWN\"}},{\"type\":\"BUYABLE_PRODUCT\",\"buyableProduct\":"
            + "{\"title\":\"foo title\",\"imageUrl\":\"https://images.com?q=1234\","
            + "\"currentPrice\":{\"currencyCode\":\"USD\",\"amountMicros\":\"123456789012345\"},"
            + "\"referenceType\":\"MAIN_PRODUCT\"}}]}";

    private static final String ENDPOINT_RESPONSE_UPDATE =
            "{\"annotations\":[{\"type\":\"DOCUMENT_INTENT\",\"documentIntent\":"
            + "{\"intent\":\"UNKNOWN\"}},{\"type\":\"BUYABLE_PRODUCT\",\"buyableProduct\":"
            + "{\"title\":\"foo title\",\"imageUrl\":\"https://images.com?q=1234\","
            + "\"currentPrice\":{\"currencyCode\":\"USD\",\"amountMicros\":\"287000000\"},"
            + "\"referenceType\":\"MAIN_PRODUCT\"}}]}";

    private static final String EMPTY_RESPONSE = "{}";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        mMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        // Ensure native pointer is initialized
        doReturn(1L).when(mOptimizationGuideBridgeJniMock).init();
        mockOptimizationGuideResponse(OptimizationGuideDecision.TRUE);
        PersistedTabDataConfiguration.setUseTestConfig(true);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testShoppingProto() {
        Tab tab = new MockTab(TAB_ID, IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
        shoppingPersistedTabData.setPriceMicros(PRICE_MICROS, null);
        byte[] serialized = shoppingPersistedTabData.serialize();
        ShoppingPersistedTabData deserialized = new ShoppingPersistedTabData(tab);
        deserialized.deserialize(serialized);
        Assert.assertEquals(PRICE_MICROS, deserialized.getPriceMicros());
        Assert.assertEquals(
                ShoppingPersistedTabData.NO_PRICE_KNOWN, deserialized.getPreviousPriceMicros());
    }

    @SmallTest
    @Test
    public void testShoppingPriceChange() {
        shoppingPriceChange(createTabOnUiThread(TAB_ID, IS_INCOGNITO));
    }

    @SmallTest
    @Test
    public void testShoppingPriceChangeExtraFetchAfterChange() {
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        long mLastPriceChangeTimeMs = shoppingPriceChange(tab);
        final Semaphore updateTtlSemaphore = new Semaphore(0);
        // Set TimeToLive such that a refetch will be forced
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PersistedTabData.from(tab, ShoppingPersistedTabData.class).setTimeToLiveMs(-1000);
            updateTtlSemaphore.release();
        });
        acquireSemaphore(updateTtlSemaphore);
        final Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                verifyEndpointFetcherCalled(3);
                Assert.assertEquals(
                        UPDATED_PRICE_MICROS, shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(
                        PRICE_MICROS, shoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertEquals(mLastPriceChangeTimeMs,
                        shoppingPersistedTabData.getLastPriceChangeTimeMs());
                Assert.assertEquals(
                        UNITED_STATES_CURRENCY_CODE, shoppingPersistedTabData.getCurrencyCode());
                semaphore.release();
            });
        });
        acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    public void testShoppingBloomFilterNotShoppingWebsite() {
        mockEndpointResponse(ENDPOINT_RESPONSE_INITIAL);
        mockOptimizationGuideResponse(OptimizationGuideDecision.FALSE);
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(
                    tab, (shoppingPersistedTabData) -> { semaphore.release(); });
        });
        acquireSemaphore(semaphore);
        verifyEndpointFetcherCalled(0);
    }

    @SmallTest
    @Test
    public void testShoppingBloomFilterShoppingWebsite() {
        for (@OptimizationGuideDecision int decision :
                new int[] {OptimizationGuideDecision.TRUE, OptimizationGuideDecision.UNKNOWN}) {
            mockEndpointResponse(ENDPOINT_RESPONSE_INITIAL);
            mockOptimizationGuideResponse(decision);
            Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
            Semaphore semaphore = new Semaphore(0);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                ShoppingPersistedTabData.from(
                        tab, (shoppingPersistedTabData) -> { semaphore.release(); });
            });
            acquireSemaphore(semaphore);
            verifyEndpointFetcherCalled(1);
        }
    }

    private long shoppingPriceChange(Tab tab) {
        final Semaphore initialSemaphore = new Semaphore(0);
        final Semaphore updateSemaphore = new Semaphore(0);
        mockEndpointResponse(ENDPOINT_RESPONSE_INITIAL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                verifyEndpointFetcherCalled(1);
                Assert.assertEquals(PRICE_MICROS, shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(
                        UNITED_STATES_CURRENCY_CODE, shoppingPersistedTabData.getCurrencyCode());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_TRANSITIONS_OCCURRED,
                        shoppingPersistedTabData.getLastPriceChangeTimeMs());
                // By setting time to live to be a negative number, an update
                // will be forced in the subsequent call
                shoppingPersistedTabData.setTimeToLiveMs(-1000);
                initialSemaphore.release();
            });
        });
        acquireSemaphore(initialSemaphore);
        long firstUpdateTime = getTimeLastUpdatedOnUiThread(tab);
        mockEndpointResponse(ENDPOINT_RESPONSE_UPDATE);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (updatedShoppingPersistedTabData) -> {
                verifyEndpointFetcherCalled(2);
                Assert.assertEquals(
                        UPDATED_PRICE_MICROS, updatedShoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(
                        PRICE_MICROS, updatedShoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertTrue(firstUpdateTime
                        < updatedShoppingPersistedTabData.getLastPriceChangeTimeMs());
                updateSemaphore.release();
            });
        });
        acquireSemaphore(updateSemaphore);
        return getTimeLastUpdatedOnUiThread(tab);
    }

    @SmallTest
    @Test
    public void testNoRefetch() {
        final Semaphore initialSemaphore = new Semaphore(0);
        final Semaphore updateSemaphore = new Semaphore(0);
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        mockEndpointResponse(ENDPOINT_RESPONSE_INITIAL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertEquals(PRICE_MICROS, shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertEquals(
                        UNITED_STATES_CURRENCY_CODE, shoppingPersistedTabData.getCurrencyCode());
                // By setting time to live to be a negative number, an update
                // will be forced in the subsequent call
                initialSemaphore.release();
            });
        });
        acquireSemaphore(initialSemaphore);
        verifyEndpointFetcherCalled(1);
        mockEndpointResponse(ENDPOINT_RESPONSE_UPDATE);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertEquals(PRICE_MICROS, shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());

                // By setting time to live to be a negative number, an update
                // will be forced in the subsequent call
                updateSemaphore.release();
            });
        });
        acquireSemaphore(updateSemaphore);
        // EndpointFetcher should not have been called a second time - because we haven't passed the
        // time to live
        verifyEndpointFetcherCalled(1);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDrop() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();
        // Prices unknown is not a price drop
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Same price is not a price drop
        shoppingPersistedTabData.setPriceMicros(HIGH_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(HIGH_PRICE_MICROS);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Lower -> Higher price is not a price drop
        shoppingPersistedTabData.setPriceMicros(HIGH_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(LOW_PRICE_MICROS);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Actual price drop (Higher -> Lower)
        shoppingPersistedTabData.setPriceMicros(LOW_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(HIGH_PRICE_MICROS);
        ShoppingPersistedTabData.PriceDrop priceDrop = shoppingPersistedTabData.getPriceDrop();
        Assert.assertEquals(LOW_PRICE_FORMATTED, priceDrop.price);
        Assert.assertEquals(HIGH_PRICE_FORMATTED, priceDrop.previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterSamePrice() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $10 -> $10 is not a price drop (same price)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterNoFormattedPriceDifference() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $10.40 -> $10 (which would be displayed $10 -> $10 is not a price drop)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_400_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAbsoluteDifferenceTooSmall() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $2 -> $1 ($1 price drop - less than $2. Not big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(1_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(2_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterPriceIncrease() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $9.33 -> $9.66 price increase is not a price drop
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_330_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(9_660_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterPercentageDropNotEnough() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $50 -> $46 (8% price drop (less than 10%) not big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(50_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(46_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    private ShoppingPersistedTabData createShoppingPersistedTabDataWithDefaults() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));
        shoppingPersistedTabData.setCurrencyCode(UNITED_STATES_CURRENCY_CODE);
        return shoppingPersistedTabData;
    }

    private ShoppingPersistedTabData createShoppingPersistedTabDataWithCurrencyCode(
            int tabId, boolean isIncognito, String currencyCode) {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(tabId, isIncognito));
        shoppingPersistedTabData.setCurrencyCode(currencyCode);
        return shoppingPersistedTabData;
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop1() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $10 -> $7 (30% and $3 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(7_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$7.00", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$10", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop2() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $15.72 -> $4.80 (70% and $10.92 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(15_720_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(4_800_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$4.80", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$16", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop3() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $20 -> $10 (50% and $10 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(20_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$10", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$20", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop4() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $30 -> $27 (10% and $3 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(30_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(27_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$27", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$30", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop5() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $30.65 -> $25.50 (17% and $5.15 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(30_650_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(25_500_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$26", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$31", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop6() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $9.65 -> $3.80 (40% and $5.85 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_650_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(3_800_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$3.80", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$9.65", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop7() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $9.33 -> $0.90 (96% price drop and $8.43 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_330_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(900_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$0.90", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$9.33", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop8() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $20 -> $18 (10% price drop and $2 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(20_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(18_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$18", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$20", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop9() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithDefaults();

        // $2 -> $0 ($2 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(2_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(0L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$0.00", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$2.00", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropGBP() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithCurrencyCode(
                        TAB_ID, IS_INCOGNITO, GREAT_BRITAIN_CURRENCY_CODE);

        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(15_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(9_560_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("£15", shoppingPersistedTabData.getPriceDrop().previousPrice);
        Assert.assertEquals("£9.56", shoppingPersistedTabData.getPriceDrop().price);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropJPY() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithCurrencyCode(
                        TAB_ID, IS_INCOGNITO, JAPAN_CURRENCY_CODE);

        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(3_140_000_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(287_000_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("¥3,140,000", shoppingPersistedTabData.getPriceDrop().previousPrice);
        Assert.assertEquals("¥287,000", shoppingPersistedTabData.getPriceDrop().price);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testStalePriceDropUSD() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                createShoppingPersistedTabDataWithCurrencyCode(
                        TAB_ID, IS_INCOGNITO, UNITED_STATES_CURRENCY_CODE);
        // $10 -> $5 (50% and $5 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10000000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(5000000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$5.00", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$10", shoppingPersistedTabData.getPriceDrop().previousPrice);

        // Price drops greater than a week old are removed
        shoppingPersistedTabData.setLastPriceChangeTimeMsForTesting(
                System.currentTimeMillis() - TimeUnit.DAYS.toMillis(8));
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testNewUrl() {
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        Assert.assertFalse(shoppingPersistedTabData.mIsTabSaveEnabledSupplier.get());
        shoppingPersistedTabData.mIsTabSaveEnabledSupplier.set(true);
        shoppingPersistedTabData.mUrlUpdatedObserver.onUrlUpdated(tab);
        Assert.assertFalse(shoppingPersistedTabData.mIsTabSaveEnabledSupplier.get());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSPTDSavingEnabledUponSuccessfulResponse() {
        final Semaphore semaphore = new Semaphore(0);
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        mockEndpointResponse(ENDPOINT_RESPONSE_INITIAL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertTrue(shoppingPersistedTabData.mIsTabSaveEnabledSupplier.get());
                semaphore.release();
            });
        });
        acquireSemaphore(semaphore);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSPTDNullUponUnsuccessfulResponse() {
        final Semaphore semaphore = new Semaphore(0);
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        mockEndpointResponse(EMPTY_RESPONSE);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertNull(shoppingPersistedTabData);
                semaphore.release();
            });
        });
        acquireSemaphore(semaphore);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSerializationBug() {
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        shoppingPersistedTabData.setPriceMicros(42_000_000L, null);
        byte[] serialized = shoppingPersistedTabData.serialize();
        PersistedTabDataConfiguration config = PersistedTabDataConfiguration.get(
                ShoppingPersistedTabData.class, tab.isIncognito());
        ShoppingPersistedTabData deserialized =
                new ShoppingPersistedTabData(tab, serialized, config.getStorage(), config.getId());
        Assert.assertEquals(42_000_000L, deserialized.getPriceMicros());
    }

    private void verifyEndpointFetcherCalled(int numTimes) {
        verify(mEndpointFetcherJniMock, times(numTimes))
                .nativeFetchChromeAPIKey(any(Profile.class), anyString(), anyString(), anyString(),
                        anyString(), anyLong(), any(String[].class), any(Callback.class));
    }

    private static Tab createTabOnUiThread(int tabId, boolean isIncognito) {
        AtomicReference<Tab> res = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { res.set(MockTab.createAndInitialize(TAB_ID, IS_INCOGNITO)); });
        return res.get();
    }

    private static long getTimeLastUpdatedOnUiThread(Tab tab) {
        AtomicReference<Long> res = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            res.set(PersistedTabData.from(tab, ShoppingPersistedTabData.class)
                            .getLastPriceChangeTimeMs());
        });
        return res.get();
    }

    private void mockEndpointResponse(String response) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[7];
                callback.onResult(new EndpointResponse(response));
                return null;
            }
        })
                .when(mEndpointFetcherJniMock)
                .nativeFetchChromeAPIKey(any(Profile.class), anyString(), anyString(), anyString(),
                        anyString(), anyLong(), any(String[].class), any(Callback.class));
    }

    private static void acquireSemaphore(Semaphore semaphore) {
        try {
            semaphore.acquire();
        } catch (InterruptedException e) {
            // Throw Runtime exception to make catching InterruptedException unnecessary
            throw new RuntimeException(e);
        }
    }

    private void mockOptimizationGuideResponse(@OptimizationGuideDecision int decision) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                OptimizationGuideCallback callback =
                        (OptimizationGuideCallback) invocation.getArguments()[3];
                callback.onOptimizationGuideDecision(decision, null);
                return null;
            }
        })
                .when(mOptimizationGuideBridgeJniMock)
                .canApplyOptimization(
                        anyLong(), any(GURL.class), anyInt(), any(OptimizationGuideCallback.class));
    }
}
