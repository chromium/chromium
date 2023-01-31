// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.Color;

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
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
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
    public static DisableAnimationsTestRule sEnableAnimationsRule =
            new DisableAnimationsTestRule(false);
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private @ColorInt int mScrimColor;
    private OmniboxTestUtils mOmniboxUtils;

    @Before
    public void setUp() {
        mScrimColor = sActivityTestRule.getActivity().getColor(
                org.chromium.chrome.R.color.default_scrim_color);
        mOmniboxUtils = new OmniboxTestUtils(sActivityTestRule.getActivity());
    }

    /**
     * Test that the status bar color is toggled when toggling incognito while in overview mode.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testColorToggleIncognitoInOverview() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int expectedOverviewStandardColor =
                ChromeColors.getPrimaryBackgroundColor(activity, false);
        final int expectedOverviewIncognitoColor =
                ChromeColors.getPrimaryBackgroundColor(activity, true);

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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testBrandColorIgnoredInOverview() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int expectedDefaultStandardColor = ChromeColors.getDefaultThemeColor(activity, false);

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

        Assert.assertEquals("Wrong status bar color.", Color.BLUE, statusBarColor.get().intValue());

        // StatusBarColorController#getStatusBarColorWithoutStatusIndicator should still return the
        // initial color.
        Assert.assertEquals("Wrong value returned by #getStatusBarColorWithoutStatusIndicator().",
                initialColor, statusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Set scrim.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.setStatusBarScrimFraction(.5f));

        // The resulting color should be a scrimmed version of the status bar color.
        Assert.assertEquals("Wrong status bar color w/ scrim.", getScrimmedColor(Color.BLUE, .5f),
                statusBarColor.get().intValue());

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

    /**
     * Test that the theme color is cleared when the Omnibox gains focus.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testBrandColorIgnoredWhenOmniboxIsFocused() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int expectedDefaultStandardColor = ChromeColors.getDefaultThemeColor(activity, false);

        String pageWithBrandColorUrl = sActivityTestRule.getTestServer().getURL(
                "/chrome/test/data/android/theme_color_test.html");
        sActivityTestRule.loadUrl(pageWithBrandColorUrl);
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);
        waitForStatusBarColor(activity, Color.RED);

        mOmniboxUtils.requestFocus();
        waitForStatusBarColor(activity, expectedDefaultStandardColor);
        mOmniboxUtils.clearFocus();
        waitForStatusBarColor(activity, Color.RED);
    }

    /**
     * Test that the theme color is received and cleared when the Omnibox gains focus, given the
     * feature flag OMNIBOX_MATCH_TOOLBAR_AND_STATUS_BAR_COLOR is enabled.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Features.EnableFeatures({ChromeFeatureList.OMNIBOX_MATCH_TOOLBAR_AND_STATUS_BAR_COLOR})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testBrandColorIgnoredWhenOmniboxIsFocused_FeatureMatchToolbarColorEnabled()
            throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int expectedDefaultStandardColor = ChromeColors.getDefaultThemeColor(activity, false);

        String pageWithBrandColorUrl = sActivityTestRule.getTestServer().getURL(
                "/chrome/test/data/android/theme_color_test.html");
        sActivityTestRule.loadUrl(pageWithBrandColorUrl);
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);
        mOmniboxUtils.waitAnimationsComplete();
        waitForStatusBarColor(activity, Color.RED);
        waitForStatusBarColorToMatchToolbarColor(activity);

        mOmniboxUtils.requestFocus();
        mOmniboxUtils.waitAnimationsComplete();
        waitForStatusBarColor(activity, expectedDefaultStandardColor);
        waitForStatusBarColorToMatchToolbarColor(activity);
        mOmniboxUtils.clearFocus();
        mOmniboxUtils.waitAnimationsComplete();
        waitForStatusBarColor(activity, Color.RED);
        waitForStatusBarColorToMatchToolbarColor(activity);
    }

    /**
     * Test that the status indicator color is included in the color calculation correctly, given
     * the feature flag OMNIBOX_MATCH_TOOLBAR_AND_STATUS_BAR_COLOR is enabled.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Features.EnableFeatures({ChromeFeatureList.OMNIBOX_MATCH_TOOLBAR_AND_STATUS_BAR_COLOR})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Status bar is always black on tablets
    public void testColorWithStatusIndicator_FeatureMatchToolbarColorEnabled() {
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

        Assert.assertEquals("Wrong status bar color.", Color.BLUE, statusBarColor.get().intValue());

        // StatusBarColorController#getStatusBarColorWithoutStatusIndicator should still return the
        // initial color.
        Assert.assertEquals("Wrong value returned by #getStatusBarColorWithoutStatusIndicator().",
                initialColor, statusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Set scrim.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.setStatusBarScrimFraction(.5f));

        Assert.assertEquals("Wrong status bar color w/ scrim", getScrimmedColor(Color.BLUE, .5f),
                statusBarColor.get().intValue());

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

    /**
     * Test status bar color for Tab Strip Redesign Folio.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void testStatusBarColorForTabStripRedesignFolioTablet() throws Exception {
        final ChromeActivity activity = sActivityTestRule.getActivity();
        final StatusBarColorController statusBarColorController =
                sActivityTestRule.getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getStatusBarColorController();

        // Before enable tab strip redesign, status bar should be black.
        assertEquals("Wrong initial value returned before enable Tab Strip Redesign Folio",
                Color.BLACK, activity.getWindow().getStatusBarColor());

        // Enable Tab strip redesign folio, and status bar color should update to the same as folio
        // background color.
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.updateStatusBarColor());
        assertEquals("Wrong value returned for Tab Strip Redesign Folio.",
                TabUiThemeUtil.getTabStripBackgroundColor(activity, false),
                activity.getWindow().getStatusBarColor());
    }

    /**
     * Test status bar color for Tab Strip Redesign Detached.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void testStatusBarColorForTabStripRedesignDetachedTablet() throws Exception {
        final ChromeActivity activity = sActivityTestRule.getActivity();
        final StatusBarColorController statusBarColorController =
                sActivityTestRule.getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getStatusBarColorController();

        // Before enable tab strip redesign, status bar should be black.
        assertEquals("Wrong initial value returned before enable Tab Strip Redesign Detached",
                Color.BLACK, activity.getWindow().getStatusBarColor());

        // Enable Tab strip redesign detached, and status bar color should update to the same as
        // detached background color.
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.updateStatusBarColor());
        assertEquals("Wrong value returned for Tab Strip Redesign Detached.",
                TabUiThemeUtil.getTabStripBackgroundColor(activity, false),
                activity.getWindow().getStatusBarColor());
    }

    private int getScrimmedColor(@ColorInt int color, float fraction) {
        final float scrimColorAlpha = (mScrimColor >>> 24) / 255f;
        final int scrimColorOpaque = mScrimColor | 0xFF000000;
        return ColorUtils.getColorWithOverlay(color, scrimColorOpaque, fraction * scrimColorAlpha);
    }

    private void waitForStatusBarColor(Activity activity, int expectedColor)
            throws ExecutionException, TimeoutException {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    activity.getWindow().getStatusBarColor(), Matchers.is(expectedColor));
        }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void waitForStatusBarColorToMatchToolbarColor(Activity activity)
            throws ExecutionException, TimeoutException {
        ToolbarLayout toolbar = activity.findViewById(R.id.toolbar);
        Assert.assertTrue(
                "ToolbarLayout should be of type ToolbarPhone to get and check toolbar background.",
                toolbar instanceof ToolbarPhone);

        final int toolbarColor = ((ToolbarPhone) toolbar).getToolbarBackgroundColor();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(activity.getWindow().getStatusBarColor(), Matchers.is(toolbarColor));
        }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
