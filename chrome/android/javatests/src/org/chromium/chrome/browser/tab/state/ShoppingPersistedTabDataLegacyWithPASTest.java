// Copyright 2021 The Chromium Authors. All rights reserved.
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
import org.chromium.chrome.browser.page_annotations.PageAnnotationsService;
import org.chromium.chrome.browser.page_annotations.PageAnnotationsServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.Semaphore;
/**
 * Legacy test relating to {@link ShoppingPersistedTabData} and {@link PageAnnotationService}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class ShoppingPersistedTabDataLegacyWithPASTest {
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

    @Mock
    protected PageAnnotationsServiceFactory mServiceFactoryMock;

    @Mock
    protected PageAnnotationsService mPageAnnotationsServiceMock;

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
        doReturn(mPageAnnotationsServiceMock).when(mServiceFactoryMock).getForLastUsedProfile();
        ShoppingPersistedTabData.sPageAnnotationsServiceFactory = mServiceFactoryMock;
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_time_to_live_ms/-1000/"
            + "price_tracking_with_optimization_guide/false"})
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
            + "price_tracking_with_optimization_guide/false"})
    public void
    testShoppingPriceChangeExtraFetchAfterChange() {
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        long mLastPriceChangeTimeMs = shoppingPriceChange(tab);
        final Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                ShoppingPersistedTabDataWithPASTestUtils.verifyGetPageAnnotationsCalled(
                        mPageAnnotationsServiceMock, 3);
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
        ShoppingPersistedTabDataWithPASTestUtils.mockPageAnnotationsResponse(
                mPageAnnotationsServiceMock,
                ShoppingPersistedTabDataWithPASTestUtils.MockPageAnnotationsResponse
                        .BUYABLE_PRODUCT_INITIAL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                ShoppingPersistedTabDataWithPASTestUtils.verifyGetPageAnnotationsCalled(
                        mPageAnnotationsServiceMock, 1);
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
        ShoppingPersistedTabDataWithPASTestUtils.mockPageAnnotationsResponse(
                mPageAnnotationsServiceMock,
                ShoppingPersistedTabDataWithPASTestUtils.MockPageAnnotationsResponse
                        .BUYABLE_PRODUCT_PRICE_UPDATED);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (updatedShoppingPersistedTabData) -> {
                ShoppingPersistedTabDataWithPASTestUtils.verifyGetPageAnnotationsCalled(
                        mPageAnnotationsServiceMock, 2);
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
    @UiThreadTest
    @SmallTest
    @Test
    public void testNoRefetch() {
        final Semaphore initialSemaphore = new Semaphore(0);
        final Semaphore updateSemaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        // Mock annotations response.
        ShoppingPersistedTabDataWithPASTestUtils.mockPageAnnotationsResponse(
                mPageAnnotationsServiceMock,
                ShoppingPersistedTabDataWithPASTestUtils.MockPageAnnotationsResponse
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
        ShoppingPersistedTabDataWithPASTestUtils.verifyGetPageAnnotationsCalled(
                mPageAnnotationsServiceMock, 1);
        ShoppingPersistedTabDataWithPASTestUtils.mockPageAnnotationsResponse(
                mPageAnnotationsServiceMock,
                ShoppingPersistedTabDataWithPASTestUtils.MockPageAnnotationsResponse
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
        ShoppingPersistedTabDataWithPASTestUtils.verifyGetPageAnnotationsCalled(
                mPageAnnotationsServiceMock, 1);
    }
}