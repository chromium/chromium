// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.graphics.Color;
import android.os.Build;
import android.view.View;

import androidx.browser.customtabs.CustomTabColorSchemeParams;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.util.ColorUtils;

/**
 * Tests for Custom UI available in Custom Tabs.
 */
// clang-format off
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
// clang-format on
public class CustomTabsUiTest {
    private ActivityScenario<CustomTabActivity> mActivityScenario;

    @Rule
    public TestRule mCommandLineFlagsRule = CommandLineFlags.getTestRule();
    private CustomTabsIntent.Builder mIntent = new CustomTabsIntent.Builder();

    private void launch(ActivityScenario.ActivityAction<CustomTabActivity> onActivity) {
        Intent intent = mIntent.build().intent;
        intent.setClass(ApplicationProvider.getApplicationContext(), CustomTabActivity.class);

        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(onActivity);
    }

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
        }

        // DisplayAndroidManager will reuse the Display between tests. This can cause
        // AsyncInitializationActivity#applyOverrides to set incorrect smallestWidth.
        DisplayAndroidManager.resetInstanceForTesting();
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    @Test
    public void customTabsToolbarShown() {
        launch(activity -> {
            View toolbarView = activity.findViewById(R.id.toolbar);
            assertTrue(toolbarView instanceof CustomTabToolbar);
        });
    }

    @Test
    public void toolbarColor() {
        int color = Color.RED;

        mIntent.setDefaultColorSchemeParams(
                new CustomTabColorSchemeParams.Builder().setToolbarColor(color).build());

        launch(activity -> {
            CustomTabToolbar toolbar = activity.findViewById(R.id.toolbar);
            assertEquals(color, toolbar.getBackground().getColor());
        });
    }

    @Test
    public void toolbarColor_tintsMenuButtons_whenDark() {
        mIntent.setDefaultColorSchemeParams(
                new CustomTabColorSchemeParams.Builder().setToolbarColor(Color.BLACK).build());

        launch(activity -> {
            MenuButton menuButtonView = activity.findViewById(R.id.menu_button_wrapper);
            assertTrue(menuButtonView.getUseLightDrawablesForTesting());
        });
    }

    @Test
    public void toolbarColor_tintsMenuButtons_whenLight() {
        mIntent.setDefaultColorSchemeParams(
                new CustomTabColorSchemeParams.Builder().setToolbarColor(Color.WHITE).build());

        launch(activity -> {
            MenuButton menuButtonView = activity.findViewById(R.id.menu_button_wrapper);
            assertFalse(menuButtonView.getUseLightDrawablesForTesting());
        });
    }

    @Test
    public void toolbarColor_setsStatusBarColor() {
        int color = Color.RED;
        mIntent.setDefaultColorSchemeParams(
                new CustomTabColorSchemeParams.Builder().setToolbarColor(color).build());

        launch(activity -> assertEquals(color, activity.getWindow().getStatusBarColor()));
    }

    @Test
    public void toolbarColor_setsStatusBarColor_darkenedOnLollipop() {
        // @Config(..., sdk = Build.VERSION_CODES.LOLLIPOP) doesn't work. See crbug.com/944476.
        ReflectionHelpers.setStaticField(
                Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.LOLLIPOP);

        int color = Color.RED;
        mIntent.setDefaultColorSchemeParams(
                new CustomTabColorSchemeParams.Builder().setToolbarColor(color).build());

        launch(activity
                -> assertEquals(ColorUtils.getDarkenedColorForStatusBar(color),
                        activity.getWindow().getStatusBarColor()));
    }
}
