// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Color;
import android.os.Build;
import android.view.WindowManager;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Tests status bar color transitions for a standalone webapp with the short-edges cutout feature
 * enabled, as the page opts in and out of viewport-fit=cover.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests activity startup behavior.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// Standalone webapps run in a windowed container on tablets/desktop where the cutout treatment
// does not apply.
@DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
public class WebappThemeColorShortEdgesCutoutTest {
    private static final int PAGE_THEME_COLOR = Color.RED;
    private static final int UPDATED_PAGE_THEME_COLOR = Color.BLUE;

    @Rule public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Test
    @LargeTest
    @Feature({"Webapps"})
    @MaxAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Features.DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE})
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testThemeColorTransitionsWithViewportFitChanges() throws Exception {
        String pageWithThemeColorUrl =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/theme_color_test.html");
        mActivityTestRule.startWebappActivity(
                mActivityTestRule
                        .createIntent()
                        .putExtra(WebappConstants.EXTRA_URL, pageWithThemeColorUrl));
        WebappActivity activity = mActivityTestRule.getActivity();

        // Themed state: status bar uses the page theme-color.
        ThemeTestUtils.waitForThemeColor(activity, PAGE_THEME_COLOR);
        waitForStatusBarColor(activity, PAGE_THEME_COLOR);
        waitForContentFitsWindowInsets(activity, true);
        waitForLayoutInDisplayCutoutMode(
                activity, WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);

        // Opting into viewport-fit=cover makes the window status bar transparent.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "var meta = document.createElement('meta');"
                        + "meta.setAttribute('name', 'viewport');"
                        + "meta.setAttribute('content', 'viewport-fit=cover');"
                        + "document.head.appendChild(meta);"
                        + "window.__viewportMeta = meta;");
        waitForLayoutInDisplayCutoutMode(
                activity, WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
        waitForContentFitsWindowInsets(activity, false);
        waitForWindowStatusBarColor(activity, Color.TRANSPARENT);

        // Changing the theme-color while edge-to-edge keeps the window transparent; the tracked
        // color follows the theme.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "document.querySelector('meta[name=theme-color]')"
                        + ".setAttribute('content', '#0000ff');");
        ThemeTestUtils.waitForThemeColor(activity, UPDATED_PAGE_THEME_COLOR);
        waitForStatusBarColor(activity, UPDATED_PAGE_THEME_COLOR);
        waitForWindowStatusBarColor(activity, Color.TRANSPARENT);

        // Opting out restores the themed status bar with the updated color.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "window.__viewportMeta.setAttribute('content', 'viewport-fit=auto');");
        waitForLayoutInDisplayCutoutMode(
                activity, WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
        waitForContentFitsWindowInsets(activity, true);
        waitForStatusBarColor(activity, UPDATED_PAGE_THEME_COLOR);
    }

    @Test
    @LargeTest
    @Feature({"Webapps"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures({
        ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE,
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE
    })
    public void testThemeColorTransitionsWithViewportFitChanges_EdgeToEdgeEverywhere()
            throws Exception {
        String pageWithThemeColorUrl =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/theme_color_test.html");
        mActivityTestRule.startWebappActivity(
                mActivityTestRule
                        .createIntent()
                        .putExtra(WebappConstants.EXTRA_URL, pageWithThemeColorUrl));
        WebappActivity activity = mActivityTestRule.getActivity();

        // Themed state: status bar uses the page theme-color.
        ThemeTestUtils.waitForThemeColor(activity, PAGE_THEME_COLOR);
        waitForStatusBarColor(activity, PAGE_THEME_COLOR);
        waitForContentFitsWindowInsets(activity, false);
        waitForLayoutInDisplayCutoutMode(
                activity, WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);

        // Opting into viewport-fit=cover makes the window status bar transparent.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "var meta = document.createElement('meta');"
                        + "meta.setAttribute('name', 'viewport');"
                        + "meta.setAttribute('content', 'viewport-fit=cover');"
                        + "document.head.appendChild(meta);"
                        + "window.__viewportMeta = meta;");
        waitForLayoutInDisplayCutoutMode(
                activity, WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
        waitForContentFitsWindowInsets(activity, false);
        waitForWindowStatusBarColor(activity, Color.TRANSPARENT);

        // Changing the theme-color while edge-to-edge keeps the window transparent; the tracked
        // color follows the theme.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "document.querySelector('meta[name=theme-color]')"
                        + ".setAttribute('content', '#0000ff');");
        ThemeTestUtils.waitForThemeColor(activity, UPDATED_PAGE_THEME_COLOR);
        waitForStatusBarColor(activity, UPDATED_PAGE_THEME_COLOR);
        waitForWindowStatusBarColor(activity, Color.TRANSPARENT);

        // Opting out restores the themed status bar with the updated color.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "window.__viewportMeta.setAttribute('content', 'viewport-fit=auto');");
        waitForLayoutInDisplayCutoutMode(
                activity, WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
        waitForContentFitsWindowInsets(activity, false);
        waitForStatusBarColor(activity, UPDATED_PAGE_THEME_COLOR);
    }

    /**
     * Waits for the status bar color tracked by {@link
     * org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper}. This is the color painted for
     * the status bar region even when the platform enforces edge-to-edge and the window status bar
     * itself is transparent.
     */
    private static void waitForStatusBarColor(WebappActivity activity, int expectedColor) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getEdgeToEdgeManager()
                                    .getEdgeToEdgeSystemBarColorHelper()
                                    .getStatusBarColor(),
                            Matchers.is(expectedColor));
                });
    }

    /** Waits for the window's own status bar color. */
    private static void waitForWindowStatusBarColor(WebappActivity activity, int expectedColor) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getWindow().getStatusBarColor(), Matchers.is(expectedColor));
                });
    }

    private static void waitForContentFitsWindowInsets(WebappActivity activity, boolean expected) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getEdgeToEdgeManager().shouldContentFitsWindowInsets(),
                            Matchers.is(expected));
                });
    }

    private static void waitForLayoutInDisplayCutoutMode(WebappActivity activity, int expected) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getWindow().getAttributes().layoutInDisplayCutoutMode,
                            Matchers.is(expected));
                });
    }
}
