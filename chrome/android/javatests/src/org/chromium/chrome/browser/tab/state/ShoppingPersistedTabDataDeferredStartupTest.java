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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabDataTestUtils.ShoppingServiceResponse;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.content_public.browser.NavigationHandle;

import java.util.concurrent.Semaphore;

/** Tests the deferred startup of {@link ShoppingPersistedTabData} */
@RunWith(BaseJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.PRICE_ANNOTATIONS, ChromeFeatureList.PRICE_CHANGE_MODULE})
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class ShoppingPersistedTabDataDeferredStartupTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Mock ShoppingService mShoppingService;

    @Mock protected Profile mProfileMock;

    @Mock protected NavigationHandle mNavigationHandle;

    @Mock private ShoppingPersistedTabDataService mShoppingPersistedTabDataService;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PersistedTabDataConfiguration.setUseTestConfig(true);
                });
        ProfileManager.setLastUsedProfileForTesting(mProfileMock);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        doReturn(true).when(mNavigationHandle).isInPrimaryMainFrame();
        ShoppingPersistedTabDataService.setServiceForTesting(mShoppingPersistedTabDataService);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
    }

    @SmallTest
    @Test
    @EnableFeatures(
            ChromeFeatureList.PRICE_ANNOTATIONS + ":return_empty_price_drops_until_init/false")
    public void testDeferredStartup() {
        final Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, mProfileMock);
        ShoppingPersistedTabDataTestUtils.mockShoppingServiceResponse(
                mShoppingService, tab.getUrl(), ShoppingServiceResponse.PRICE_DROP_1);
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
    @EnableFeatures(
            ChromeFeatureList.PRICE_ANNOTATIONS + ":return_empty_price_drops_until_init/true")
    public void testReturnEmptyPriceDropsUntilInit() {
        final Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, mProfileMock);
        ShoppingPersistedTabDataTestUtils.mockShoppingServiceResponse(
                mShoppingService, tab.getUrl(), ShoppingServiceResponse.PRICE_DROP_1);
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
    @EnableFeatures(
            ChromeFeatureList.PRICE_ANNOTATIONS + ":return_empty_price_drops_until_init/true")
    public void testSkipDelayedInitialization_NotSkip() {
        final Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, mProfileMock);
        ShoppingPersistedTabDataTestUtils.mockShoppingServiceResponse(
                mShoppingService, tab.getUrl(), ShoppingServiceResponse.PRICE_DROP_1);
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
    @EnableFeatures(
            ChromeFeatureList.PRICE_ANNOTATIONS + ":return_empty_price_drops_until_init/true")
    public void testSkipDelayedInitialization_Skip() {
        final Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(0, mProfileMock);
        ShoppingPersistedTabDataTestUtils.mockShoppingServiceResponse(
                mShoppingService, tab.getUrl(), ShoppingServiceResponse.PRICE_DROP_1);
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
    @EnableFeatures(
            ChromeFeatureList.PRICE_ANNOTATIONS + ":return_empty_price_drops_until_init/true")
    public void testSkipDelayedInitialization_SkipForNullTab() {
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
    @EnableFeatures(
            ChromeFeatureList.PRICE_ANNOTATIONS + ":return_empty_price_drops_until_init/true")
    public void testSkipDelayedInitialization_SkipForDestroyedTab() {
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
