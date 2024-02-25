// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.WindowManager;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;

/** Unit tests for {@link ChromeBaseAppCompatActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeBaseAppCompatActivityUnitTest {
    private static final float MOCK_REAL_DISPLAY_DENSITY = 1.0f;
    private static final int MOCK_REAL_DISPLAY_DENSITY_DPI = 160;
    private static final int MOCK_REAL_DISPLAY_WIDTH_PIXELS = 600;
    private static final int MOCK_REAL_DISPLAY_HEIGHT_PIXELS = 300;

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    @Mock private Context mContext;
    @Mock private WindowManager mWindowManager;
    @Mock private Display mDisplay;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mContext.getSystemService(eq(Context.WINDOW_SERVICE))).thenReturn(mWindowManager);
        when(mContext.getResources())
                .thenReturn(ContextUtils.getApplicationContext().getResources());
        when(mWindowManager.getDefaultDisplay()).thenReturn(mDisplay);
        doAnswer(
                        (invocation) -> {
                            DisplayMetrics realDisplayMetrics = invocation.getArgument(0);
                            realDisplayMetrics.density = MOCK_REAL_DISPLAY_DENSITY;
                            realDisplayMetrics.densityDpi = MOCK_REAL_DISPLAY_DENSITY_DPI;
                            realDisplayMetrics.widthPixels = MOCK_REAL_DISPLAY_WIDTH_PIXELS;
                            realDisplayMetrics.heightPixels = MOCK_REAL_DISPLAY_HEIGHT_PIXELS;
                            return null;
                        })
                .when(mDisplay)
                .getRealMetrics(any());
    }

    @Test
    @MediumTest
    public void testApplyOverridesForAutomotive_onAutomotiveDevice_scaleUpUI() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);

        Configuration config = new Configuration();
        config.densityDpi = MOCK_REAL_DISPLAY_DENSITY_DPI;
        config.screenWidthDp = MOCK_REAL_DISPLAY_WIDTH_PIXELS;
        config.screenHeightDp = MOCK_REAL_DISPLAY_HEIGHT_PIXELS;
        config.smallestScreenWidthDp = 0;
        ChromeBaseAppCompatActivity.applyOverridesForAutomotive(mContext, config);

        float automotiveScaleUpFactor =
                (float) DisplayMetrics.DENSITY_220 / (float) MOCK_REAL_DISPLAY_DENSITY_DPI;
        assertEquals(
                "Density dpi should be scaled up from the real display metric " + "on automotive.",
                DisplayMetrics.DENSITY_220,
                config.densityDpi);
        assertEquals(
                "Screen width in density pixels should be scaled down from the "
                        + "real display metric on automotive.",
                (int) (MOCK_REAL_DISPLAY_WIDTH_PIXELS / automotiveScaleUpFactor),
                config.screenWidthDp);
        assertEquals(
                "Screen height in density pixels should be scaled down from the "
                        + "real display metric on automotive.",
                (int) (MOCK_REAL_DISPLAY_HEIGHT_PIXELS / automotiveScaleUpFactor),
                config.screenHeightDp);
        assertEquals(
                "Smallest screen width should be overridden to match the screen height, "
                        + "as it is lower than the screen width.",
                config.screenHeightDp,
                config.smallestScreenWidthDp);
    }

    @Test
    @MediumTest
    public void testApplyOverridesForAutomotive_onNonAutomotiveDevice_noUIScaleUp() {
        Configuration config = new Configuration();
        config.densityDpi = MOCK_REAL_DISPLAY_DENSITY_DPI;
        config.screenWidthDp = MOCK_REAL_DISPLAY_WIDTH_PIXELS;
        config.screenHeightDp = MOCK_REAL_DISPLAY_HEIGHT_PIXELS;
        config.smallestScreenWidthDp = 0;
        ChromeBaseAppCompatActivity.applyOverridesForAutomotive(mContext, config);

        assertEquals(
                "Density dpi should not be scaled up from the real display metric "
                        + "on non-automotive devices.",
                MOCK_REAL_DISPLAY_DENSITY_DPI,
                config.densityDpi);
        assertEquals(
                "Screen width in density pixels should not be scaled down from the "
                        + "real display metric on non-automotive devices.",
                MOCK_REAL_DISPLAY_WIDTH_PIXELS,
                config.screenWidthDp);
        assertEquals(
                "Screen height in density pixels should not be scaled down from the "
                        + "real display metric on non-automotive devices.",
                MOCK_REAL_DISPLAY_HEIGHT_PIXELS,
                config.screenHeightDp);
        assertEquals(
                "Smallest screen width should not have changed.", 0, config.smallestScreenWidthDp);
    }
}
