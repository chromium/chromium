// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_history;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_SHOPPING;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfoCallback;
import org.chromium.components.commerce.core.ShoppingService.PricePoint;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.io.IOException;
import java.util.Arrays;
import java.util.Optional;

/** Render Tests for the price history bottom sheet content. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED})
@Batch(Batch.UNIT_TESTS)
public class PriceHistoryBottomSheetContentRenderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(UI_BROWSER_SHOPPING)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mMockTab;
    @Mock private Profile mMockProfile;
    @Mock private TabModelSelector mMockTabModelSelector;
    @Mock private PriceInsightsDelegate mMockPriceInsightsDelegate;
    @Mock private Callback<PropertyModel> mMockCallback;
    @Mock private ShoppingService mMockShoppingService;

    private static final String PRODUCT_TITLE = "Testing Sneaker";
    private static final String PRICE_HISTORY_SINGLE_CATALOGS_TITLE =
            "Price history across the web";
    private static final GURL TEST_URL = JUnitTestGURLs.EXAMPLE_URL;
    private static final String PRICE_HISTORY_MULTIPLE_CATALOGS_TITLE =
            "Price history across the web for this option";
    private static final String CATALOG_ATTRIBUTES = "Stainless steel, Espresso Bundle";
    private static final PriceInsightsInfo PRICE_INSIGHTS_INFO_SINGLE_CATALOG =
            new PriceInsightsInfo(
                    Optional.empty(),
                    "USD",
                    Optional.empty(),
                    Optional.empty(),
                    Optional.empty(),
                    Arrays.asList(new PricePoint("08-08-2024", 65000000L)),
                    Optional.of(TEST_URL),
                    0,
                    false);
    private static final PriceInsightsInfo PRICE_INSIGHTS_INFO_MULTIPLE_CATALOGS =
            new PriceInsightsInfo(
                    Optional.empty(),
                    "USD",
                    Optional.empty(),
                    Optional.empty(),
                    Optional.of(CATALOG_ATTRIBUTES),
                    Arrays.asList(new PricePoint("08-08-2024", 65000000L)),
                    Optional.of(TEST_URL),
                    0,
                    true);

    private View mContentView;
    private PriceHistoryBottomSheetContentCoordinator mCoordinator;
    private TextView mFakePriceHistoryChart;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        doReturn(mMockProfile).when(mMockTab).getProfile();
        doReturn(PRODUCT_TITLE).when(mMockTab).getTitle();
        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);
        doReturn(true).when(mMockShoppingService).isPriceInsightsEligible();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mFakePriceHistoryChart = new TextView(sActivityTestRule.getActivity());
                    mFakePriceHistoryChart.setText("Price history chart holder");
                    mFakePriceHistoryChart.setLayoutParams(
                            new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
                    mCoordinator =
                            new PriceHistoryBottomSheetContentCoordinator(
                                    sActivity,
                                    () -> mMockTab,
                                    () -> mMockTabModelSelector,
                                    mMockPriceInsightsDelegate);
                    mContentView = mCoordinator.getContentViewForTesting();
                    sActivity.setContentView(mContentView);
                });
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.hideContentView();
                });
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testPriceHistorySingleCatalog() throws IOException {
        doReturn(mFakePriceHistoryChart)
                .when(mMockPriceInsightsDelegate)
                .getPriceHistoryChartForPriceInsightsInfo(PRICE_INSIGHTS_INFO_SINGLE_CATALOG);
        setShoppingServiceGetPriceInsightsInfoForUrl(PRICE_INSIGHTS_INFO_SINGLE_CATALOG);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.requestContent(mMockCallback);
                });
        mRenderTestRule.render(mContentView, "price_history_single_catalog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testPriceHistoryMultipleCatalog() throws IOException {
        doReturn(mFakePriceHistoryChart)
                .when(mMockPriceInsightsDelegate)
                .getPriceHistoryChartForPriceInsightsInfo(PRICE_INSIGHTS_INFO_MULTIPLE_CATALOGS);
        setShoppingServiceGetPriceInsightsInfoForUrl(PRICE_INSIGHTS_INFO_MULTIPLE_CATALOGS);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.requestContent(mMockCallback);
                });
        mRenderTestRule.render(mContentView, "price_history_multiple_catalog");
    }

    private void setShoppingServiceGetPriceInsightsInfoForUrl(PriceInsightsInfo info) {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ((PriceInsightsInfoCallback) invocation.getArgument(1))
                                    .onResult(TEST_URL, info);
                            return null;
                        })
                .when(mMockShoppingService)
                .getPriceInsightsInfoForUrl(any(), any());
    }
}
