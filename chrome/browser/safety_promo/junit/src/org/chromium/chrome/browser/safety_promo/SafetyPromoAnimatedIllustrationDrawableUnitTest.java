// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link SafetyPromoAnimatedIllustrationDrawable}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoAnimatedIllustrationDrawableUnitTest {
    private static final int TOP_LEFT = 0;
    private static final int TOP_RIGHT = 3;
    private static final int BOTTOM_LEFT = 8;
    private static final int BOTTOM_RIGHT = 11;

    private SafetyPromoAnimatedIllustrationDrawable mDrawable;
    private int mWidthPx;
    private int mHeightPx;
    private int mIconSizePx;
    private int mLogoSizePx;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        context.setTheme(R.style.Theme_BrowserUI_DayNight);
        Resources res = context.getResources();
        mWidthPx = res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_width);
        mHeightPx = res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_height);
        mIconSizePx = res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_icon_size);
        mLogoSizePx = res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_logo_size);
        mDrawable = SafetyPromoAnimatedIllustrationDrawable.create(context);
    }

    @Test
    public void testIntrinsicSize_matchesDimens() {
        assertEquals(mWidthPx, mDrawable.getIntrinsicWidth());
        assertEquals(mHeightPx, mDrawable.getIntrinsicHeight());
    }

    @Test
    public void testOnBoundsChange_centersLayoutInBounds() {
        mDrawable.setBounds(10, 20, 10 + mWidthPx + 40, 20 + mHeightPx + 40);
        int left = 30;
        int top = 40;
        int right = left + mWidthPx;
        int bottom = top + mHeightPx;

        int logoLeft = left + (mWidthPx - mLogoSizePx) / 2;
        int logoTop = top + (mHeightPx - mLogoSizePx) / 2;
        assertEquals(
                new Rect(logoLeft, logoTop, logoLeft + mLogoSizePx, logoTop + mLogoSizePx),
                mDrawable.getLogoForTesting().getBounds());

        List<Drawable> icons = mDrawable.getIconsForTesting();
        assertEquals(
                new Rect(left, top, left + mIconSizePx, top + mIconSizePx),
                icons.get(TOP_LEFT).getBounds());
        assertEquals(
                new Rect(right - mIconSizePx, top, right, top + mIconSizePx),
                icons.get(TOP_RIGHT).getBounds());
        assertEquals(
                new Rect(right - mIconSizePx, bottom - mIconSizePx, right, bottom),
                icons.get(BOTTOM_RIGHT).getBounds());
        assertEquals(
                new Rect(left, bottom - mIconSizePx, left + mIconSizePx, bottom),
                icons.get(BOTTOM_LEFT).getBounds());
    }
}
