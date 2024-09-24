// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.util.ColorUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * {@link StatusBarColorController} tests. There are additional status bar color tests in {@link
 * BrandColorTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class StatusBarColorControllerTest {
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
        mScrimColor = sActivityTestRule.getActivity().getColor(R.color.default_scrim_color);
        mOmniboxUtils = new OmniboxTestUtils(sActivityTestRule.getActivity());
    }

    /** Test that the status bar color is toggled when toggling incognito while in overview mode. */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.PHONE}) // Status bar is always black on tablets
    @DisabledTest(message = "crbug.com/353460498")
    public void testColorToggleIncognitoInTabSwitcher() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int expectedOverviewStandardColor =
                ChromeColors.getPrimaryBackgroundColor(activity, false);
        final int expectedOverviewIncognitoColor =
                ChromeColors.getPrimaryBackgroundColor(activity, true);

        sActivityTestRule.loadUrlInNewTab(
                "about:blank", /* incognito= */ true, TabLaunchType.FROM_CHROME_UI);
        TabModelSelector tabModelSelector = activity.getTabModelSelector();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModelSelector.selectModel(/* incognito= */ true);
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.getLayoutManager()
                            .showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false);
                });

        waitForStatusBarColor(activity, expectedOverviewIncognitoColor);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModelSelector.selectModel(/* incognito= */ false);
                });
        ThemeTestUtils.assertStatusBarColor(activity, expectedOverviewStandardColor);
    }

    /**
     * Test that the default color (and not the active tab's brand color) is used in overview mode.
     */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.PHONE}) // Status bar is always black on tablets
    public void testBrandColorIgnoredInTabSwitcher() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int expectedDefaultStandardColor = ChromeColors.getDefaultThemeColor(activity, false);

        String pageWithBrandColorUrl =
                sActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/theme_color_test.html");
        sActivityTestRule.loadUrl(pageWithBrandColorUrl);
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);
        waitForStatusBarColor(activity, Color.RED);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.getLayoutManager()
                            .showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false);
                });
        waitForStatusBarColor(activity, expectedDefaultStandardColor);
    }

    /** Test the color of status bar when used in NTP. */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.PHONE}) // Status bar is always black on tablets
    @DisabledTest(message = "https://issues.chromium.org/issues/341157444")
    public void testStatusBarColorNtp() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int expectedColor =
                ChromeColors.getSurfaceColor(
                        activity, R.dimen.home_surface_background_color_elevation);

        sActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, false);
        NewTabPageTestUtils.waitForNtpLoaded(activity.getActivityTab());

        // Scroll the toolbar up and let it pinned on top.
        scrollUpToolbarUntilPinnedAtTop(activity);
        waitForStatusBarColor(activity, expectedColor);
    }

    /** Test that the status indicator color is included in the color calculation correctly. */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.PHONE}) // Status bar is always black on tablets
    public void testColorWithStatusIndicator() {
        final ChromeActivity activity = sActivityTestRule.getActivity();
        final StatusBarColorController statusBarColorController =
                sActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getStatusBarColorController();
        final Supplier<Integer> statusBarColor = () -> activity.getWindow().getStatusBarColor();
        final int initialColor = statusBarColor.get();

        // Initially, StatusBarColorController#getStatusBarColorWithoutStatusIndicator should return
        // the same color as the current status bar color.
        Assert.assertEquals(
                "Wrong initial value returned by #getStatusBarColorWithoutStatusIndicator().",
                initialColor,
                statusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Set a status indicator color.
        ThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.onStatusIndicatorColorChanged(Color.BLUE));

        Assert.assertEquals("Wrong status bar color.", Color.BLUE, statusBarColor.get().intValue());

        // StatusBarColorController#getStatusBarColorWithoutStatusIndicator should still return the
        // initial color.
        Assert.assertEquals(
                "Wrong value returned by #getStatusBarColorWithoutStatusIndicator().",
                initialColor,
                statusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Set scrim.
        ThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.setStatusBarScrimFraction(.5f));

        // The resulting color should be a scrimmed version of the status bar color.
        Assert.assertEquals(
                "Wrong status bar color w/ scrim.",
                getScrimmedColor(Color.BLUE, .5f),
                statusBarColor.get().intValue());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Remove scrim.
                    statusBarColorController.setStatusBarScrimFraction(.0f);
                    // Set the status indicator color to the default, i.e. transparent.
                    statusBarColorController.onStatusIndicatorColorChanged(Color.TRANSPARENT);
                });

        // Now, the status bar color should be back to the initial color.
        Assert.assertEquals(
                "Wrong status bar color after the status indicator color is set to default.",
                initialColor,
                statusBarColor.get().intValue());
    }

    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @DisabledTest(message = "b/352622267")
    @Restriction({DeviceFormFactor.PHONE}) // Status bar is always black on tablets
    public void testFocusAndScrollColors() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final StatusBarColorController statusBarColorController =
                sActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getStatusBarColorController();
        loadUrlInNewTabAndWaitForShowing("about:blank", /* incognito= */ false);

        mOmniboxUtils.requestFocus();
        final @ColorInt int focusedColor =
                ChromeColors.getSurfaceColor(
                        activity, R.dimen.omnibox_suggestion_dropdown_bg_elevation);

        statusBarColorController.onSuggestionDropdownScroll();
        final @ColorInt int scrolledColor =
                ChromeColors.getSurfaceColor(activity, R.dimen.toolbar_text_box_elevation);
        waitForStatusBarColor(activity, scrolledColor);

        statusBarColorController.onSuggestionDropdownOverscrolledToTop();
        waitForStatusBarColor(activity, focusedColor);

        TabModelSelector tabModelSelector = activity.getTabModelSelectorSupplier().get();
        ThreadUtils.runOnUiThread(() -> tabModelSelector.selectModel(/* incognito= */ true));
        loadUrlInNewTabAndWaitForShowing("about:blank", /* incognito= */ true);

        mOmniboxUtils.requestFocus();
        final @ColorInt int focusedIncognitoColor =
                ContextCompat.getColor(activity, R.color.omnibox_dropdown_bg_incognito);
        waitForStatusBarColor(activity, focusedIncognitoColor);

        statusBarColorController.onSuggestionDropdownScroll();
        final @ColorInt int scrolledIncognitoColor =
                ContextCompat.getColor(activity, R.color.default_bg_color_dark_elev_2_baseline);
        waitForStatusBarColor(activity, scrolledIncognitoColor);

        statusBarColorController.onSuggestionDropdownOverscrolledToTop();
        waitForStatusBarColor(activity, focusedIncognitoColor);
    }

    /** Test that the theme color is cleared when the Omnibox gains focus. */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.PHONE}) // Status bar is always black on tablets
    public void testBrandColorIgnoredWhenOmniboxIsFocused() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final @ColorInt int expectedFocusedColor =
                ChromeColors.getSurfaceColor(
                        activity, R.dimen.omnibox_suggestion_dropdown_bg_elevation);

        String pageWithBrandColorUrl =
                sActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/theme_color_test.html");
        loadUrlAndWaitForShowing(pageWithBrandColorUrl);
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);
        waitForStatusBarColor(activity, Color.RED);

        mOmniboxUtils.requestFocus();
        waitForStatusBarColor(activity, expectedFocusedColor);
        mOmniboxUtils.clearFocus();
        waitForStatusBarColor(activity, Color.RED);
    }

    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.PHONE}) // Status bar is always black on tablets
    public void testBrandColorIgnoredWhenOmniboxIsFocused_FeatureMatchToolbarColorEnabled()
            throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int expectedFocusedColor =
                ChromeColors.getSurfaceColor(
                        activity, R.dimen.omnibox_suggestion_dropdown_bg_elevation);

        String pageWithBrandColorUrl =
                sActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/theme_color_test.html");
        sActivityTestRule.loadUrl(pageWithBrandColorUrl);
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);
        mOmniboxUtils.waitAnimationsComplete();
        waitForStatusBarColor(activity, Color.RED);
        waitForStatusBarColorToMatchToolbarColor(activity);

        mOmniboxUtils.requestFocus();
        mOmniboxUtils.waitAnimationsComplete();
        waitForStatusBarColor(activity, expectedFocusedColor);
        waitForStatusBarColorToMatchToolbarColor(activity);
        mOmniboxUtils.clearFocus();
        mOmniboxUtils.waitAnimationsComplete();
        waitForStatusBarColor(activity, Color.RED);
        waitForStatusBarColorToMatchToolbarColor(activity);
    }

    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.PHONE}) // Status bar is always black on tablets
    public void testColorWithStatusIndicator_FeatureMatchToolbarColorEnabled() {
        final ChromeActivity activity = sActivityTestRule.getActivity();
        final StatusBarColorController statusBarColorController =
                sActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getStatusBarColorController();
        final Supplier<Integer> statusBarColor = () -> activity.getWindow().getStatusBarColor();
        final int initialColor = statusBarColor.get();

        // Initially, StatusBarColorController#getStatusBarColorWithoutStatusIndicator should return
        // the same color as the current status bar color.
        Assert.assertEquals(
                "Wrong initial value returned by #getStatusBarColorWithoutStatusIndicator().",
                initialColor,
                statusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Set a status indicator color.
        ThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.onStatusIndicatorColorChanged(Color.BLUE));

        Assert.assertEquals("Wrong status bar color.", Color.BLUE, statusBarColor.get().intValue());

        // StatusBarColorController#getStatusBarColorWithoutStatusIndicator should still return the
        // initial color.
        Assert.assertEquals(
                "Wrong value returned by #getStatusBarColorWithoutStatusIndicator().",
                initialColor,
                statusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Set scrim.
        ThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.setStatusBarScrimFraction(.5f));

        Assert.assertEquals(
                "Wrong status bar color w/ scrim",
                getScrimmedColor(Color.BLUE, .5f),
                statusBarColor.get().intValue());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Remove scrim.
                    statusBarColorController.setStatusBarScrimFraction(.0f);
                    // Set the status indicator color to the default, i.e. transparent.
                    statusBarColorController.onStatusIndicatorColorChanged(Color.TRANSPARENT);
                });

        // Now, the status bar color should be back to the initial color.
        Assert.assertEquals(
                "Wrong status bar color after the status indicator color is set to default.",
                initialColor,
                statusBarColor.get().intValue());
    }

    /** Test status bar color for Tab Strip Redesign Folio. */
    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.TABLET, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testStatusBarColorForTabStripRedesignFolioTablet() {
        final ChromeActivity activity = sActivityTestRule.getActivity();
        final StatusBarColorController statusBarColorController =
                sActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getStatusBarColorController();

        ThreadUtils.runOnUiThreadBlocking(() -> statusBarColorController.updateStatusBarColor());
        assertEquals(
                "Wrong value returned for Tab Strip Redesign Folio.",
                TabUiThemeUtil.getTabStripBackgroundColor(activity, false),
                activity.getWindow().getStatusBarColor());
    }

    @Test
    @LargeTest
    @Feature({"StatusBar"})
    @Restriction({DeviceFormFactor.TABLET, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testStatusBarColorOnTabletDuringTabStripTransition() {
        final ChromeActivity activity = sActivityTestRule.getActivity();
        final StatusBarColorController statusBarColorController =
                sActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getStatusBarColorController();
        statusBarColorController.setAllowToolbarColorOnTablets(true);

        var toolbarColor = sActivityTestRule.getActivity().getToolbarManager().getPrimaryColor();

        // We will invoke #onToolbarColorChanged() on a tablet that in turn invokes
        // #updateStatusBarColor() to assert that it sets |mToolbarColor| as expected. The status
        // bar should use |mToolbarColor| when the tab strip is hidden, and the tab strip background
        // color otherwise.

        // Assume that the tab strip is initially hidden.
        statusBarColorController.setTabStripHiddenOnTablet(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.onToolbarColorChanged(toolbarColor));
        assertEquals(
                "Status bar color on tablet should match the toolbar background when the tab strip"
                        + " is hidden.",
                toolbarColor,
                activity.getWindow().getStatusBarColor());

        // Simulate an in-progress hide->show transition, where a scrim will be added on the status
        // bar.
        // TabStripHeightObserver#onHeightChanged() is expected to update the final strip visibility
        // state in StatusBarColorController for this transition once the control container margins
        // are updated and before the transition runs to completion.
        statusBarColorController.setTabStripHiddenOnTablet(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> statusBarColorController.setTabStripColorOverlay(toolbarColor, 0.5f));
        assertEquals(
                "Status bar color on tablet should use the tab strip transition scrim overlay"
                        + " during a strip transition.",
                ColorUtils.getColorWithOverlay(
                        TabUiThemeUtil.getTabStripBackgroundColor(activity, false),
                        toolbarColor,
                        0.5f),
                activity.getWindow().getStatusBarColor());

        // Simulate transition completion by resetting the transition overlay state in
        // StatusBarColorController.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        statusBarColorController.setTabStripColorOverlay(
                                ScrimProperties.INVALID_COLOR, 0f));
        assertEquals(
                "Status bar color on tablet should match the default tab strip background when the"
                        + " tab strip is visible.",
                TabUiThemeUtil.getTabStripBackgroundColor(activity, false),
                activity.getWindow().getStatusBarColor());
    }

    /** Test status bar is always black in Automotive devices. */
    @Test
    @SmallTest
    @Feature({"StatusBar, Automotive Toolbar"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    public void testStatusBarBlackInAutomotive() {
        final ChromeActivity activity = sActivityTestRule.getActivity();
        assertEquals(
                "Status bar should always be black in automotive devices.",
                Color.BLACK,
                activity.getWindow().getStatusBarColor());
    }

    private int getScrimmedColor(@ColorInt int color, float fraction) {
        return ColorUtils.overlayColor(color, mScrimColor, fraction);
    }

    private void waitForStatusBarColor(Activity activity, int expectedColor)
            throws ExecutionException, TimeoutException {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getWindow().getStatusBarColor(), Matchers.is(expectedColor));
                },
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void waitForStatusBarColorToMatchToolbarColor(Activity activity)
            throws ExecutionException, TimeoutException {
        ToolbarLayout toolbar = activity.findViewById(R.id.toolbar);
        Assert.assertTrue(
                "ToolbarLayout should be of type ToolbarPhone to get and check toolbar background.",
                toolbar instanceof ToolbarPhone);

        final int toolbarColor = ((ToolbarPhone) toolbar).getBackgroundDrawable().getColor();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getWindow().getStatusBarColor(), Matchers.is(toolbarColor));
                },
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void scrollUpToolbarUntilPinnedAtTop(Activity activity) {
        Resources resources = activity.getResources();
        // Drag the Feed header title to scroll the toolbar to the top.
        int toY =
                -resources.getDimensionPixelOffset(R.dimen.toolbar_height_no_shadow)
                        - activity.findViewById(R.id.logo_holder).getHeight();
        TestTouchUtils.dragCompleteView(
                InstrumentationRegistry.getInstrumentation(),
                activity.findViewById(R.id.header_title),
                0,
                0,
                0,
                toY,
                10);

        // Toolbar layout view should show.
        onViewWaiting(withId(R.id.toolbar));
    }

    private void loadUrlAndWaitForShowing(String url) {
        sActivityTestRule.loadUrl(url);
        waitForShowing();
    }

    private void loadUrlInNewTabAndWaitForShowing(String url, boolean incognito) {
        sActivityTestRule.loadUrlInNewTab(url, incognito);
        waitForShowing();
    }

    private void waitForShowing() {
        // At least for now, StaticLayout requests focus when it's doneShowing(). When exactly this
        // happens is quite racy and can cause flakes, as it removes focus from the omnibox. See
        // crbug.com/342539152. Unclear if we even want this, see crbug.com/40249125.
        LayoutManagerImpl lmi = sActivityTestRule.getActivity().getLayoutManagerSupplier().get();
        LayoutTestUtils.waitForLayout(lmi, LayoutType.BROWSING);
    }
}
