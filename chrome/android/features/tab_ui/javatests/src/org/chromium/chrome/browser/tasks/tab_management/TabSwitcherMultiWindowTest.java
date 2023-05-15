// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.moveActivityToFront;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForSecondChromeTabbedActivity;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllIncognitoTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.switchTabModel;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabModelTabCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabStripFaviconCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.test.util.UiRestriction;

/** Tests for Multi-window related behavior in grid tab switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING})
@Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
public class TabSwitcherMultiWindowTest {
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TabUiTestHelper.verifyTabSwitcherLayoutType(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/1382930")
    public void testMoveTabsAcrossWindow_GTS_WithoutGroup() {
        final ChromeTabbedActivity cta1 = mActivityTestRule.getActivity();
        // Initially, we have 4 normal tabs (including the one created at activity start) and 3
        // incognito tabs in cta1.
        TabUiTestHelper.addBlankTabs(cta1, false, 3);
        TabUiTestHelper.addBlankTabs(cta1, true, 3);
        verifyTabModelTabCount(cta1, 4, 3);

        // Enter tab switcher in cta1 in incognito mode.
        enterTabSwitcher(cta1);
        assertTrue(cta1.getTabModelSelector().getCurrentModel().isIncognito());

        // Before move, there are 3 incognito tabs in cta1.
        verifyTabSwitcherCardCount(cta1, 3);

        // Move 2 incognito tabs to cta2.
        clickFirstCardFromTabSwitcher(cta1);
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), cta1,
                R.id.move_to_other_window_menu_id);
        final ChromeTabbedActivity cta2 = waitForSecondChromeTabbedActivity();
        moveActivityToFront(cta1);
        moveTabsToOtherWindow(cta1, 1);

        // After move, there are 1 incognito tab in cta1 and 2 incognito tabs in cta2.
        enterTabSwitcher(cta1);
        verifyTabSwitcherCardCount(cta1, 1);
        clickFirstCardFromTabSwitcher(cta1);
        moveActivityToFront(cta2);
        enterTabSwitcher(cta2);
        verifyTabSwitcherCardCount(cta2, 2);
        verifyTabModelTabCount(cta1, 4, 1);
        verifyTabModelTabCount(cta2, 0, 2);

        // Move 1 incognito tab back to cta1.
        clickFirstCardFromTabSwitcher(cta2);
        moveTabsToOtherWindow(cta2, 1);

        // After move, there are 2 incognito tabs in cta1 and 1 incognito tab in cta2.
        enterTabSwitcher(cta2);
        verifyTabSwitcherCardCount(cta2, 1);
        clickFirstCardFromTabSwitcher(cta2);
        moveActivityToFront(cta1);
        enterTabSwitcher(cta1);
        verifyTabSwitcherCardCount(cta1, 2);
        verifyTabModelTabCount(cta1, 4, 2);
        verifyTabModelTabCount(cta2, 0, 1);

        // Switch to normal tab list in cta1.
        switchTabModel(cta1, false);
        assertFalse(cta1.getTabModelSelector().getCurrentModel().isIncognito());

        // Move 3 normal tabs to cta2.
        clickFirstCardFromTabSwitcher(cta1);
        moveTabsToOtherWindow(cta1, 3);

        // After move, there are 1 normal tab in cta1 and 3 normal tabs in cta2.
        enterTabSwitcher(cta1);
        verifyTabSwitcherCardCount(cta1, 1);
        clickFirstCardFromTabSwitcher(cta1);
        moveActivityToFront(cta2);
        enterTabSwitcher(cta2);
        verifyTabSwitcherCardCount(cta2, 3);
        verifyTabModelTabCount(cta1, 1, 2);
        verifyTabModelTabCount(cta2, 3, 1);

        // Move 2 normal tabs back to cta1.
        clickFirstCardFromTabSwitcher(cta2);
        moveTabsToOtherWindow(cta2, 2);

        // After move, there are 3 normal tabs in cta1 and 1 normal tab in cta2.
        enterTabSwitcher(cta2);
        verifyTabSwitcherCardCount(cta2, 1);
        clickFirstCardFromTabSwitcher(cta2);
        moveActivityToFront(cta1);
        enterTabSwitcher(cta1);
        verifyTabSwitcherCardCount(cta1, 3);
        verifyTabModelTabCount(cta1, 3, 2);
        verifyTabModelTabCount(cta2, 1, 1);
    }

    @Test
    @LargeTest
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @DisabledTest(message = "https://crbug.com/1413875")
    public void testMoveTabsAcrossWindow_GTS_WithGroup() {
        // Initially, we have 5 normal tabs (including the one created at activity start) and 5
        // incognito tabs in cta1.
        final ChromeTabbedActivity cta1 = mActivityTestRule.getActivity();
        TabUiTestHelper.addBlankTabs(cta1, false, 4);
        TabUiTestHelper.addBlankTabs(cta1, true, 5);
        verifyTabModelTabCount(cta1, 5, 5);

        // Enter tab switcher in cta1 in incognito mode.
        enterTabSwitcher(cta1);
        assertTrue(cta1.getTabModelSelector().getCurrentModel().isIncognito());

        // Merge all incognito tabs into one group.
        mergeAllIncognitoTabsToAGroup(cta1);
        verifyTabSwitcherCardCount(cta1, 1);

        // Enter group and verify there are 5 favicons in tab strip.
        clickFirstCardFromTabSwitcher(cta1);
        clickFirstTabInDialog(cta1);
        verifyTabStripFaviconCount(cta1, 5);

        // Move 3 incognito tabs in this group to cta2.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), cta1,
                R.id.move_to_other_window_menu_id);
        final ChromeTabbedActivity cta2 = waitForSecondChromeTabbedActivity();
        moveActivityToFront(cta1);
        moveTabsToOtherWindow(cta1, 2);

        // After move, there is a group of 2 incognito tabs in cta1 and a group of 3 incognito tabs
        // in cta2.
        verifyTabStripFaviconCount(cta1, 2);
        moveActivityToFront(cta2);
        verifyTabStripFaviconCount(cta2, 3);
        verifyTabModelTabCount(cta1, 5, 2);
        verifyTabModelTabCount(cta2, 0, 3);

        // Move one incognito tab in group back to cta1.
        moveTabsToOtherWindow(cta2, 1);

        // After move, there is group of 3 incognito tabs in cta1 and a group of 2 incognito tabs in
        // cta2.
        verifyTabStripFaviconCount(cta2, 2);
        moveActivityToFront(cta1);
        verifyTabStripFaviconCount(cta1, 3);
        verifyTabModelTabCount(cta1, 5, 3);
        verifyTabModelTabCount(cta2, 0, 2);

        // Switch to normal tab model in cta1 and create a tab group with 5 normal tabs.
        enterTabSwitcher(cta1);
        switchTabModel(cta1, false);
        mergeAllNormalTabsToAGroup(cta1);
        verifyTabSwitcherCardCount(cta1, 1);

        // Enter group and verify there are 5 favicons in tab strip.
        clickFirstCardFromTabSwitcher(cta1);
        clickFirstTabInDialog(cta1);
        verifyTabStripFaviconCount(cta1, 5);

        // Move 3 normal tabs in this group to cta2.
        moveTabsToOtherWindow(cta1, 3);

        // After move, there is a group of 2 normal tabs in cta1 and a group of 3 normal tabs in
        // cta2.
        verifyTabStripFaviconCount(cta1, 2);
        moveActivityToFront(cta2);
        verifyTabStripFaviconCount(cta2, 3);
        verifyTabModelTabCount(cta1, 2, 3);
        verifyTabModelTabCount(cta2, 3, 2);

        // Move one normal tab in group back to cta1.
        moveTabsToOtherWindow(cta2, 1);

        // After move, there is a group of 3 normal tabs in cta1 and a group of 2 normal tabs in
        // cta2.
        verifyTabStripFaviconCount(cta2, 2);
        moveActivityToFront(cta1);
        verifyTabStripFaviconCount(cta1, 3);
        verifyTabModelTabCount(cta1, 3, 3);
        verifyTabModelTabCount(cta2, 2, 2);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1363248")
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testMoveLastIncognitoTab() {
        // clang-format on
        // Initially, we have 1 normal tab (created in #setup()) and 1 incognito tab in cta1.
        final ChromeTabbedActivity cta1 = mActivityTestRule.getActivity();
        TabUiTestHelper.addBlankTabs(cta1, true, 1);
        verifyTabModelTabCount(cta1, 1, 1);

        // Move the incognito tab to cta2.
        assertTrue(cta1.getTabModelSelector().getCurrentModel().isIncognito());
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), cta1,
                R.id.move_to_other_window_menu_id);
        final ChromeTabbedActivity cta2 = waitForSecondChromeTabbedActivity();

        assertThat(cta1.getTabModelSelector()
                           .getTabModelFilterProvider()
                           .getTabModelFilter(true)
                           .getCount(),
                is(0));
        assertThat(cta2.getTabModelSelector()
                           .getTabModelFilterProvider()
                           .getTabModelFilter(true)
                           .getCount(),
                is(1));
    }

    private void moveTabsToOtherWindow(ChromeTabbedActivity cta, int number) {
        for (int i = 0; i < number; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), cta,
                    R.id.move_to_other_window_menu_id);
            moveActivityToFront(cta);
        }
    }
}
