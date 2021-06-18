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
 * Test relating to {@link ShoppingPersistedTabData} and {@link PageAnnotationService}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class ShoppingPersistedTabDataWithPASTest {
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
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/false"})
    public void testShoppingBloomFilterNotShoppingWebsite() {
        ShoppingPersistedTabDataWithPASTestUtils.mockPageAnnotationsResponse(
                mPageAnnotationsServiceMock,
                ShoppingPersistedTabDataWithPASTestUtils.MockPageAnnotationsResponse
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
        ShoppingPersistedTabDataWithPASTestUtils.verifyGetPageAnnotationsCalled(
                mPageAnnotationsServiceMock, 0);
    }

    @SmallTest
    @Test
    public void testShoppingBloomFilterShoppingWebsite() {
        for (@OptimizationGuideDecision int decision :
                new int[] {OptimizationGuideDecision.TRUE, OptimizationGuideDecision.UNKNOWN}) {
            ShoppingPersistedTabDataWithPASTestUtils.mockPageAnnotationsResponse(
                    mPageAnnotationsServiceMock,
                    ShoppingPersistedTabDataWithPASTestUtils.MockPageAnnotationsResponse
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
            ShoppingPersistedTabDataWithPASTestUtils.verifyGetPageAnnotationsCalled(
                    mPageAnnotationsServiceMock, 1);
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSPTDSavingEnabledUponSuccessfulProductUpdateResponse() {
        final Semaphore semaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataWithPASTestUtils.mockPageAnnotationsResponse(
                mPageAnnotationsServiceMock,
                ShoppingPersistedTabDataWithPASTestUtils.MockPageAnnotationsResponse
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
        ShoppingPersistedTabDataWithPASTestUtils.mockPageAnnotationsResponse(
                mPageAnnotationsServiceMock,
                ShoppingPersistedTabDataWithPASTestUtils.MockPageAnnotationsResponse
                        .BUYABLE_PRODUCT_EMPTY);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertNull(shoppingPersistedTabData);
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }
}