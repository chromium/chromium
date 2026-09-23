// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link SafetyPromoPageIndicatorView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoPageIndicatorViewUnitTest {
    private Context mContext;
    private SafetyPromoPageIndicatorView mView;
    private int mDotDiameterPx;
    private int mDotPaddingPx;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView = new SafetyPromoPageIndicatorView(mContext, null);

        mDotDiameterPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.safety_fre_carousel_page_indicator_dot_size);
        mDotPaddingPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.safety_fre_carousel_page_indicator_padding);
    }

    @Test
    public void testOnMeasure_zeroOrOnePage_returnsZeroDimension() {
        mView.setPageCount(0);
        mView.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));

        assertEquals(0, mView.getMeasuredWidth());
        assertEquals(0, mView.getMeasuredHeight());

        mView.setPageCount(1);
        mView.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));

        assertEquals(0, mView.getMeasuredWidth());
        assertEquals(0, mView.getMeasuredHeight());
    }

    @Test
    public void testOnMeasure_multiplePages_returnsCorrectDimension() {
        int pageCount = 3;
        mView.setPageCount(pageCount);

        int expectedWidth =
                (int) Math.ceil(pageCount * mDotDiameterPx + (pageCount - 1) * mDotPaddingPx);
        int expectedHeight = (int) Math.ceil(mDotDiameterPx);

        mView.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));

        assertEquals(expectedWidth, mView.getMeasuredWidth());
        assertEquals(expectedHeight, mView.getMeasuredHeight());
    }
}
