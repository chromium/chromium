// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TAB_SWITCHER_TOOLBAR;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_MENU;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_NEW_TAB;
import static org.chromium.chrome.test.util.ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION;
import static org.chromium.chrome.test.util.ToolbarTestUtils.checkToolbarButtonVisibility;

import android.content.res.Configuration;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.test.util.UiRestriction;

/** End-to-end tests for adaptive toolbar. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.DisableFeatures({
    ChromeFeatureList.TAB_GROUPS_ANDROID, ChromeFeatureList.START_SURFACE_ANDROID})
public class AdaptiveToolbarTest {
    // Params to turn off new tab variation in GTS.
    private static final String NO_NEW_TAB_VARIATION_PARAMS = "force-fieldtrial-params=" +
            "Study.Group:tab_grid_layout_android_new_tab/false";
    // Params to turn on new tab variation in GTS.
    private static final String NEW_TAB_VARIATION_PARAMS = "force-fieldtrial-params=" +
            "Study.Group:tab_grid_layout_android_new_tab/NewTabVariation";
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @After
    public void tearDown() {
        ChromeFeatureList.sTabGridLayoutAndroid.setForTesting(null);
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
    }

    private void setupFlagsAndLaunchActivity(boolean isGridTabSwitcherEnabled) {
        ChromeFeatureList.sTabGridLayoutAndroid.setForTesting(isGridTabSwitcherEnabled);
        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID +
            "<Study", "force-fieldtrials=Study/Group", NO_NEW_TAB_VARIATION_PARAMS})
    public void testTopToolbar_WithGTS() throws InterruptedException {
        // clang-format on
        setupFlagsAndLaunchActivity(true);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabUiTestHelper.verifyTabSwitcherLayoutType(cta);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);

        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID +
            "<Study", "force-fieldtrials=Study/Group", NEW_TAB_VARIATION_PARAMS})
    public void testTopToolbar_NewTabVariation() {
        // clang-format on
        setupFlagsAndLaunchActivity(true);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabUiTestHelper.verifyTabSwitcherLayoutType(mActivityTestRule.getActivity());
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, false);

        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION, true);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, false);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID +
            "<Study", "force-fieldtrials=Study/Group", NEW_TAB_VARIATION_PARAMS})
    public void testTopToolbar_NewTabVariation_IncognitoDisabled() {
        // clang-format on
        IncognitoUtils.setEnabledForTesting(false);
        setupFlagsAndLaunchActivity(true);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabUiTestHelper.verifyTabSwitcherLayoutType(cta);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION, false);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);

        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_MENU, true);
        checkToolbarButtonVisibility(
                TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION, false);
        checkToolbarButtonVisibility(TAB_SWITCHER_TOOLBAR, TAB_SWITCHER_TOOLBAR_NEW_TAB, true);
    }
}
