// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.view.View;

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

    @Mock private SafetyPromoCarouselView mView;
    @Mock private View.OnClickListener mOnClickListener;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel(SafetyPromoCarouselProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, SafetyPromoCarouselViewBinder::bind);
    }

    @Test
    public void testSetTitleText() {
        int titleResId = R.string.safety_fre_promo_password_manager_carousel_title;
        mModel.set(SafetyPromoCarouselProperties.TITLE_RES_ID, titleResId);
        verify(mView).setTitleText(eq(titleResId));
    }

    @Test
    public void testSetSubtitleText() {
        int subtitleResId = R.string.safety_fre_promo_password_manager_carousel_subtitle;
        mModel.set(SafetyPromoCarouselProperties.SUBTITLE_RES_ID, subtitleResId);
        verify(mView).setSubtitleText(eq(subtitleResId));
    }

    @Test
    public void testSetOnContinueClicked() {
        mModel =
                new PropertyModel.Builder(SafetyPromoCarouselProperties.ALL_KEYS)
                        .with(SafetyPromoCarouselProperties.ON_CONTINUE_CLICKED, mOnClickListener)
                        .build();
        SafetyPromoCarouselViewBinder.bind(
                mModel, mView, SafetyPromoCarouselProperties.ON_CONTINUE_CLICKED);
        verify(mView).setContinueButtonOnClickListener(eq(mOnClickListener));
    }
}
