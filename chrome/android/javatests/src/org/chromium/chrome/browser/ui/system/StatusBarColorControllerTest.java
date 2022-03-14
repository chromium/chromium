// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import android.app.Activity;
import android.graphics.Color;
import android.os.Build;

import androidx.annotation.ColorInt;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.util.ColorUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * {@link StatusBarColorController} tests.
 * There are additional status bar color tests in {@link BrandColorTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
public class StatusBarColorControllerTest {
    // clang-format on
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private @ColorInt int mScrimColor;

    @Before
    public void setUp() {
        mScrimColor = sActivityTestRule.getActivity().getColor(
                org.chromium.chrome.R.color.default_scrim_color);
    }

    /**
     * Test that the status bar color is toggled when toggling incognito while in overview mode.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testColorToggleIncognitoInOverview() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int expectedOverviewStandardColor = defaultColorFallbackToBlack(
                ChromeColors.getPrimaryBackgroundColor(activity, false));
        final int expectedOverviewIncognitoColor =
                defaultColorFallbackToBlack(ChromeColors.getPrimaryBackgroundColor(activity, true));

        sActivityTestRule.loadUrlInNewTab(
                "about:blank", true /* incognito */, TabLaunchType.FROM_CHROME_UI);
        TabModelSelector tabModelSelector = activity.getTabModelSelector();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tabModelSelector.selectModel(true /* incognito */); });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getLayoutManager().showLayout(LayoutType.TAB_SWITCHER, false /* animate */);
        });

        waitForStatusBarColor(activity, expectedOverviewIncognitoColor);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tabModelSelector.selectModel(false /* incognito */); });
        ThemeTestUtils.assertStatusBarColor(activity, expectedOverviewStandardColor);
    }

    /**
     * Test that the default color (and not the active tab's brand color) is used in overview mode.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testBrandColorIgnoredInOverview() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int expectedDefaultStandardColor =
                defaultColorFallbackToBlack(ChromeColors.getDefaultThemeColor(activity, false));

        String pageWithBrandColorUrl = sActivityTestRule.getTestServer().getURL(
                "/chrome/test/data/android/theme_color_test.html");
        sActivityTestRule.loadUrl(pageWithBrandColorUrl);
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);
        waitForStatusBarColor(activity, Color.RED);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getLayoutManager().showLayout(LayoutType.TAB_SWITCHER, false /* animate */);
        });
        waitForStatusBarColor(activity, expectedDefaultStandardColor);
    }

    /**
     * Test that the status indicator color is included in the color calculation correctly.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testColorWithStatusIndicator() {
        final ChromeActivity activity = sActivityTestRule.getActivity();
        final StatusBarColorController statusBarColorController =
                sActivityTestRule.getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getStatusBarColorController();
        final Supplier<Integer> statusBarColor = () -> activity.getWindow().getStatusBarColor();
        final int initialColor = statusBarColor.get();

        // Initially, StatusBarColorController#getStatusBarColorWithoutStatusIndicator should return
        // the same color as the current status bar color.
        Assert.assertEquals(
                "Wrong initial value returned by #getStatusBarColorWithoutStatusIndicator().",
                initialColor, statusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Set a status indicator color.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.onStatusIndicatorColorChanged(Color.BLUE));

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            Assert.assertEquals("Wrong status bar color for Android L.",
                    ColorUtils.getDarkenedColorForStatusBar(Color.BLUE),
                    statusBarColor.get().intValue());
        } else {
            Assert.assertEquals("Wrong status bar color for Android M+.", Color.BLUE,
                    statusBarColor.get().intValue());
        }

        // StatusBarColorController#getStatusBarColorWithoutStatusIndicator should still return the
        // initial color.
        Assert.assertEquals("Wrong value returned by #getStatusBarColorWithoutStatusIndicator().",
                initialColor, statusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Set scrim.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.setStatusBarScrimFraction(.5f));

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            // If we're already darkening the color for Android L, scrim shouldn't be applied.
            Assert.assertEquals("Wrong status bar color w/ scrim for Android L.",
                    ColorUtils.getDarkenedColorForStatusBar(Color.BLUE),
                    statusBarColor.get().intValue());
        } else {
            // Otherwise, the resulting color should be a scrimmed version of the status bar color.
            Assert.assertEquals("Wrong status bar color w/ scrim for Android M+.",
                    getScrimmedColor(Color.BLUE, .5f), statusBarColor.get().intValue());
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Remove scrim.
            statusBarColorController.setStatusBarScrimFraction(.0f);
            // Set the status indicator color to the default, i.e. transparent.
            statusBarColorController.onStatusIndicatorColorChanged(Color.TRANSPARENT);
        });

        // Now, the status bar color should be back to the initial color.
        Assert.assertEquals(
                "Wrong status bar color after the status indicator color is set to default.",
                initialColor, statusBarColor.get().intValue());
    }

    private int defaultColorFallbackToBlack(int color) {
        return (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) ? Color.BLACK : color;
    }

    private int getScrimmedColor(@ColorInt int color, float fraction) {
        final float scrimColorAlpha = (mScrimColor >>> 24) / 255f;
        final int scrimColorOpaque = mScrimColor & 0xFF000000;
        return ColorUtils.getColorWithOverlay(color, scrimColorOpaque, fraction * scrimColorAlpha);
    }

    private void waitForStatusBarColor(Activity activity, int expectedColor)
            throws ExecutionException, TimeoutException {
        final int actualExpectedColor = Build.VERSION.SDK_INT < Build.VERSION_CODES.M
                ? ColorUtils.getDarkenedColorForStatusBar(expectedColor)
                : expectedColor;
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    activity.getWindow().getStatusBarColor(), Matchers.is(actualExpectedColor));
        }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
