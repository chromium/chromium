// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Test relating to {@link ShoppingPersistedTabData} covering the client-side price tracking
 * implementation.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class ShoppingPersistedTabDataLegacyTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Mock
    protected OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        // Ensure native pointer is initialized
        doReturn(1L).when(mOptimizationGuideBridgeJniMock).init();
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.onDeferredStartup();
            PersistedTabDataConfiguration.setUseTestConfig(true);
        });
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_time_to_live_ms/-1000/"
            + "price_tracking_with_optimization_guide/true"})
    public void
    testShoppingPriceChange() {
        shoppingPriceChange(ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO));
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_time_to_live_ms/-1000/"
            + "price_tracking_with_optimization_guide/true"})
    public void
    testShoppingPriceChangeExtraFetchAfterChange() {
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        long mLastPriceChangeTimeMs = shoppingPriceChange(tab);
        final Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                        mOptimizationGuideBridgeJniMock, 3);
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.UPDATED_PRICE_MICROS,
                        shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.PRICE_MICROS,
                        shoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertEquals(mLastPriceChangeTimeMs,
                        shoppingPersistedTabData.getLastPriceChangeTimeMs());
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.UNITED_STATES_CURRENCY_CODE,
                        shoppingPersistedTabData.getCurrencyCode());
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    private long shoppingPriceChange(Tab tab) {
        final Semaphore initialSemaphore = new Semaphore(0);
        final Semaphore updateSemaphore = new Semaphore(0);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                        mOptimizationGuideBridgeJniMock, 1);
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.PRICE_MICROS,
                        shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.UNITED_STATES_CURRENCY_CODE,
                        shoppingPersistedTabData.getCurrencyCode());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_TRANSITIONS_OCCURRED,
                        shoppingPersistedTabData.getLastPriceChangeTimeMs());
                initialSemaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(initialSemaphore);
        long firstUpdateTime = ShoppingPersistedTabDataTestUtils.getTimeLastUpdatedOnUiThread(tab);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_PRICE_UPDATED);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (updatedShoppingPersistedTabData) -> {
                ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                        mOptimizationGuideBridgeJniMock, 2);
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.UPDATED_PRICE_MICROS,
                        updatedShoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.PRICE_MICROS,
                        updatedShoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertTrue(firstUpdateTime
                        < updatedShoppingPersistedTabData.getLastPriceChangeTimeMs());
                updateSemaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(updateSemaphore);
        return ShoppingPersistedTabDataTestUtils.getTimeLastUpdatedOnUiThread(tab);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testNoRefetch() {
        final Semaphore initialSemaphore = new Semaphore(0);
        final Semaphore updateSemaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        // Mock annotations response.
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.PRICE_MICROS,
                        shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.UNITED_STATES_CURRENCY_CODE,
                        shoppingPersistedTabData.getCurrencyCode());
                // By setting time to live to be a negative number, an update
                // will be forced in the subsequent call
                initialSemaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(initialSemaphore);
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 1);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_PRICE_UPDATED);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.PRICE_MICROS,
                        shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());

                // By setting time to live to be a negative number, an update
                // will be forced in the subsequent call
                updateSemaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(updateSemaphore);
        // PageAnnotationsService should not have been called a second time - because we haven't
        // passed the time to live
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 1);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacy() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;
        // Prices unknown is not a price drop
        Assert.assertNull(shoppingPersistedTabData.getPriceDropLegacy());

        // Same price is not a price drop
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS);
        Assert.assertNull(shoppingPersistedTabData.getPriceDropLegacy());

        // Lower -> Higher price is not a price drop
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.LOW_PRICE_MICROS);
        Assert.assertNull(shoppingPersistedTabData.getPriceDropLegacy());

        // Actual price drop (Higher -> Lower)
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.LOW_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS);
        ShoppingPersistedTabData.PriceDrop priceDrop =
                shoppingPersistedTabData.getPriceDropLegacy();
        Assert.assertEquals(ShoppingPersistedTabDataTestUtils.LOW_PRICE_FORMATTED, priceDrop.price);
        Assert.assertEquals(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_FORMATTED, priceDrop.previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterSamePrice() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $10 -> $10 is not a price drop (same price)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDropLegacy());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterNoFormattedPriceDifference() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $10.40 -> $10 (which would be displayed $10 -> $10 is not a price drop)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_400_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDropLegacy());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterAbsoluteDifferenceTooSmall() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $2 -> $1 ($1 price drop - less than $2. Not big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(1_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(2_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDropLegacy());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterPriceIncrease() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $9.33 -> $9.66 price increase is not a price drop
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_330_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(9_660_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDropLegacy());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterPercentageDropNotEnough() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $50 -> $46 (8% price drop (less than 10%) not big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(50_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(46_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDropLegacy());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterAllowedPriceDrop1() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $10 -> $7 (30% and $3 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(7_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("$7.00", shoppingPersistedTabData.getPriceDropLegacy().price);
        Assert.assertEquals("$10", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterAllowedPriceDrop2() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $15.72 -> $4.80 (70% and $10.92 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(15_720_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(4_800_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("$4.80", shoppingPersistedTabData.getPriceDropLegacy().price);
        Assert.assertEquals("$16", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterAllowedPriceDrop3() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $20 -> $10 (50% and $10 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(20_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("$10", shoppingPersistedTabData.getPriceDropLegacy().price);
        Assert.assertEquals("$20", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterAllowedPriceDrop4() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $30 -> $27 (10% and $3 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(30_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(27_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("$27", shoppingPersistedTabData.getPriceDropLegacy().price);
        Assert.assertEquals("$30", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterAllowedPriceDrop5() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $30.65 -> $25.50 (17% and $5.15 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(30_650_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(25_500_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("$26", shoppingPersistedTabData.getPriceDropLegacy().price);
        Assert.assertEquals("$31", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterAllowedPriceDrop6() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;

        // $9.65 -> $3.80 (40% and $5.85 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_650_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(3_800_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("$3.80", shoppingPersistedTabData.getPriceDropLegacy().price);
        Assert.assertEquals("$9.65", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterAllowedPriceDrop7() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;
        // $9.33 -> $0.90 (96% price drop and $8.43 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_330_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(900_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("$0.90", shoppingPersistedTabData.getPriceDropLegacy().price);
        Assert.assertEquals("$9.33", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterAllowedPriceDrop8() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;
        // $20 -> $18 (10% price drop and $2 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(20_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(18_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("$18", shoppingPersistedTabData.getPriceDropLegacy().price);
        Assert.assertEquals("$20", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyFilterAllowedPriceDrop9() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;
        // $2 -> $0 ($2 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(2_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(0L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("$0.00", shoppingPersistedTabData.getPriceDropLegacy().price);
        Assert.assertEquals("$2.00", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyGBP() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithCurrencyCode(
                        ShoppingPersistedTabDataTestUtils.TAB_ID,
                        ShoppingPersistedTabDataTestUtils.IS_INCOGNITO,
                        ShoppingPersistedTabDataTestUtils.GREAT_BRITAIN_CURRENCY_CODE);
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(15_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(9_560_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("£15", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
        Assert.assertEquals("£9.56", shoppingPersistedTabData.getPriceDropLegacy().price);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropLegacyJPY() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithCurrencyCode(
                        ShoppingPersistedTabDataTestUtils.TAB_ID,
                        ShoppingPersistedTabDataTestUtils.IS_INCOGNITO,
                        ShoppingPersistedTabDataTestUtils.JAPAN_CURRENCY_CODE);
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(3_140_000_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(287_000_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals(
                "¥3,140,000", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);
        Assert.assertEquals("¥287,000", shoppingPersistedTabData.getPriceDropLegacy().price);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testStalePriceDropUSD() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithCurrencyCode(
                        ShoppingPersistedTabDataTestUtils.TAB_ID,
                        ShoppingPersistedTabDataTestUtils.IS_INCOGNITO,
                        ShoppingPersistedTabDataTestUtils.UNITED_STATES_CURRENCY_CODE);
        shoppingPersistedTabData.mPriceDropMethod = ShoppingPersistedTabData.PriceDropMethod.LEGACY;
        // $10 -> $5 (50% and $5 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10000000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(5000000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDropLegacy());
        Assert.assertEquals("$5.00", shoppingPersistedTabData.getPriceDropLegacy().price);
        Assert.assertEquals("$10", shoppingPersistedTabData.getPriceDropLegacy().previousPrice);

        // Price drops greater than a week old are removed
        shoppingPersistedTabData.setLastPriceChangeTimeMsForTesting(
                System.currentTimeMillis() - TimeUnit.DAYS.toMillis(8));
        Assert.assertNull(shoppingPersistedTabData.getPriceDropLegacy());
    }
}
