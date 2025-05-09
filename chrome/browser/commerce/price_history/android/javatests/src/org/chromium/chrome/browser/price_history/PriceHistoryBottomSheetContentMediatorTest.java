// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.ALL_KEYS;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.OPEN_URL_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.OPEN_URL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_CHART;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_CHART_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_DESCRIPTION;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_TITLE;

import android.app.Activity;
import android.view.View;
import android.view.View.OnClickListener;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfoCallback;
import org.chromium.components.commerce.core.ShoppingService.PricePoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Optional;

/** Tests for {@link PriceHistoryBottomSheetContentMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceHistoryBottomSheetContentMediatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mMockTab;
    @Mock private Profile mMockProfile;
    @Mock private TabModelSelector mMockTabModelSelector;
    @Mock private ShoppingService mMockShoppingService;
    @Mock private PriceInsightsDelegate mMockPriceInsightsDelegate;
    @Mock private Callback<Boolean> mMockCallback;
    @Mock private View mMockPriceHistoryChart;

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

    private PriceHistoryBottomSheetContentMediator mMediator;
    private final PropertyModel mPropertyModel = new PropertyModel(ALL_KEYS);
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        doReturn(mMockProfile).when(mMockTab).getProfile();
        doReturn(PRODUCT_TITLE).when(mMockTab).getTitle();

        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);
        doReturn(true).when(mMockShoppingService).isPriceInsightsEligible();

        mMediator =
                new PriceHistoryBottomSheetContentMediator(
                        mActivity,
                        () -> mMockTab,
                        () -> mMockTabModelSelector,
                        mPropertyModel,
                        mMockPriceInsightsDelegate);
    }

    @Test
    public void testRequestShowContent_PriceInsightsNotEligible() {
        doReturn(false).when(mMockShoppingService).isPriceInsightsEligible();
        mMediator.requestShowContent(mMockCallback);
        verify(mMockCallback).onResult(false);
    }

    @Test
    public void testRequestShowContent_PriceInsightsInfoNotAvailable() {
        setUpGetPriceInsightsInfoForUrl(null);
        mMediator.requestShowContent(mMockCallback);
        verify(mMockCallback).onResult(false);
    }

    @Test
    public void testRequestShowContent_PriceHistorySingleCatalog() {
        doReturn(mMockPriceHistoryChart)
                .when(mMockPriceInsightsDelegate)
                .getPriceHistoryChartForPriceInsightsInfo(PRICE_INSIGHTS_INFO_SINGLE_CATALOG);
        setUpGetPriceInsightsInfoForUrl(PRICE_INSIGHTS_INFO_SINGLE_CATALOG);
        mMediator.requestShowContent(mMockCallback);

        verify(mMockCallback).onResult(true);
        assertEquals(PRODUCT_TITLE, mPropertyModel.get(PRICE_HISTORY_CHART_CONTENT_DESCRIPTION));
        assertEquals(PRICE_HISTORY_SINGLE_CATALOGS_TITLE, mPropertyModel.get(PRICE_HISTORY_TITLE));
        assertFalse(mPropertyModel.get(PRICE_HISTORY_DESCRIPTION_VISIBLE));
        assertNull(mPropertyModel.get(PRICE_HISTORY_DESCRIPTION));
        assertEquals(mMockPriceHistoryChart, mPropertyModel.get(PRICE_HISTORY_CHART));
    }

    @Test
    public void testRequestShowContent_PriceHistoryMultipleCatalogs() {
        doReturn(mMockPriceHistoryChart)
                .when(mMockPriceInsightsDelegate)
                .getPriceHistoryChartForPriceInsightsInfo(PRICE_INSIGHTS_INFO_MULTIPLE_CATALOGS);
        setUpGetPriceInsightsInfoForUrl(PRICE_INSIGHTS_INFO_MULTIPLE_CATALOGS);
        mMediator.requestShowContent(mMockCallback);

        verify(mMockCallback).onResult(true);
        assertEquals(PRODUCT_TITLE, mPropertyModel.get(PRICE_HISTORY_CHART_CONTENT_DESCRIPTION));
        assertEquals(
                PRICE_HISTORY_MULTIPLE_CATALOGS_TITLE, mPropertyModel.get(PRICE_HISTORY_TITLE));
        assertTrue(mPropertyModel.get(PRICE_HISTORY_DESCRIPTION_VISIBLE));
        assertEquals(CATALOG_ATTRIBUTES, mPropertyModel.get(PRICE_HISTORY_DESCRIPTION));
        assertEquals(mMockPriceHistoryChart, mPropertyModel.get(PRICE_HISTORY_CHART));
    }

    @Test
    public void testRequestShowContent_OpenUrlButton() {
        setUpGetPriceInsightsInfoForUrl(PRICE_INSIGHTS_INFO_SINGLE_CATALOG);
        mMediator.requestShowContent(mMockCallback);

        verify(mMockCallback).onResult(true);
        assertTrue(mPropertyModel.get(OPEN_URL_BUTTON_VISIBLE));
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Commerce.PriceInsights.BuyingOptionsClicked")
                        .build();

        OnClickListener openUrlListener = mPropertyModel.get(OPEN_URL_BUTTON_ON_CLICK_LISTENER);
        assertNotNull(openUrlListener);
        openUrlListener.onClick(null);
        verify(mMockTabModelSelector)
                .openNewTab(
                        any(LoadUrlParams.class),
                        eq(TabLaunchType.FROM_LINK),
                        eq(mMockTab),
                        eq(false));
        watcher.assertExpected();
    }

    private void setUpGetPriceInsightsInfoForUrl(PriceInsightsInfo info) {
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
