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
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** Tests that {@link CustomTabActivity} launches into the correct color scheme. */
@RunWith(ChromeRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
public class LaunchesWithColorSchemeTest {
    private ActivityScenario<CustomTabActivity> mActivityScenario;

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
        }
    }

    @Test
    @SmallTest
    public void testLaunchCustomTabWithColorSchemeDark() {
        mActivityScenario =
                ActivityScenario.launch(
                        createIntentWithColorScheme(CustomTabsIntent.COLOR_SCHEME_DARK));

        mActivityScenario.onActivity(
                activity -> {
                    assertNotNull(activity.getNightModeStateProviderForTesting());
                    assertTrue(activity.getNightModeStateProviderForTesting().isInNightMode());

                    MenuButton menuButtonView = activity.findViewById(R.id.menu_button_wrapper);
                    assertEquals(
                            BrandedColorScheme.APP_DEFAULT,
                            menuButtonView.getBrandedColorSchemeForTesting());
                });
    }

    @Test
    @SmallTest
    public void testLaunchCustomTabWithColorSchemeLight() {
        mActivityScenario =
                ActivityScenario.launch(
                        createIntentWithColorScheme(CustomTabsIntent.COLOR_SCHEME_LIGHT));

        mActivityScenario.onActivity(
                activity -> {
                    assertNotNull(activity.getNightModeStateProviderForTesting());
                    assertFalse(activity.getNightModeStateProviderForTesting().isInNightMode());

                    MenuButton menuButtonView = activity.findViewById(R.id.menu_button_wrapper);
                    assertEquals(
                            BrandedColorScheme.APP_DEFAULT,
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
