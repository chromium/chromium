// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_SHOPPING;

import android.app.Activity;
import android.view.View;

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
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.components.commerce.core.ShoppingService.ProductInfoCallback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.Optional;

/** Render Tests for the price tracking bottom sheet content. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED})
@Batch(Batch.UNIT_TESTS)
public class PriceTrackingBottomSheetContentRenderTest {
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
    @Mock private PriceInsightsDelegate mMockPriceInsightsDelegate;
    @Mock private Callback<PropertyModel> mMockCallback;
    @Mock private ObservableSupplier<Boolean> mMockPriceTrackingStateSupplier;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingService mMockShoppingService;

    private static final String PRODUCT_TITLE = "Testing Sneaker";
    private static final ProductInfo PRODUCT_INFO =
            new ProductInfo(
                    null,
                    null,
                    Optional.of(12345L),
                    Optional.empty(),
                    null,
                    0,
                    null,
                    Optional.empty());

    private View mContentView;
    private PriceTrackingBottomSheetContentCoordinator mCoordinator;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        doReturn(mMockProfile).when(mMockTab).getProfile();
        doReturn(PRODUCT_TITLE).when(mMockTab).getTitle();
        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);
        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        doReturn(false).when(mMockPriceTrackingStateSupplier).get();
        doReturn(mMockPriceTrackingStateSupplier)
                .when(mMockPriceInsightsDelegate)
                .getPriceTrackingStateSupplier(mMockTab);
        setUpGetPriceProductInfoForUrl();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new PriceTrackingBottomSheetContentCoordinator(
                                    sActivity, () -> mMockTab, mMockPriceInsightsDelegate);
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
    public void testPriceTrackingEnabled() throws IOException {
        doReturn(true).when(mMockPriceTrackingStateSupplier).get();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.requestContent(mMockCallback);
                });
        mRenderTestRule.render(mContentView, "price_tracking_enabled");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testPriceTrackingDisabled() throws IOException {
        doReturn(false).when(mMockPriceTrackingStateSupplier).get();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.requestContent(mMockCallback);
                });
        mRenderTestRule.render(mContentView, "price_tracking_disabled");
    }

    private void setUpGetPriceProductInfoForUrl() {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ((ProductInfoCallback) invocation.getArgument(1))
                                    .onResult(invocation.getArgument(0), PRODUCT_INFO);
                            return null;
                        })
                .when(mMockShoppingService)
                .getProductInfoForUrl(any(), any());
    }
}
