// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.safety_promo.R;

/** Unit tests for {@link SafetyPromoBottomSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoBottomSheetViewUnitTest {
    @Test
    @Config(qualifiers = "w411dp")
    public void testConfigurationChange_updatesPaddingHorizontal() {
        Context context = ApplicationProvider.getApplicationContext();
        context.setTheme(R.style.Theme_BrowserUI_DayNight);
        SafetyPromoBottomSheetView view =
                (SafetyPromoBottomSheetView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.safety_promo_show_me_how_bottom_sheet,
                                        /* root= */ null);
        View scrollContent = view.findViewById(R.id.safety_promo_bottom_sheet_scroll_content);
        int narrowPaddingPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.safety_fre_bottom_sheet_padding_horizontal);
        assertEquals(narrowPaddingPx, scrollContent.getPaddingStart());
        assertEquals(narrowPaddingPx, scrollContent.getPaddingEnd());

        RuntimeEnvironment.setQualifiers("w891dp");
        view.dispatchConfigurationChanged(context.getResources().getConfiguration());

        int widePaddingPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.safety_fre_bottom_sheet_padding_horizontal);
        assertNotEquals(narrowPaddingPx, widePaddingPx);
        assertEquals(widePaddingPx, scrollContent.getPaddingStart());
        assertEquals(widePaddingPx, scrollContent.getPaddingEnd());

        RuntimeEnvironment.setQualifiers("w411dp");
        view.dispatchConfigurationChanged(context.getResources().getConfiguration());

        assertEquals(narrowPaddingPx, scrollContent.getPaddingStart());
        assertEquals(narrowPaddingPx, scrollContent.getPaddingEnd());
    }
}
