// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.color;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.color.AndroidColorRole;
import org.chromium.ui.util.ColorUtils;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.WEB_UI_ANDROID_THEMING)
public class ColorProviderBridgeImplTest {

    private TestActivity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    public void testGetThemeColors_Success() {
        ColorProviderBridgeImpl bridge = new ColorProviderBridgeImpl();
        long[] colors = bridge.getThemeColors(mActivity);

        assertEquals(
                "Array length should match MAX_VALUE + 1",
                AndroidColorRole.MAX_VALUE + 1,
                colors.length);

        long primaryResult = colors[AndroidColorRole.PRIMARY];
        assertNotEquals("Primary color should be valid.", ColorUtils.INVALID_COLOR, primaryResult);

        long inverseSurfaceResult = colors[AndroidColorRole.INVERSE_SURFACE];
        assertNotEquals(
                "Inverse Surface color should be valid.",
                ColorUtils.INVALID_COLOR,
                inverseSurfaceResult);
    }

    @Test
    public void testGetThemeColors_NullContext_FallbackToActivity() {
        ColorProviderBridgeImpl bridge = new ColorProviderBridgeImpl();
        long[] result = bridge.getThemeColors(null);
        assertEquals(
                "Null context must fallback to activity and return color array.",
                AndroidColorRole.MAX_VALUE + 1,
                result.length);
        assertNotEquals(
                "Primary color should be resolved via Activity fallback.",
                ColorUtils.INVALID_COLOR,
                result[AndroidColorRole.PRIMARY]);
    }

    @Test
    public void testGetThemeColors_NoMaterialTheme() {
        ColorProviderBridgeImpl bridge = new ColorProviderBridgeImpl();
        Context appContext = ApplicationProvider.getApplicationContext();
        Context themedContext =
                new ContextThemeWrapper(appContext, android.R.style.Theme_NoTitleBar);

        int originalTheme = appContext.getApplicationInfo().theme;
        appContext.setTheme(android.R.style.Theme_NoTitleBar);

        long[] result = bridge.getThemeColors(themedContext);

        appContext.setTheme(originalTheme);

        assertEquals(
                "Array length should match MAX_VALUE + 1",
                AndroidColorRole.MAX_VALUE + 1,
                result.length);
        assertEquals(
                "Non-Material context must fallback cleanly to sentinel.",
                ColorUtils.INVALID_COLOR,
                result[AndroidColorRole.PRIMARY]);
    }

    @Test(expected = AssertionError.class)
    public void testGetThemeColors_NullContext_NoActivity_AssertsAndReturnsEmptyArray() {
        mActivity.finish();
        ApplicationStatus.destroyForJUnitTests();
        assertNull(ApplicationStatus.getLastTrackedFocusedActivity());

        ColorProviderBridgeImpl bridge = new ColorProviderBridgeImpl();
        bridge.getThemeColors(null);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.WEB_UI_ANDROID_THEMING)
    public void testGetThemeColors_FlagDisabled_ReturnsEmptyArray() {
        ColorProviderBridgeImpl bridge = new ColorProviderBridgeImpl();
        long[] result = bridge.getThemeColors(mActivity);
        assertEquals("Should return empty array when flag is disabled.", 0, result.length);
    }
}
