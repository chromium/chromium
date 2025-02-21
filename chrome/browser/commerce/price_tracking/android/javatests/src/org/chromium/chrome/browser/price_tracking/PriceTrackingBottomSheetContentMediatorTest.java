// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ENABLED;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ICON;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_TEXT;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_KEYS;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_TITLE;

import android.app.Activity;
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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ToastManager;

/** Tests for {@link PriceTrackingBottomSheetContentMediator}. */
@Batch(Batch.UNIT_TESTS)
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

    private PriceTrackingBottomSheetContentMediator mMediator;
    private PropertyModel mPropertyModel = new PropertyModel(PRICE_TRACKING_KEYS);
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        doReturn(mMockProfile).when(mMockTab).getProfile();
        doReturn(PRODUCT_TITLE).when(mMockTab).getTitle();

        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);

        doReturn(false).when(mMockPriceTrackingStateSupplier).get();
        doReturn(mMockPriceTrackingStateSupplier)
                .when(mMockPriceInsightsDelegate)
                .getPriceTrackingStateSupplier(mMockTab);

        mMediator =
                new PriceTrackingBottomSheetContentMediator(
                        mActivity, mMockTab, mPropertyModel, mMockPriceInsightsDelegate);
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
        mMediator.requestShowContent(mMockCallback);

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
    }

    @Test
    public void testRequestShowContent_PriceTrackingEligibleAndEnabled() {
        doReturn(true).when(mMockPriceTrackingStateSupplier).get();
        mMediator.requestShowContent(mMockCallback);

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
    }

    @Test
    public void testRequestShowContent_PriceTrackingButtonOnClick_Failed() {
        doReturn(false).when(mMockPriceTrackingStateSupplier).get();
        mMediator.requestShowContent(mMockCallback);

        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ false);

        // Test click price tracking button and set from state disabled to enabled failed.
        setResultForPriceTrackingUpdate(/* success= */ false);
        OnClickListener priceTrackingButtonListener =
                mPropertyModel.get(PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER);
        priceTrackingButtonListener.onClick(null);
        assertPriceTrackingButtonHasTrackingState(/* isTracking= */ false);
        assertNotNull(ShadowToast.getLatestToast());
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
