// Copyright 2021 The Chromium Authors
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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.Semaphore;

/** Tests the deferred startup of {@link ShoppingPersistedTabData} */
@RunWith(BaseJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
public class ShoppingPersistedTabDataDeferredStartupTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule public JniMocker mMocker = new JniMocker();

    @Rule public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Mock protected OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    @Mock protected Profile mProfileMock;

    @Mock protected NavigationHandle mNavigationHandle;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        // Ensure native pointer is initialized
        doReturn(1L).when(mOptimizationGuideBridgeJniMock).init();
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE,
                null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PersistedTabDataConfiguration.setUseTestConfig(true);
                });
        Profile.setLastUsedProfileForTesting(mProfileMock);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);
        doReturn(true).when(mNavigationHandle).isInPrimaryMainFrame();
    }

    @SmallTest
    @Test
    @CommandLineFlags.Add({
        "force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true/"
                + "return_empty_price_drops_until_init/false"
    })
    public void testDeferredStartup() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        final Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, mProfileMock);
        Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.initialize(tab);
                    ShoppingPersistedTabData.from(
                            tab,
                            (shoppingPersistedTabData) -> {
                                Assert.assertNotNull(shoppingPersistedTabData);
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.UPDATED_PRICE_MICROS,
                                        shoppingPersistedTabData.getPriceMicros());
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.PRICE_MICROS,
                                        shoppingPersistedTabData.getPreviousPriceMicros());
                                semaphore.release();
                            });
                    ShoppingPersistedTabData.onDeferredStartup();
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    @CommandLineFlags.Add({
        "force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true/"
                + "return_empty_price_drops_until_init/true"
    })
    public void testReturnEmptyPriceDropsUntilInit() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        final Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, mProfileMock);
        final Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.initialize(tab);
                    ShoppingPersistedTabData.from(
                            tab,
                            (shoppingPersistedTabData) -> {
                                Assert.assertNull(shoppingPersistedTabData);
                                semaphore.release();
                            });
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.onDeferredStartup();
                });
        final Semaphore newSemaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.from(
                            tab,
                            (shoppingPersistedTabData) -> {
                                Assert.assertNotNull(shoppingPersistedTabData);
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.UPDATED_PRICE_MICROS,
                                        shoppingPersistedTabData.getPriceMicros());
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.PRICE_MICROS,
                                        shoppingPersistedTabData.getPreviousPriceMicros());
                                newSemaphore.release();
                            });
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(newSemaphore);
    }
}
