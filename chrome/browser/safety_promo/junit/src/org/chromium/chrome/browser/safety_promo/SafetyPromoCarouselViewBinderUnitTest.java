// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link SafetyPromoCarouselViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoCarouselViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mOnClickListener;

    private Context mContext;
    private SafetyPromoCarouselView mView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView =
                (SafetyPromoCarouselView)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.safety_promo_fre_carousel_portrait_view,
                                        /* root= */ null);
        mModel =
                new PropertyModel.Builder(SafetyPromoCarouselProperties.ALL_KEYS)
                        .with(SafetyPromoCarouselProperties.ON_CONTINUE_CLICKED, mOnClickListener)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, SafetyPromoCarouselViewBinder::bind);
    }

    @Test
    public void testModelSet() {
        int titleResId = R.string.safety_fre_promo_password_manager_carousel_title;
        int subtitleResId = R.string.safety_fre_promo_password_manager_carousel_subtitle;
        mModel.set(SafetyPromoCarouselProperties.TITLE_RES_ID, titleResId);
        mModel.set(SafetyPromoCarouselProperties.SUBTITLE_RES_ID, subtitleResId);
        mModel.set(SafetyPromoCarouselProperties.PAGE_COUNT, 3);
        mModel.set(SafetyPromoCarouselProperties.ACTIVE_PAGE_INDEX, 1);

        TextView titleView = mView.findViewById(R.id.safety_promo_carousel_title);
        assertEquals(mContext.getString(titleResId), titleView.getText().toString());
        TextView subtitleView = mView.findViewById(R.id.safety_promo_carousel_subtitle);
        assertEquals(mContext.getString(subtitleResId), subtitleView.getText().toString());

        SafetyPromoPageIndicatorView pageIndicator =
                mView.findViewById(R.id.safety_promo_carousel_page_indicator);
        assertEquals(3, pageIndicator.getPageCountForTesting());
        assertEquals(1, pageIndicator.getActivePositionForTesting());

        View continueButton = mView.findViewById(R.id.fre_continue_button);
        continueButton.performClick();
        verify(mOnClickListener).onClick(continueButton);
    }
}
