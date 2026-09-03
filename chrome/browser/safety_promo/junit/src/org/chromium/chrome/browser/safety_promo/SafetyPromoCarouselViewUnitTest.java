// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.widget.ButtonCompat;

/** Unit tests for {@link SafetyPromoCarouselView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoCarouselViewUnitTest {
    private Context mContext;
    private SafetyPromoCarouselView mView;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mView =
                (SafetyPromoCarouselView)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.safety_promo_fre_carousel_portrait_view,
                                        /* root= */ null);
    }

    @Test
    public void testOnFinishInflate() {
        assertNotNull(mView.getRecyclerView());
        assertNotNull(mView.findViewById(R.id.safety_promo_carousel_title));
        assertNotNull(mView.findViewById(R.id.safety_promo_carousel_subtitle));
        assertNotNull(mView.findViewById(R.id.fre_continue_button));
    }

    @Test
    public void testSetTitleText() {
        int titleResId = R.string.safety_fre_promo_password_manager_carousel_title;
        mView.setTitleText(titleResId);

        TextView titleView = mView.findViewById(R.id.safety_promo_carousel_title);
        assertEquals(mContext.getString(titleResId), titleView.getText().toString());
    }

    @Test
    public void testSetSubtitleText() {
        int subtitleResId = R.string.safety_fre_promo_password_manager_carousel_subtitle;
        mView.setSubtitleText(subtitleResId);

        TextView subtitleView = mView.findViewById(R.id.safety_promo_carousel_subtitle);
        assertEquals(mContext.getString(subtitleResId), subtitleView.getText().toString());
    }

    @Test
    public void testSetContinueButtonOnClickListener() {
        View.OnClickListener mockListener = mock(View.OnClickListener.class);
        mView.setContinueButtonOnClickListener(mockListener);

        ButtonCompat continueButton = mView.findViewById(R.id.fre_continue_button);
        continueButton.performClick();

        verify(mockListener).onClick(continueButton);
    }
}
