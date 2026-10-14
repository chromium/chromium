// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.edge_to_edge.layout.EdgeToEdgeBaseLayout;
import org.chromium.ui.test.util.DeviceRestriction;

/**
 * Tests status bar color transitions for a standalone webapp with the short-edges cutout feature
 * enabled, as the page opts in and out of viewport-fit=cover.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests activity startup behavior.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// Standalone webapps run in a windowed container on tablets/desktop where the cutout treatment
// does not apply. Automotive keeps edge-to-edge everywhere disabled.
@DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class WebappThemeColorShortEdgesCutoutTest {
    private static final int PAGE_THEME_COLOR = Color.RED;
    private static final int UPDATED_PAGE_THEME_COLOR = Color.BLUE;

    /** Opts standalone webapps into the short-edges cutout mode, which is off by default. */
    private static final String SHORT_EDGES_CUTOUT_MODE_STANDALONE =
            ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE + ":enable_standalone/true";

    @Rule public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Test
    @LargeTest
    @Feature({"Webapps"})
    // The coordinated cutout path requires edge-to-edge everywhere, unsupported pre-R.
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @EnableFeatures({SHORT_EDGES_CUTOUT_MODE_STANDALONE, ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE})
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

        // Themed state: the window status bar is transparent from startup (the activity draws
        // edge-to-edge), and EdgeToEdgeBaseLayout pads the content and paints the status bar
        // region with the page theme-color.
        ThemeTestUtils.waitForThemeColor(activity, PAGE_THEME_COLOR);
        waitForStatusBarColor(activity, PAGE_THEME_COLOR);
        waitForWindowStatusBarColor(activity, Color.TRANSPARENT);
        waitForWebappDrawsEdgeToEdge(activity, false);
        waitForEdgeToEdgeLayoutTopPadding(activity, /* padded= */ true);
        waitForLayoutInDisplayCutoutMode(
                activity, WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);

        // Opting into viewport-fit=cover withholds the insets, so the layout stops padding and
        // the content draws under the (still transparent) status bar.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "var meta = document.createElement('meta');"
                        + "meta.setAttribute('name', 'viewport');"
                        + "meta.setAttribute('content', 'viewport-fit=cover');"
                        + "document.head.appendChild(meta);"
                        + "window.__viewportMeta = meta;");
        waitForLayoutInDisplayCutoutMode(
                activity, WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
        waitForWebappDrawsEdgeToEdge(activity, true);
        waitForEdgeToEdgeLayoutTopPadding(activity, /* padded= */ false);
        waitForWindowStatusBarColor(activity, Color.TRANSPARENT);

        // Changing the theme-color while edge-to-edge keeps the window transparent; the tracked
        // color follows the theme.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "document.querySelector('meta[name=theme-color]')"
                        + ".setAttribute('content', '#0000ff');");
        ThemeTestUtils.waitForThemeColor(activity, UPDATED_PAGE_THEME_COLOR);
        waitForStatusBarColor(activity, UPDATED_PAGE_THEME_COLOR);
        waitForWindowStatusBarColor(activity, Color.TRANSPARENT);

        // Opting out restores the padded layout, painted with the updated theme color.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "window.__viewportMeta.setAttribute('content', 'viewport-fit=auto');");
        waitForLayoutInDisplayCutoutMode(
                activity, WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
        waitForWebappDrawsEdgeToEdge(activity, false);
        waitForEdgeToEdgeLayoutTopPadding(activity, /* padded= */ true);
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

    /** Waits until the webapp inset consumer withholds (or restores) the system bar insets. */
    private static void waitForWebappDrawsEdgeToEdge(WebappActivity activity, boolean expected) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            assumeNonNull(activity.getWebappInsetsConsumer())
                                    .getDrawEdgeToEdgeSupplier()
                                    .get(),
                            Matchers.is(expected));
                });
    }

    /** Waits until the edge-to-edge layout pads (or stops padding) the content at the top. */
    private static void waitForEdgeToEdgeLayoutTopPadding(WebappActivity activity, boolean padded) {
        CriteriaHelper.pollUiThread(
                () -> {
                    EdgeToEdgeBaseLayout layout =
                            findEdgeToEdgeLayout(activity.getWindow().getDecorView());
                    Criteria.checkThat(layout, Matchers.notNullValue());
                    Criteria.checkThat(
                            layout.getPaddingTop(),
                            padded ? Matchers.greaterThan(0) : Matchers.is(0));
                });
    }

    private static @Nullable EdgeToEdgeBaseLayout findEdgeToEdgeLayout(View root) {
        if (root instanceof EdgeToEdgeBaseLayout layout) return layout;
        if (root instanceof ViewGroup group) {
            for (int i = 0; i < group.getChildCount(); i++) {
                EdgeToEdgeBaseLayout layout = findEdgeToEdgeLayout(group.getChildAt(i));
                if (layout != null) return layout;
            }
        }
        return null;
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
