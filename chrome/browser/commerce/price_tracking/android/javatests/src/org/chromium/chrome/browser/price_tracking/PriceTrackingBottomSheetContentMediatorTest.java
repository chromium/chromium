// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.ALL_KEYS;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_ICON;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_TEXT;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_TITLE;

import android.app.Activity;
import android.view.View.OnClickListener;

import org.junit.After;
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
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.PriceBucket;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfoCallback;
import org.chromium.components.commerce.core.ShoppingService.PricePoint;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.components.commerce.core.ShoppingService.ProductInfoCallback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ToastManager;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;

/** Tests for {@link PriceTrackingBottomSheetContentMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED})
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
public class PriceTrackingBottomSheetContentMediatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mMockTab;
    @Mock private Profile mMockProfile;
    @Mock private ShoppingService mMockShoppingService;
    @Mock private PriceInsightsDelegate mMockPriceInsightsDelegate;
    @Mock private ObservableSupplier<Boolean> mMockPriceTrackingStateSupplier;
    @Mock private Callback<Boolean> mMockCallback;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;

    private static final String PRODUCT_TITLE = "Testing Sneaker";
    private static final String PRICE_TRACKING_DISABLED_BUTTON_TEXT = "Track";
    private static final String PRICE_TRACKING_ENABLED_BUTTON_TEXT = "Tracking";
    private static final GURL TEST_URL = JUnitTestGURLs.EXAMPLE_URL;
    private static final @PriceBucket int PRICE_BUCKET = 1;
    private static final PriceInsightsInfo PRICE_INSIGHTS_INFO =
            new PriceInsightsInfo(
                    null,
                    "USD",
                    null,
                    null,
                    null,
                    Arrays.asList(new PricePoint("08-08-2024", 65000000L)),
                    TEST_URL,
                    PRICE_BUCKET,
                    false);
    private static final ProductInfo PRODUCT_INFO =
            new ProductInfo(null, null, 12345L, null, null, 0, null, null);

    private PriceTrackingBottomSheetContentMediator mMediator;
    private final PropertyModel mPropertyModel = new PropertyModel(ALL_KEYS);
    private Activity mActivity;
    private HistogramWatcher mHistogramWatcher;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        doReturn(mMockProfile).when(mMockTab).getProfile();
        doReturn(PRODUCT_TITLE).when(mMockTab).getTitle();

        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);
        setShoppingServiceGetPriceInsightsInfoForUrl(PRICE_INSIGHTS_INFO);

        doReturn(false).when(mMockPriceTrackingStateSupplier).get();
        doReturn(mMockPriceTrackingStateSupplier)
                .when(mMockPriceInsightsDelegate)
                .getPriceTrackingStateSupplier(mMockTab);

        mMediator =
                new PriceTrackingBottomSheetContentMediator(
                        mActivity, () -> mMockTab, mPropertyModel, mMockPriceInsightsDelegate);
    }

    @After
    public void tearDown() {
        ToastManager.resetForTesting();
        ShadowToast.reset();
    }

    @Test
    public void testRequestShowContent_PriceTrackingNotEligible() {
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());
        mMediator.requestShowContent(mMockCallback);
        verify(mMockCallback).onResult(false);
    }

    @Test
    public void testRequestShowContent_ProductInfoNotAvailable() {
        setUpGetPriceProductInfoForUrl(null);
        mMediator.requestShowContent(mMockCallback);
        verify(mMockCallback).onResult(false);
    }

    @Test
    public void testRequestShowContent_PriceTrackingEligibleAndDisabled() {
        setUpGetPriceProductInfoForUrl(PRODUCT_INFO);
        mMediator.requestShowContent(mMockCallback);
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Commerce.PriceInsights.PriceTracking.Track", PRICE_BUCKET)
                        .build();

        verify(mMockCallback).onResult(true);
        assertEquals(PRODUCT_TITLE, mPropertyModel.get(PRICE_TRACKING_TITLE));
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ false);

        // Test click price tracking button and set from state disabled to enabled success.
        setResultForPriceTrackingUpdate(/* success= */ true);
        OnClickListener priceTrackingButtonListener =
                mPropertyModel.get(PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER);
        assertNotNull(priceTrackingButtonListener);
        priceTrackingButtonListener.onClick(null);
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ true);
        assertNotNull(ShadowToast.getLatestToast());
        mHistogramWatcher.assertExpected();
    }

    @Test
    public void testRequestShowContent_PriceTrackingEligibleAndEnabled() {
        setUpGetPriceProductInfoForUrl(PRODUCT_INFO);
        doReturn(true).when(mMockPriceTrackingStateSupplier).get();
        mMediator.requestShowContent(mMockCallback);
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Commerce.PriceInsights.PriceTracking.Untrack", PRICE_BUCKET)
                        .build();

        verify(mMockCallback).onResult(true);
        assertEquals(PRODUCT_TITLE, mPropertyModel.get(PRICE_TRACKING_TITLE));
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ true);

        // Test click price tracking button and set from state enabled to disabled success.
        setResultForPriceTrackingUpdate(/* success= */ true);
        OnClickListener priceTrackingButtonListener =
                mPropertyModel.get(PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER);
        assertNotNull(priceTrackingButtonListener);
        priceTrackingButtonListener.onClick(null);
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ false);
        assertNotNull(ShadowToast.getLatestToast());
        mHistogramWatcher.assertExpected();
    }

    @Test
    public void testRequestShowContent_PriceTrackingButtonOnClick_Failed() {
        setUpGetPriceProductInfoForUrl(PRODUCT_INFO);
        doReturn(false).when(mMockPriceTrackingStateSupplier).get();
        mMediator.requestShowContent(mMockCallback);
        mHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Commerce.PriceInsights.PriceTracking.Track", PRICE_BUCKET)
                        .build();

        verify(mMockCallback).onResult(true);
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ false);

        // Test click price tracking button and set from state disabled to enabled failed.
        setResultForPriceTrackingUpdate(/* success= */ false);
        OnClickListener priceTrackingButtonListener =
                mPropertyModel.get(PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER);
        priceTrackingButtonListener.onClick(null);
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ false);
        assertNotNull(ShadowToast.getLatestToast());
        mHistogramWatcher.assertExpected();
    }

    @Test
    public void testPriceTrackingStateSupplier() {
        mMediator.requestShowContent(mMockCallback);
        verify(mMockPriceTrackingStateSupplier, times(1)).addObserver(any());

        mMediator.closeContent();
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

    private void setUpGetPriceProductInfoForUrl(ProductInfo info) {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ((ProductInfoCallback) invocation.getArgument(1))
                                    .onResult(TEST_URL, info);
                            return null;
                        })
                .when(mMockShoppingService)
                .getProductInfoForUrl(any(), any());
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

        assertEquals(buttonText, mPropertyModel.get(PRICE_TRACKING_BUTTON_TEXT));
        assertEquals(buttonIconResId, mPropertyModel.get(PRICE_TRACKING_BUTTON_ICON));
        assertEquals(
                buttonForegroundColor, mPropertyModel.get(PRICE_TRACKING_BUTTON_FOREGROUND_COLOR));
        assertEquals(
                buttonBackgroundColor, mPropertyModel.get(PRICE_TRACKING_BUTTON_BACKGROUND_COLOR));
    }
}
