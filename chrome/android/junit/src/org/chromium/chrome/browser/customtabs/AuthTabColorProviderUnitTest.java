// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;

import androidx.browser.auth.AuthTabColorSchemeParams;
import androidx.browser.auth.AuthTabIntent;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class AuthTabColorProviderUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private AuthTabColorProvider mAuthTabColorProvider;

    @Before
    public void setUp() {
        mActivityScenario.getScenario().onActivity(activity -> mActivity = activity);
    }

    @Test
    public void testReturnsDefaults() {
        Intent intent = new AuthTabIntent.Builder().build().intent;
        AuthTabColorProvider provider =
                new AuthTabColorProvider(intent, mActivity, COLOR_SCHEME_LIGHT);
        int color = ChromeColors.getDefaultThemeColor(mActivity, /* isIncognito= */ false);
        assertEquals("Wrong toolbar color", color, provider.getToolbarColor());
        assertEquals("Wrong bottom bar color", color, provider.getBottomBarColor());
        assertEquals(
                "Initial bg color should be transparent.",
                Color.TRANSPARENT,
                provider.getInitialBackgroundColor());
        assertFalse(
                "Shouldn't have a custom toolbar color specified.",
                provider.hasCustomToolbarColor());
        assertNull("Shouldn't have a navbar color specified.", provider.getNavigationBarColor());
        assertNull(
                "Shouldn't have a navbar divider color specified.",
                provider.getNavigationBarDividerColor());
    }

    @Test
    public void testReturnsLightColors() {
        AuthTabColorSchemeParams params =
                new AuthTabColorSchemeParams.Builder()
                        .setToolbarColor(0xffff0000)
                        .setNavigationBarColor(0x8800ff00)
                        .setNavigationBarDividerColor(0x0000ff)
                        .build();
        Intent intent =
                new AuthTabIntent.Builder()
                        .setColorSchemeParams(COLOR_SCHEME_LIGHT, params)
                        .build()
                        .intent;
        AuthTabColorProvider provider =
                new AuthTabColorProvider(intent, mActivity, COLOR_SCHEME_LIGHT);
        assertTrue(
                "Should have a custom toolbar color specified.", provider.hasCustomToolbarColor());
        assertEquals("Wrong opaque toolbar color", 0xffff0000, provider.getToolbarColor());
        assertEquals(
                "Wrong opaque navigation bar color.",
                0xff00ff00,
                provider.getNavigationBarColor().intValue());
        assertEquals(
                "Wrong navigation bar divider color.",
                0x0000ff,
                provider.getNavigationBarDividerColor().intValue());
    }

    @Test
    public void testReturnsDarkAndDefaultColors() {
        AuthTabColorSchemeParams defaultParams =
                new AuthTabColorSchemeParams.Builder()
                        .setToolbarColor(0xffaabbcc)
                        .setNavigationBarColor(0xffddeeff)
                        .setNavigationBarDividerColor(0x12345678)
                        .build();
        AuthTabColorSchemeParams darkParams =
                new AuthTabColorSchemeParams.Builder().setToolbarColor(0xffff0000).build();
        Intent intent =
                new AuthTabIntent.Builder()
                        .setDefaultColorSchemeParams(defaultParams)
                        .setColorSchemeParams(COLOR_SCHEME_DARK, darkParams)
                        .build()
                        .intent;
        AuthTabColorProvider provider =
                new AuthTabColorProvider(intent, mActivity, COLOR_SCHEME_DARK);
        assertTrue(
                "Should have a custom toolbar color specified.", provider.hasCustomToolbarColor());
        assertEquals("Wrong opaque toolbar color", 0xffff0000, provider.getToolbarColor());
        assertEquals(
                "Wrong navigation bar color.",
                0xffddeeff,
                provider.getNavigationBarColor().intValue());
        assertEquals(
                "Wrong navigation bar divider color.",
                0x12345678,
                provider.getNavigationBarDividerColor().intValue());
    }
}
