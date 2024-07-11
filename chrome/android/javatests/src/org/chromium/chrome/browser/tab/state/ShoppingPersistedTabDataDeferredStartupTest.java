// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactoryJni;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.content_public.browser.NavigationHandle;

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

    @Mock protected OptimizationGuideBridgeFactory.Natives mOptimizationGuideBridgeFactoryJniMock;
    @Mock protected OptimizationGuideBridge mOptimizationGuideBridgeMock;

    @Mock protected Profile mProfileMock;

    @Mock protected NavigationHandle mNavigationHandle;

    @Mock private ShoppingPersistedTabDataService mShoppingPersistedTabDataService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(
                OptimizationGuideBridgeFactoryJni.TEST_HOOKS,
                mOptimizationGuideBridgeFactoryJniMock);
        doReturn(mOptimizationGuideBridgeMock)
                .when(mOptimizationGuideBridgeFactoryJniMock)
                .getForProfile(mProfileMock);

        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR,
                OptimizationGuideDecision.TRUE,
                null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PersistedTabDataConfiguration.setUseTestConfig(true);
                });
        ProfileManager.setLastUsedProfileForTesting(mProfileMock);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);
        doReturn(true).when(mNavigationHandle).isInPrimaryMainFrame();
        ShoppingPersistedTabDataService.setServiceForTesting(mShoppingPersistedTabDataService);
    }

    @SmallTest
    @Test
    @CommandLineFlags.Add({
        "force-fieldtrial-params=Study.Group:return_empty_price_drops_until_init/false"
    })
    @EnableFeatures({ChromeFeatureList.PRICE_CHANGE_MODULE})
    public void testDeferredStartup() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeMock,
                HintsProto.OptimizationType.PRICE_TRACKING,
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        final Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, mProfileMock);
        Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
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
                                verify(mShoppingPersistedTabDataService, times(1))
                                        .notifyPriceDropStatus(tab, true);
                                semaphore.release();
                            });
                    ShoppingPersistedTabData.onDeferredStartup();
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    @CommandLineFlags.Add({
        "force-fieldtrial-params=Study.Group:return_empty_price_drops_until_init/true"
    })
    @EnableFeatures({ChromeFeatureList.PRICE_CHANGE_MODULE})
    public void testReturnEmptyPriceDropsUntilInit() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeMock,
                HintsProto.OptimizationType.PRICE_TRACKING,
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        final Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, mProfileMock);
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.initialize(tab);
                    ShoppingPersistedTabData.from(
                            tab,
                            (shoppingPersistedTabData) -> {
                                Assert.assertNull(shoppingPersistedTabData);
                                verify(mShoppingPersistedTabDataService, times(0))
                                        .notifyPriceDropStatus(any(), anyBoolean());
                                semaphore.release();
                            });
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.onDeferredStartup();
                });
        final Semaphore newSemaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
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
                                verify(mShoppingPersistedTabDataService, times(1))
                                        .notifyPriceDropStatus(tab, true);
                                newSemaphore.release();
                            });
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(newSemaphore);
    }

    @SmallTest
    @Test
    @EnableFeatures({ChromeFeatureList.PRICE_CHANGE_MODULE})
    @CommandLineFlags.Add({
        "force-fieldtrial-params=Study.Group:return_empty_price_drops_until_init/true"
    })
    public void testSkipDelayedInitialization_NotSkip() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeMock,
                HintsProto.OptimizationType.PRICE_TRACKING,
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        final Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, mProfileMock);
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.initialize(tab);
                    ShoppingPersistedTabData.from(
                            tab,
                            (shoppingPersistedTabData) -> {
                                Assert.assertNull(shoppingPersistedTabData);
                                semaphore.release();
                            },
                            false);
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    @EnableFeatures({ChromeFeatureList.PRICE_CHANGE_MODULE})
    @CommandLineFlags.Add({
        "force-fieldtrial-params=Study.Group:return_empty_price_drops_until_init/true"
    })
    public void testSkipDelayedInitialization_Skip() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeMock,
                HintsProto.OptimizationType.PRICE_TRACKING,
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        final Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, mProfileMock);

        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
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
                            },
                            true);
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    @EnableFeatures({ChromeFeatureList.PRICE_CHANGE_MODULE})
    @CommandLineFlags.Add({
        "force-fieldtrial-params=Study.Group:return_empty_price_drops_until_init/true"
    })
    public void testSkipDelayedInitialization_SkipForNullTab() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeMock,
                HintsProto.OptimizationType.PRICE_TRACKING,
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);

        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.from(
                            null,
                            (shoppingPersistedTabData) -> {
                                Assert.assertNull(shoppingPersistedTabData);
                                semaphore.release();
                            },
                            true);
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    @EnableFeatures({ChromeFeatureList.PRICE_CHANGE_MODULE})
    @CommandLineFlags.Add({
        "force-fieldtrial-params=Study.Group:return_empty_price_drops_until_init/true"
    })
    public void testSkipDelayedInitialization_SkipForDestroyedTab() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeMock,
                HintsProto.OptimizationType.PRICE_TRACKING,
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        final Tab tab = mock(Tab.class);
        doReturn(true).when(tab).isDestroyed();

        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.from(
                            tab,
                            (shoppingPersistedTabData) -> {
                                Assert.assertNull(shoppingPersistedTabData);
                                semaphore.release();
                            },
                            true);
                });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }
}
