// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** Tests that {@link CustomTabActivity} launches into the correct color scheme. */
@RunWith(ChromeRobolectricTestRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    BaseSwitches.DISABLE_NATIVE_INITIALIZATION
})
public class LaunchesWithColorSchemeTest {
    private static final int TEST_INITIAL_BG_COLOR = 0xFF123456;

    private ActivityScenario<? extends CustomTabActivity> mActivityScenario;

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
        }
    }

    @Test
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

    @Test
    public void testTrustedInitialBackgroundColorUpdatesCompositorView() {
        Intent intent = createIntentWithColorScheme(CustomTabsIntent.COLOR_SCHEME_LIGHT);
        IntentUtils.addTrustedIntentExtras(intent);
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_INITIAL_BACKGROUND_COLOR, TEST_INITIAL_BG_COLOR);

        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(
                activity -> {
                    CompositorViewHolder viewHolder =
                            activity.findViewById(R.id.compositor_view_holder);
                    assertNotNull(viewHolder);
                    ColorDrawable bg =
                            (ColorDrawable) viewHolder.getCompositorView().getBackground();
                    assertNotNull(bg);
                    assertEquals(TEST_INITIAL_BG_COLOR, bg.getColor());
                });
    }

    @Test
    public void testUntrustedInitialBackgroundColorIsIgnored() {
        Intent intent = createIntentWithColorScheme(CustomTabsIntent.COLOR_SCHEME_LIGHT);
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_INITIAL_BACKGROUND_COLOR, TEST_INITIAL_BG_COLOR);

        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(
                activity -> {
                    CompositorViewHolder viewHolder =
                            activity.findViewById(R.id.compositor_view_holder);
                    assertNotNull(viewHolder);
                    ColorDrawable bg =
                            (ColorDrawable) viewHolder.getCompositorView().getBackground();
                    assertNotNull(bg);
                    assertEquals(
                            ChromeColors.getPrimaryBackgroundColor(activity, false), bg.getColor());
                });
    }

    @Test
    public void testTranslucentBackgroundSkipsInitialBackgroundColor() {
        Intent intent =
                new Intent(
                        ApplicationProvider.getApplicationContext(),
                        TranslucentCustomTabActivity.class);
        intent.putExtra(CustomTabsIntent.EXTRA_COLOR_SCHEME, CustomTabsIntent.COLOR_SCHEME_LIGHT);
        IntentUtils.addTrustedIntentExtras(intent);
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_INITIAL_BACKGROUND_COLOR, TEST_INITIAL_BG_COLOR);
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_TRANSLUCENT_BACKGROUND, Color.argb(150, 0, 0, 0));

        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(
                activity -> {
                    CompositorViewHolder viewHolder =
                            activity.findViewById(R.id.compositor_view_holder);
                    assertNotNull(viewHolder);
                    assertEquals(View.INVISIBLE, viewHolder.getVisibility());
                    ColorDrawable bg =
                            (ColorDrawable) viewHolder.getCompositorView().getBackground();
                    assertNotNull(bg);
                    assertEquals(
                            ChromeColors.getPrimaryBackgroundColor(activity, false), bg.getColor());
                });
    }

    @Test
    public void testTranslucentActivityStillAppliesTrustedInitialBackgroundColor() {
        Intent intent =
                new Intent(
                        ApplicationProvider.getApplicationContext(),
                        TranslucentCustomTabActivity.class);
        intent.putExtra(CustomTabsIntent.EXTRA_COLOR_SCHEME, CustomTabsIntent.COLOR_SCHEME_LIGHT);
        IntentUtils.addTrustedIntentExtras(intent);
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_INITIAL_BACKGROUND_COLOR, TEST_INITIAL_BG_COLOR);

        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(
                activity -> {
                    assertTrue(BaseCustomTabActivity.isWindowInitiallyTranslucent(activity));
                    CompositorViewHolder viewHolder =
                            activity.findViewById(R.id.compositor_view_holder);
                    assertNotNull(viewHolder);
                    ColorDrawable bg =
                            (ColorDrawable) viewHolder.getCompositorView().getBackground();
                    assertNotNull(bg);
                    assertEquals(TEST_INITIAL_BG_COLOR, bg.getColor());
                });
    }

    private static Intent createIntentWithColorScheme(int colorScheme) {
        Intent intent =
                new Intent(ApplicationProvider.getApplicationContext(), CustomTabActivity.class);
        intent.putExtra(CustomTabsIntent.EXTRA_COLOR_SCHEME, colorScheme);
        return intent;
    }
}
