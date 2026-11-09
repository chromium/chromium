// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View.MeasureSpec;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link SafetyPromoFirstRunView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoFirstRunViewUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testSetUpAnimatedIllustration_portrait() {
        testSetUpAnimatedIllustrationImpl(R.layout.safety_promo_fre_illustration_portrait_view);
    }

    @Test
    public void testSetUpAnimatedIllustration_landscape() {
        testSetUpAnimatedIllustrationImpl(R.layout.safety_promo_fre_illustration_landscape_view);
    }

    @Test
    public void testIllustrationView_wrapsIntrinsicSize() {
        SafetyPromoFirstRunView view =
                inflateView(R.layout.safety_promo_fre_illustration_portrait_view);
        view.setUpAnimatedIllustration();
        ImageView illustrationView = view.findViewById(R.id.safety_promo_illustration);

        illustrationView.measure(
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

        Resources res = mContext.getResources();
        assertEquals(
                res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_width),
                illustrationView.getMeasuredWidth());
        assertEquals(
                res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_height),
                illustrationView.getMeasuredHeight());
    }

    private void testSetUpAnimatedIllustrationImpl(int layoutId) {
        SafetyPromoFirstRunView view = inflateView(layoutId);
        ImageView illustrationView = view.findViewById(R.id.safety_promo_illustration);
        assertNull(illustrationView.getDrawable());

        view.setUpAnimatedIllustration();

        assertTrue(
                illustrationView.getDrawable() instanceof SafetyPromoAnimatedIllustrationDrawable);
    }

    private SafetyPromoFirstRunView inflateView(int layoutId) {
        return (SafetyPromoFirstRunView)
                LayoutInflater.from(mContext).inflate(layoutId, /* root= */ null);
    }
}
