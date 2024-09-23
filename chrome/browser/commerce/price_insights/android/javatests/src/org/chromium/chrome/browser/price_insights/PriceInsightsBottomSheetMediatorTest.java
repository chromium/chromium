// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.ALL_KEYS;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.OPEN_URL_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.OPEN_URL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_HISTORY_CHART;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_HISTORY_DESCRIPTION;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_HISTORY_TITLE;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ENABLED;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ICON;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_TEXT;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_TITLE;

import android.app.Activity;
import android.app.NotificationManager;
import android.content.res.Resources;
import android.view.View;
import android.view.View.OnClickListener;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfoCallback;
import org.chromium.components.commerce.core.ShoppingService.PricePoint;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ToastManager;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Optional;

/** Tests for {@link PriceInsightsBottomSheetMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
public class PriceInsightsBottomSheetMediatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mMockTab;
    @Mock private Profile mMockProfile;
    @Mock private TabModelSelector mMockTabModelSelector;
    @Mock private ShoppingService mMockShoppingService;
    @Mock private Resources mMockResources;
    @Mock private BookmarkId mMockBookmarkId;
    @Mock private PriceInsightsDelegate mMockPriceInsightsDelegate;
    @Mock private ObservableSupplier<Boolean> mMockPriceTrackingStateSupplier;
    @Mock private View mMockPriceHistoryChart;
    @Mock private NotificationManager mMockNotificationManager;

    private static final String PRODUCT_TITLE = "Testing Sneaker";
    private static final String PRICE_TRACKING_DISABLED_BUTTON_TEXT = "Track";
    private static final String PRICE_TRACKING_ENABLED_BUTTON_TEXT = "Tracking";
    private static final String PRICE_HISTORY_SINGLE_CATALOGS_TITLE =
            "Price history across the web";
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
                    Optional.of(JUnitTestGURLs.EXAMPLE_URL),
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
                    Optional.of(JUnitTestGURLs.EXAMPLE_URL),
                    0,
                    true);

    private PriceInsightsBottomSheetMediator mPriceInsightsMediator;
    private PropertyModel mPropertyModel = new PropertyModel(ALL_KEYS);
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        doReturn(mMockProfile).when(mMockTab).getProfile();
        doReturn(PRODUCT_TITLE).when(mMockTab).getTitle();

        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);

        doReturn(false).when(mMockPriceTrackingStateSupplier).get();
        doReturn(mMockPriceTrackingStateSupplier)
                .when(mMockPriceInsightsDelegate)
                .getPriceTrackingStateSupplier(mMockTab);

        mPriceInsightsMediator =
                new PriceInsightsBottomSheetMediator(
                        mActivity,
                        mMockTab,
                        mMockTabModelSelector,
                        mMockShoppingService,
                        mMockPriceInsightsDelegate,
                        mPropertyModel);
    }

    @After
    public void tearDown() {
        ToastManager.resetForTesting();
        ShadowToast.reset();
    }

    @Test
    public void testRequestShowContent_PriceTrackingNotEligible() {
        mPriceInsightsMediator.requestShowContent();

        assertEquals(PRODUCT_TITLE, mPropertyModel.get(PRICE_TRACKING_TITLE));
        assertFalse(mPropertyModel.get(PRICE_TRACKING_BUTTON_ENABLED));
        assertEquals(
                R.drawable.price_insights_sheet_price_tracking_button_disabled,
                mPropertyModel.get(PRICE_TRACKING_BUTTON_ICON));
        assertEquals(
                PRICE_TRACKING_DISABLED_BUTTON_TEXT,
                mPropertyModel.get(PRICE_TRACKING_BUTTON_TEXT));
        assertEquals(
                mActivity.getColor(R.color.price_tracking_ineligible_button_foreground_color),
                mPropertyModel.get(PRICE_TRACKING_BUTTON_FOREGROUND_COLOR));
        assertEquals(
                mActivity.getColor(R.color.price_tracking_ineligible_button_background_color),
                mPropertyModel.get(PRICE_TRACKING_BUTTON_BACKGROUND_COLOR));
        assertNull(mPropertyModel.get(PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER));
    }

    @Test
    public void testRequestShowContent_PriceTrackingEligibleAndDisabled() {
        doReturn(false).when(mMockPriceTrackingStateSupplier).get();
        setShoppingServiceGetProductInfoForUrl();
        mPriceInsightsMediator.requestShowContent();

        assertEquals(PRODUCT_TITLE, mPropertyModel.get(PRICE_TRACKING_TITLE));
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ false);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Commerce.PriceInsights.PriceTracking.Track")
                        .build();

        // Test click price tracking button and set from state disabled to enabled success.
        setResultForPriceTrackingUpdate(/* success= */ true);
        OnClickListener priceTrackingButtonListener =
                mPropertyModel.get(PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER);
        assertNotNull(priceTrackingButtonListener);
        priceTrackingButtonListener.onClick(null);
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ true);
        watcher.assertExpected();
        assertNotNull(ShadowToast.getLatestToast());
    }

    @Test
    public void testRequestShowContent_PriceTrackingEligibleAndEnabled() {
        doReturn(true).when(mMockPriceTrackingStateSupplier).get();
        setShoppingServiceGetProductInfoForUrl();
        mPriceInsightsMediator.requestShowContent();

        assertEquals(PRODUCT_TITLE, mPropertyModel.get(PRICE_TRACKING_TITLE));
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ true);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Commerce.PriceInsights.PriceTracking.Untrack")
                        .build();

        // Test click price tracking button and set from state enabled to disabled success.
        setResultForPriceTrackingUpdate(/* success= */ true);
        OnClickListener priceTrackingButtonListener =
                mPropertyModel.get(PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER);
        assertNotNull(priceTrackingButtonListener);
        priceTrackingButtonListener.onClick(null);
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ false);
        watcher.assertExpected();
        assertNotNull(ShadowToast.getLatestToast());
    }

    @Test
    public void testRequestShowContent_PriceTrackingButtonOnClick_Failed() {
        doReturn(false).when(mMockPriceTrackingStateSupplier).get();
        setShoppingServiceGetProductInfoForUrl();
        mPriceInsightsMediator.requestShowContent();

        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ false);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Commerce.PriceInsights.PriceTracking.Track")
                        .build();

        // Test click price tracking button and set from state disabled to enabled failed.
        setResultForPriceTrackingUpdate(/* success= */ false);
        OnClickListener priceTrackingButtonListener =
                mPropertyModel.get(PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER);
        priceTrackingButtonListener.onClick(null);
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ false);
        watcher.assertExpected();
        assertNotNull(ShadowToast.getLatestToast());
    }

    @Test
    public void testRequestShowContent_PriceHistorySingleCatalog() {
        doReturn(mMockPriceHistoryChart)
                .when(mMockPriceInsightsDelegate)
                .getPriceHistoryChartForPriceInsightsInfo(PRICE_INSIGHTS_INFO_SINGLE_CATALOG);
        setShoppingServiceGetPriceInsightsInfoForUrl(PRICE_INSIGHTS_INFO_SINGLE_CATALOG);
        mPriceInsightsMediator.requestShowContent();

        assertEquals(PRICE_HISTORY_SINGLE_CATALOGS_TITLE, mPropertyModel.get(PRICE_HISTORY_TITLE));
        assertNull(mPropertyModel.get(PRICE_HISTORY_DESCRIPTION));
        assertEquals(mMockPriceHistoryChart, mPropertyModel.get(PRICE_HISTORY_CHART));
    }

    @Test
    public void testRequestShowContent_PriceHistoryMultipleCatalogs() {
        doReturn(mMockPriceHistoryChart)
                .when(mMockPriceInsightsDelegate)
                .getPriceHistoryChartForPriceInsightsInfo(PRICE_INSIGHTS_INFO_MULTIPLE_CATALOGS);
        setShoppingServiceGetPriceInsightsInfoForUrl(PRICE_INSIGHTS_INFO_MULTIPLE_CATALOGS);
        mPriceInsightsMediator.requestShowContent();

        assertEquals(
                PRICE_HISTORY_MULTIPLE_CATALOGS_TITLE, mPropertyModel.get(PRICE_HISTORY_TITLE));
        assertEquals(CATALOG_ATTRIBUTES, mPropertyModel.get(PRICE_HISTORY_DESCRIPTION));
        assertEquals(mMockPriceHistoryChart, mPropertyModel.get(PRICE_HISTORY_CHART));
    }

    @Test
    public void testRequestShowContent_OpenUrlButton() {
        setShoppingServiceGetPriceInsightsInfoForUrl(PRICE_INSIGHTS_INFO_SINGLE_CATALOG);
        mPriceInsightsMediator.requestShowContent();

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

    @Test
    public void testPriceTrackingStateSupplier() {
        mPriceInsightsMediator.requestShowContent();
        verify(mMockPriceTrackingStateSupplier, times(1)).addObserver(any());

        mPriceInsightsMediator.closeContent();
        verify(mMockPriceTrackingStateSupplier, times(1)).removeObserver(any());
    }

    private void setResultForPriceTrackingUpdate(boolean success) {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            if (success) {
                                boolean newState = invocation.getArgument(1);
                                doReturn(newState).when(mMockPriceTrackingStateSupplier).get();
                            }
                            ((Callback<Boolean>) invocation.getArgument(2)).onResult(success);
                            return null;
                        })
                .when(mMockPriceInsightsDelegate)
                .setPriceTrackingStateForTab(any(Tab.class), anyBoolean(), any());
    }

    private void setShoppingServiceGetProductInfoForUrl() {
        ProductInfo productInfo =
                new ProductInfo(
                        null,
                        null,
                        Optional.of(12345L),
                        Optional.empty(),
                        null,
                        0,
                        null,
                        Optional.empty());
        doReturn(productInfo).when(mMockShoppingService).getAvailableProductInfoForUrl(any());
    }

    private void setShoppingServiceGetPriceInsightsInfoForUrl(PriceInsightsInfo info) {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ((PriceInsightsInfoCallback) invocation.getArgument(1))
                                    .onResult(JUnitTestGURLs.EXAMPLE_URL, info);
                            return null;
                        })
                .when(mMockShoppingService)
                .getPriceInsightsInfoForUrl(any(), any());
    }

    private void assertPriceTrackingButtonHasTrackingState(boolean isTracking) {
        String buttonText =
                isTracking
                        ? PRICE_TRACKING_ENABLED_BUTTON_TEXT
                        : PRICE_TRACKING_DISABLED_BUTTON_TEXT;
        int buttonIconResId =
                isTracking
                        ? R.drawable.price_insights_sheet_price_tracking_button_enabled
                        : R.drawable.price_insights_sheet_price_tracking_button_disabled;
        int buttonForegroundColor =
                isTracking
                        ? SemanticColorUtils.getDefaultControlColorActive(mActivity)
                        : SemanticColorUtils.getDefaultIconColorOnAccent1Container(mActivity);
        int buttonBackgroundColor =
                isTracking
                        ? SemanticColorUtils.getDefaultBgColor(mActivity)
                        : SemanticColorUtils.getColorPrimaryContainer(mActivity);

        assertTrue(mPropertyModel.get(PRICE_TRACKING_BUTTON_ENABLED));
        assertEquals(buttonText, mPropertyModel.get(PRICE_TRACKING_BUTTON_TEXT));
        assertEquals(buttonIconResId, mPropertyModel.get(PRICE_TRACKING_BUTTON_ICON));
        assertEquals(
                buttonForegroundColor, mPropertyModel.get(PRICE_TRACKING_BUTTON_FOREGROUND_COLOR));
        assertEquals(
                buttonBackgroundColor, mPropertyModel.get(PRICE_TRACKING_BUTTON_BACKGROUND_COLOR));
    }
}
