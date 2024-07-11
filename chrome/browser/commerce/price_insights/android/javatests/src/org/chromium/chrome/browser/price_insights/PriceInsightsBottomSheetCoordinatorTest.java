// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** Tests for {@link PriceInsightsBottomSheetCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PriceInsightsBottomSheetCoordinatorTest extends BlankUiTestActivityTestCase {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private ShoppingService mMockShoppingService;
    @Mock private PriceInsightsBottomSheetMediator mMockMediator;

    @Captor private ArgumentCaptor<PriceInsightsBottomSheetContent> mBottomSheetContentCaptor;

    private Activity mActivity;
    private PriceInsightsBottomSheetCoordinator mPriceInsightsCoordinator;

    @Before
    public void setUp() {
        mActivity = getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPriceInsightsCoordinator =
                            new PriceInsightsBottomSheetCoordinator(
                                    mActivity, mMockBottomSheetController, mMockShoppingService);
                });
        mPriceInsightsCoordinator.setMediatorForTesting(mMockMediator);
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
        View priceHistoryTitleView = getView(R.id.price_history_title);
        assertNotNull(priceHistoryTitleView);
        assertEquals(
                "Price history title should be visible",
                View.VISIBLE,
                priceHistoryTitleView.getVisibility());
    }

    @Test
    @SmallTest
    public void testCloseContent() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPriceInsightsCoordinator.requestShowContent();
                });
        mPriceInsightsCoordinator.closeContent();
        verify(mMockBottomSheetController, times(1))
                .hideContent(mBottomSheetContentCaptor.capture(), eq(true));
    }

    private View getView(int viewId) {
        View view = mBottomSheetContentCaptor.getValue().getContentView();
        assertNotNull(view);

        return view.findViewById(viewId);
    }
}
