// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

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
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link SafetyPromoBottomSheetViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoBottomSheetViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mOnGotItClickListener;

    private SafetyPromoBottomSheetView mView;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        context.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView =
                (SafetyPromoBottomSheetView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.safety_promo_show_me_how_bottom_sheet,
                                        /* root= */ null);
        PropertyModel model =
                new PropertyModel.Builder(SafetyPromoBottomSheetProperties.ALL_KEYS)
                        .with(
                                SafetyPromoBottomSheetProperties.ON_GOT_IT_CLICKED,
                                mOnGotItClickListener)
                        .build();
        PropertyModelChangeProcessor.create(model, mView, SafetyPromoBottomSheetViewBinder::bind);
    }

    @Test
    public void testGotItButtonClick() {
        View gotItButton = mView.findViewById(R.id.safety_promo_bottom_sheet_got_it_button);
        gotItButton.performClick();

        verify(mOnGotItClickListener).onClick(gotItButton);
    }
}
