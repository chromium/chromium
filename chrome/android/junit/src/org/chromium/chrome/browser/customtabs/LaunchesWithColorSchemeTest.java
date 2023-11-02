// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.display.DisplayAndroidManager;

/**
 * Tests that {@link CustomTabActivity} launches into the correct color scheme.
 */
// clang-format off
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.DisableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
// clang-format on
public class LaunchesWithColorSchemeTest {
    private ActivityScenario<CustomTabActivity> mActivityScenario;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

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
    @SmallTest
    public void testLaunchCustomTabWithColorSchemeDark() {
        mActivityScenario = ActivityScenario.launch(
                createIntentWithColorScheme(CustomTabsIntent.COLOR_SCHEME_DARK));

        mActivityScenario.onActivity(activity -> {
            assertNotNull(activity.getNightModeStateProviderForTesting());
            assertTrue(activity.getNightModeStateProviderForTesting().isInNightMode());

            MenuButton menuButtonView = activity.findViewById(R.id.menu_button_wrapper);
            assertEquals(BrandedColorScheme.APP_DEFAULT,
                    menuButtonView.getBrandedColorSchemeForTesting());
        });
    }

    @Test
    @SmallTest
    public void testLaunchCustomTabWithColorSchemeLight() {
        mActivityScenario = ActivityScenario.launch(
                createIntentWithColorScheme(CustomTabsIntent.COLOR_SCHEME_LIGHT));

        mActivityScenario.onActivity(activity -> {
            assertNotNull(activity.getNightModeStateProviderForTesting());
            assertFalse(activity.getNightModeStateProviderForTesting().isInNightMode());

            MenuButton menuButtonView = activity.findViewById(R.id.menu_button_wrapper);
            assertEquals(BrandedColorScheme.APP_DEFAULT,
                    menuButtonView.getBrandedColorSchemeForTesting());
        });
    }

    private static Intent createIntentWithColorScheme(int colorScheme) {
        Intent intent =
                new Intent(ApplicationProvider.getApplicationContext(), CustomTabActivity.class);
        intent.putExtra(CustomTabsIntent.EXTRA_COLOR_SCHEME, colorScheme);
        return intent;
    }
}
