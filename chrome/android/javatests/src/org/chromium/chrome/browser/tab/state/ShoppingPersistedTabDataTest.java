// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.Mockito.doReturn;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
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
 * Test relating to {@link ShoppingPersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class ShoppingPersistedTabDataTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Mock
    protected EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    protected OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    @Mock
    protected Profile mProfileMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        mMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        // Ensure native pointer is initialized
        doReturn(1L).when(mOptimizationGuideBridgeJniMock).init();
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
        PersistedTabDataConfiguration.setUseTestConfig(true);
        Profile.setLastUsedProfileForTesting(mProfileMock);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testShoppingProto() {
        Tab tab = new MockTab(ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.PRICE_MICROS, null);
        byte[] serialized = shoppingPersistedTabData.getSerializeSupplier().get();
        ShoppingPersistedTabData deserialized = new ShoppingPersistedTabData(tab);
        deserialized.deserialize(serialized);
        Assert.assertEquals(
                ShoppingPersistedTabDataTestUtils.PRICE_MICROS, deserialized.getPriceMicros());
        Assert.assertEquals(
                ShoppingPersistedTabData.NO_PRICE_KNOWN, deserialized.getPreviousPriceMicros());
    }

    @SmallTest
    @Test
    public void testShoppingBloomFilterNotShoppingWebsite() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.FALSE, null);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(
                    tab, (shoppingPersistedTabData) -> { semaphore.release(); });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 0);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testShoppingBloomFilterShoppingWebsite() {
        for (@OptimizationGuideDecision int decision :
                new int[] {OptimizationGuideDecision.TRUE, OptimizationGuideDecision.UNKNOWN}) {
            ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                    mOptimizationGuideBridgeJniMock,
                    HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                    ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                            .BUYABLE_PRODUCT_INITIAL);
            ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                    mOptimizationGuideBridgeJniMock,
                    HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(), decision,
                    null);
            Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                    ShoppingPersistedTabDataTestUtils.TAB_ID,
                    ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
            Semaphore semaphore = new Semaphore(0);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                ShoppingPersistedTabData.from(
                        tab, (shoppingPersistedTabData) -> { semaphore.release(); });
            });
            ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
            ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                    mOptimizationGuideBridgeJniMock, 1);
        }
    }

    @SmallTest
    @Test
    public void testStaleTab() {
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CriticalPersistedTabData.from(tab).setTimestampMillis(
                    System.currentTimeMillis() - TimeUnit.DAYS.toMillis(100));
            ShoppingPersistedTabData.from(
                    tab, (shoppingPersistedTabData) -> { semaphore.release(); });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 0);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_stale_tab_threshold_seconds/86400/"
            + "price_tracking_with_optimization_guide/true"})
    public void
    test2DayTabWithStaleOverride1day() {
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CriticalPersistedTabData.from(tab).setTimestampMillis(
                    System.currentTimeMillis() - TimeUnit.DAYS.toMillis(2));
            ShoppingPersistedTabData.from(
                    tab, (shoppingPersistedTabData) -> { semaphore.release(); });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 0);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_stale_tab_threshold_seconds/86400/"
            + "price_tracking_with_optimization_guide/true"})
    public void
    testHalfDayTabWithStaleOverride1day() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CriticalPersistedTabData.from(tab).setTimestampMillis(
                    System.currentTimeMillis() - TimeUnit.HOURS.toMillis(12));
            ShoppingPersistedTabData.from(
                    tab, (shoppingPersistedTabData) -> { semaphore.release(); });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 1);
    }

    @UiThreadTest
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
        // EndpointFetcher should not have been called a second time - because we haven't passed the
        // time to live
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 1);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDrop() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        // Prices unknown is not a price drop
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Same price is not a price drop
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Lower -> Higher price is not a price drop
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.LOW_PRICE_MICROS);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Actual price drop (Higher -> Lower)
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.LOW_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS);
        ShoppingPersistedTabData.PriceDrop priceDrop = shoppingPersistedTabData.getPriceDrop();
        Assert.assertEquals(ShoppingPersistedTabDataTestUtils.LOW_PRICE_FORMATTED, priceDrop.price);
        Assert.assertEquals(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_FORMATTED, priceDrop.previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterSamePrice() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();

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
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();

        // $10.40 -> $10 (which would be displayed $10 -> $10 is not a price drop)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_400_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterPriceIncrease() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();

        // $9.33 -> $9.66 price increase is not a price drop
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_330_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(9_660_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropGBP() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithCurrencyCode(
                        ShoppingPersistedTabDataTestUtils.TAB_ID,
                        ShoppingPersistedTabDataTestUtils.IS_INCOGNITO,
                        ShoppingPersistedTabDataTestUtils.GREAT_BRITAIN_CURRENCY_CODE);
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
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithCurrencyCode(
                        ShoppingPersistedTabDataTestUtils.TAB_ID,
                        ShoppingPersistedTabDataTestUtils.IS_INCOGNITO,
                        ShoppingPersistedTabDataTestUtils.JAPAN_CURRENCY_CODE);
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
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithCurrencyCode(
                        ShoppingPersistedTabDataTestUtils.TAB_ID,
                        ShoppingPersistedTabDataTestUtils.IS_INCOGNITO,
                        ShoppingPersistedTabDataTestUtils.UNITED_STATES_CURRENCY_CODE);
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
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        Assert.assertFalse(shoppingPersistedTabData.mIsTabSaveEnabledSupplier.get());
        shoppingPersistedTabData.mIsTabSaveEnabledSupplier.set(true);
        shoppingPersistedTabData.mUrlUpdatedObserver.onUrlUpdated(tab);
        Assert.assertFalse(shoppingPersistedTabData.mIsTabSaveEnabledSupplier.get());
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testSPTDSavingEnabledUponSuccessfulResponse() {
        final Semaphore semaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertTrue(shoppingPersistedTabData.mIsTabSaveEnabledSupplier.get());
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testSPTDSavingEnabledUponSuccessfulProductUpdateResponse() {
        final Semaphore semaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertTrue(shoppingPersistedTabData.mIsTabSaveEnabledSupplier.get());
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSPTDNullUponUnsuccessfulResponse() {
        final Semaphore semaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse.BUYABLE_PRODUCT_EMPTY);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertNull(shoppingPersistedTabData);
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSPTDNullOptimizationGuideFalse() {
        final Semaphore semaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse.NONE);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertNull(shoppingPersistedTabData);
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSerializationBug() {
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        shoppingPersistedTabData.setPriceMicros(42_000_000L, null);
        byte[] serialized = shoppingPersistedTabData.getSerializeSupplier().get();
        PersistedTabDataConfiguration config = PersistedTabDataConfiguration.get(
                ShoppingPersistedTabData.class, tab.isIncognito());
        ShoppingPersistedTabData deserialized =
                new ShoppingPersistedTabData(tab, serialized, config.getStorage(), config.getId());
        Assert.assertEquals(42_000_000L, deserialized.getPriceMicros());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSerializeWithOfferId() {
        Tab tab = new MockTab(ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
        shoppingPersistedTabData.setMainOfferId(ShoppingPersistedTabDataTestUtils.FAKE_OFFER_ID);

        byte[] serialized = shoppingPersistedTabData.getSerializeSupplier().get();
        ShoppingPersistedTabData deserialized = new ShoppingPersistedTabData(tab);
        deserialized.deserialize(serialized);
        Assert.assertEquals(
                ShoppingPersistedTabDataTestUtils.FAKE_OFFER_ID, deserialized.getMainOfferId());
    }
}