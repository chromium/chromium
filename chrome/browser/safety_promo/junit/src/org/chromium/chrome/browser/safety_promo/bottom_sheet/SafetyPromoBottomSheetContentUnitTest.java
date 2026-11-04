// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View.MeasureSpec;
import android.widget.ScrollView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.safety_promo.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;

/** Unit tests for {@link SafetyPromoBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoBottomSheetContentUnitTest {
    private static final float FLOATING_POINT_DELTA = 0.1f;
    private static final int SHEET_WIDTH_PX = 1000;
    private static final int SHEET_HEIGHT_PX = 100;
    private static final int SCROLL_Y_PX = 10;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnDestroyRunnable;

    private SafetyPromoBottomSheetView mContentView;
    private SafetyPromoBottomSheetContent mBottomSheetContent;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        context.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView =
                (SafetyPromoBottomSheetView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.safety_promo_show_me_how_bottom_sheet,
                                        /* root= */ null);
        mBottomSheetContent = new SafetyPromoBottomSheetContent(mContentView, mOnDestroyRunnable);
    }

    @Test
    public void testBasics() {
        assertSame(mContentView, mBottomSheetContent.getContentView());
        assertNull(mBottomSheetContent.getToolbarView());
        assertEquals(ContentPriority.HIGH, mBottomSheetContent.getPriority());
        assertTrue(mBottomSheetContent.swipeToDismissEnabled());
        assertEquals(
                HeightMode.DISABLED,
                mBottomSheetContent.getHalfHeightRatio(),
                FLOATING_POINT_DELTA);
        assertEquals(
                HeightMode.WRAP_CONTENT,
                mBottomSheetContent.getFullHeightRatio(),
                FLOATING_POINT_DELTA);
    }

    @Test
    public void testGetVerticalScrollOffset_isScrollViewScrollOffset() {
        assertEquals(0, mBottomSheetContent.getVerticalScrollOffset());

        // Make the scroll view content taller than the sheet, so that the scroll view can scroll.
        ScrollView scrollView =
                mContentView.findViewById(R.id.safety_promo_bottom_sheet_scroll_view);
        scrollView.getChildAt(0).setMinimumHeight(2 * SHEET_HEIGHT_PX);
        mContentView.measure(
                MeasureSpec.makeMeasureSpec(SHEET_WIDTH_PX, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(SHEET_HEIGHT_PX, MeasureSpec.EXACTLY));
        mContentView.layout(0, 0, SHEET_WIDTH_PX, SHEET_HEIGHT_PX);
        scrollView.scrollTo(0, SCROLL_Y_PX);

        assertEquals(SCROLL_Y_PX, mBottomSheetContent.getVerticalScrollOffset());
    }

    @Test
    public void testDestroy() {
        mBottomSheetContent.destroy();

        verify(mOnDestroyRunnable).run();
    }
}
