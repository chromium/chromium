// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfoCallback;
import org.chromium.components.commerce.core.ShoppingService.PricePoint;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;

/** Tests for {@link PriceInsightsBottomSheetCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PriceInsightsBottomSheetCoordinatorTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private Tab mMockTab;
    @Mock private TabModelSelector mMockTabModelSelector;
    @Mock private ShoppingService mMockShoppingService;
    @Mock private Profile mMockProfile;
    @Mock private PriceInsightsDelegate mMockPriceInsightsDelegate;
    @Mock private ObservableSupplier<Boolean> mMockPriceTrackingStateSupplier;

    @Captor private ArgumentCaptor<PriceInsightsBottomSheetContent> mBottomSheetContentCaptor;

    @Captor private ArgumentCaptor<EmptyBottomSheetObserver> mBottomSheetObserverCaptor;

    private static final String PRODUCT_TITLE = "Testing Sneaker";
    private static final String PRICE_TRACKING_DESCRIPTION =
            "Get alerts when the price drops on any site across the web";
    private static final String PRICE_TRACKING_DISABLED_BUTTON_TEXT = "Track";
    private static final String PRICE_HISTORY_TITLE = "Price history across the web";
    private static final String OPEN_URL_TITLE = "Search buying options";
    private static final PriceInsightsInfo PRICE_INSIGHTS_INFO =
            new PriceInsightsInfo(
                    null,
                    "USD",
                    null,
                    null,
                    null,
                    Arrays.asList(new PricePoint("08-08-2024", 65000000L)),
                    JUnitTestGURLs.EXAMPLE_URL,
                    0,
                    false);

    private View mMockPriceHistoryChart;
    private PriceInsightsBottomSheetCoordinator mPriceInsightsCoordinator;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        doReturn(mMockProfile).when(mMockTab).getProfile();
        doReturn(PRODUCT_TITLE).when(mMockTab).getTitle();
        setShoppingServiceGetPriceInsightsInfoForUrl();
        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);
        mMockPriceHistoryChart = new View(sActivity);
        doReturn(mMockPriceHistoryChart)
                .when(mMockPriceInsightsDelegate)
                .getPriceHistoryChartForPriceInsightsInfo(PRICE_INSIGHTS_INFO);
        doReturn(false).when(mMockPriceTrackingStateSupplier).get();
        doReturn(mMockPriceTrackingStateSupplier)
                .when(mMockPriceInsightsDelegate)
                .getPriceTrackingStateSupplier(mMockTab);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPriceInsightsCoordinator =
                            new PriceInsightsBottomSheetCoordinator(
                                    sActivity,
                                    mMockBottomSheetController,
                                    mMockTab,
                                    mMockTabModelSelector,
                                    mMockShoppingService,
                                    mMockPriceInsightsDelegate);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPriceInsightsCoordinator.closeContent();
                });
    }

    @Test
    @SmallTest
    public void testRequestShowContent() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPriceInsightsCoordinator.requestShowContent();
                });
        verify(mMockBottomSheetController, times(1))
                .requestShowContent(mBottomSheetContentCaptor.capture(), eq(true));
        verify(mMockBottomSheetController, times(1))
                .addObserver(mBottomSheetObserverCaptor.capture());
        ScrollView scrollView = (ScrollView) getView(R.id.scroll_view);
        TextView priceTrackingTitle = (TextView) getView(R.id.price_tracking_title);
        TextView priceTrackingDescription = (TextView) getView(R.id.price_tracking_description);
        TextView priceTrackingButton = (TextView) getView(R.id.price_tracking_button);
        Drawable priceTrackingButtonDrawable =
                priceTrackingButton.getCompoundDrawablesRelative()[0];
        TextView priceHistoryTitleView = (TextView) getView(R.id.price_history_title);
        TextView openUrlButton = (TextView) getView(R.id.open_jackpot_url_button);
        Drawable openUrlButtonDrawable = openUrlButton.getCompoundDrawablesRelative()[2];

        assertNotNull(scrollView);
        assertEquals(PRODUCT_TITLE, priceTrackingTitle.getText());
        assertEquals(PRICE_TRACKING_DESCRIPTION, priceTrackingDescription.getText());
        assertEquals(PRICE_TRACKING_DISABLED_BUTTON_TEXT, priceTrackingButton.getText());
        assertNotNull(priceTrackingButtonDrawable);
        assertEquals(
                sActivity.getColor(R.color.price_tracking_ineligible_button_foreground_color),
                priceTrackingButton.getCurrentTextColor());
        assertEquals(
                sActivity.getColor(R.color.price_tracking_ineligible_button_foreground_color),
                priceTrackingButton.getCompoundDrawableTintList().getDefaultColor());
        assertEquals(
                sActivity.getColor(R.color.price_tracking_ineligible_button_background_color),
                priceTrackingButton.getBackgroundTintList().getDefaultColor());
        assertEquals(PRICE_HISTORY_TITLE, priceHistoryTitleView.getText());
        assertEquals(View.VISIBLE, openUrlButton.getVisibility());
        assertEquals(OPEN_URL_TITLE, openUrlButton.getText());
        assertNotNull(openUrlButtonDrawable);
    }

    @Test
    @SmallTest
    public void testCloseContent() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPriceInsightsCoordinator.requestShowContent();
                });
        verify(mMockBottomSheetController, times(1))
                .requestShowContent(mBottomSheetContentCaptor.capture(), eq(true));
        verify(mMockBottomSheetController, times(1))
                .addObserver(mBottomSheetObserverCaptor.capture());
        mPriceInsightsCoordinator.closeContent();
        verify(mMockBottomSheetController, times(1))
                .hideContent(eq(mBottomSheetContentCaptor.getValue()), eq(true));
        verify(mMockBottomSheetController)
                .removeObserver(eq(mBottomSheetObserverCaptor.getValue()));
    }

    @Test
    @SmallTest
    public void testBottomSheetObserverOnContentSheetChanged() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPriceInsightsCoordinator.requestShowContent();
                });
        verify(mMockBottomSheetController, times(1))
                .addObserver(mBottomSheetObserverCaptor.capture());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBottomSheetObserverCaptor.getValue().onSheetContentChanged(null);
                });
        verify(mMockBottomSheetController)
                .removeObserver(eq(mBottomSheetObserverCaptor.getValue()));
    }

    private View getView(@IdRes int viewId) {
        View view = mBottomSheetContentCaptor.getValue().getContentView();
        assertNotNull(view);
        return view.findViewById(viewId);
    }

    private void setShoppingServiceGetPriceInsightsInfoForUrl() {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ((PriceInsightsInfoCallback) invocation.getArgument(1))
                                    .onResult(JUnitTestGURLs.EXAMPLE_URL, PRICE_INSIGHTS_INFO);
                            return null;
                        })
                .when(mMockShoppingService)
                .getPriceInsightsInfoForUrl(any(), any());
    }
}
