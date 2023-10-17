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
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.leaveTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllIncognitoTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.switchTabModel;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabModelTabCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabStripFaviconCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.os.Build;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.ui.test.util.UiRestriction;

/** Tests for Multi-window related behavior in grid tab switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING
})
@Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@Batch(Batch.PER_CLASS)
public class TabSwitcherMultiWindowTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private ChromeTabbedActivity mCta1;
    private ChromeTabbedActivity mCta2;

    @Before
    public void setUp() {
        TabUiTestHelper.verifyTabSwitcherLayoutType(sActivityTestRule.getActivity());
        mCta1 = sActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(mCta1.getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() throws Exception {
        // On Android N this throws an exception because of legacy multi window.
        if (mCta2 != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ApplicationTestUtils.finishActivity(mCta2);
        }
        if (mCta1 != null) {
            moveActivityToFront(mCta1);
            if (mCta1.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                leaveTabSwitcher(mCta1);
            }
        }
    }

    @Test
    @LargeTest
    public void testMoveTabsAcrossWindow_GTS_WithoutGroup() {
        // Initially, we have 4 normal tabs (including the one created at activity start) and 3
        // incognito tabs in mCta1.
        TabUiTestHelper.addBlankTabs(mCta1, false, 3);
        TabUiTestHelper.addBlankTabs(mCta1, true, 3);
        verifyTabModelTabCount(mCta1, 4, 3);

        // Enter tab switcher in mCta1 in incognito mode.
        enterTabSwitcher(mCta1);
        assertTrue(mCta1.getTabModelSelector().getCurrentModel().isIncognito());

        // Before move, there are 3 incognito tabs in mCta1.
        verifyTabSwitcherCardCount(mCta1, 3);

        // Move 2 incognito tabs to mCta2.
        clickFirstCardFromTabSwitcher(mCta1);
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mCta1,
                R.id.move_to_other_window_menu_id);
        mCta2 = waitForSecondChromeTabbedActivity();
        CriteriaHelper.pollUiThread(mCta2.getTabModelSelector()::isTabStateInitialized);
        moveActivityToFront(mCta1);
        moveTabsToOtherWindow(mCta1, 1);

        // After move, there are 1 incognito tab in mCta1 and 2 incognito tabs in mCta2.
        enterTabSwitcher(mCta1);
        verifyTabSwitcherCardCount(mCta1, 1);
        clickFirstCardFromTabSwitcher(mCta1);
        moveActivityToFront(mCta2);
        enterTabSwitcher(mCta2);
        verifyTabSwitcherCardCount(mCta2, 2);
        verifyTabModelTabCount(mCta1, 4, 1);
        verifyTabModelTabCount(mCta2, 0, 2);

        // Move 1 incognito tab back to mCta1.
        clickFirstCardFromTabSwitcher(mCta2);
        moveTabsToOtherWindow(mCta2, 1);

        // After move, there are 2 incognito tabs in mCta1 and 1 incognito tab in mCta2.
        enterTabSwitcher(mCta2);
        verifyTabSwitcherCardCount(mCta2, 1);
        clickFirstCardFromTabSwitcher(mCta2);
        moveActivityToFront(mCta1);
        enterTabSwitcher(mCta1);
        verifyTabSwitcherCardCount(mCta1, 2);
        verifyTabModelTabCount(mCta1, 4, 2);
        verifyTabModelTabCount(mCta2, 0, 1);

        // Switch to normal tab list in mCta1.
        switchTabModel(mCta1, false);
        assertFalse(mCta1.getTabModelSelector().getCurrentModel().isIncognito());

        // Move 3 normal tabs to mCta2.
        clickFirstCardFromTabSwitcher(mCta1);
        moveTabsToOtherWindow(mCta1, 3);

        // After move, there are 1 normal tab in mCta1 and 3 normal tabs in mCta2.
        enterTabSwitcher(mCta1);
        verifyTabSwitcherCardCount(mCta1, 1);
        clickFirstCardFromTabSwitcher(mCta1);
        moveActivityToFront(mCta2);
        enterTabSwitcher(mCta2);
        verifyTabSwitcherCardCount(mCta2, 3);
        verifyTabModelTabCount(mCta1, 1, 2);
        verifyTabModelTabCount(mCta2, 3, 1);

        // Move 2 normal tabs back to mCta1.
        clickFirstCardFromTabSwitcher(mCta2);
        moveTabsToOtherWindow(mCta2, 2);

        // After move, there are 3 normal tabs in mCta1 and 1 normal tab in mCta2.
        enterTabSwitcher(mCta2);
        verifyTabSwitcherCardCount(mCta2, 1);
        clickFirstCardFromTabSwitcher(mCta2);
        moveActivityToFront(mCta1);
        enterTabSwitcher(mCta1);
        verifyTabSwitcherCardCount(mCta1, 3);
        verifyTabModelTabCount(mCta1, 3, 2);
        verifyTabModelTabCount(mCta2, 1, 1);
    }

    @Test
    @LargeTest
    public void testMoveTabsAcrossWindow_GTS_WithGroup() {
        // Initially, we have 5 normal tabs (including the one created at activity start) and 5
        // incognito tabs in mCta1.
        TabUiTestHelper.addBlankTabs(mCta1, false, 4);
        TabUiTestHelper.addBlankTabs(mCta1, true, 5);
        verifyTabModelTabCount(mCta1, 5, 5);

        // Enter tab switcher in mCta1 in incognito mode.
        enterTabSwitcher(mCta1);
        assertTrue(mCta1.getTabModelSelector().getCurrentModel().isIncognito());

        // Merge all incognito tabs into one group.
        mergeAllIncognitoTabsToAGroup(mCta1);
        verifyTabSwitcherCardCount(mCta1, 1);

        // Enter group and verify there are 5 favicons in tab strip.
        clickFirstCardFromTabSwitcher(mCta1);
        clickFirstTabInDialog(mCta1);
        verifyTabStripFaviconCount(mCta1, 5);

        // Move 3 incognito tabs in this group to mCta2.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mCta1,
                R.id.move_to_other_window_menu_id);
        mCta2 = waitForSecondChromeTabbedActivity();
        CriteriaHelper.pollUiThread(mCta2.getTabModelSelector()::isTabStateInitialized);
        moveActivityToFront(mCta1);
        moveTabsToOtherWindow(mCta1, 2);

        // After move, there is a group of 2 incognito tabs in mCta1 and a group of 3 incognito tabs
        // in mCta2.
        verifyTabStripFaviconCount(mCta1, 2);
        moveActivityToFront(mCta2);
        verifyTabStripFaviconCount(mCta2, 3);
        verifyTabModelTabCount(mCta1, 5, 2);
        verifyTabModelTabCount(mCta2, 0, 3);

        // Move one incognito tab in group back to mCta1.
        moveTabsToOtherWindow(mCta2, 1);

        // After move, there is group of 3 incognito tabs in mCta1 and a group of 2 incognito tabs
        // in mCta2.
        verifyTabStripFaviconCount(mCta2, 2);
        moveActivityToFront(mCta1);
        verifyTabStripFaviconCount(mCta1, 3);
        verifyTabModelTabCount(mCta1, 5, 3);
        verifyTabModelTabCount(mCta2, 0, 2);

        // Switch to normal tab model in mCta1 and create a tab group with 5 normal tabs.
        enterTabSwitcher(mCta1);
        switchTabModel(mCta1, false);
        mergeAllNormalTabsToAGroup(mCta1);
        verifyTabSwitcherCardCount(mCta1, 1);

        // Enter group and verify there are 5 favicons in tab strip.
        clickFirstCardFromTabSwitcher(mCta1);
        clickFirstTabInDialog(mCta1);
        verifyTabStripFaviconCount(mCta1, 5);

        // Move 3 normal tabs in this group to mCta2.
        moveTabsToOtherWindow(mCta1, 3);

        // After move, there is a group of 2 normal tabs in mCta1 and a group of 3 normal tabs in
        // mCta2.
        verifyTabStripFaviconCount(mCta1, 2);
        moveActivityToFront(mCta2);
        verifyTabStripFaviconCount(mCta2, 3);
        verifyTabModelTabCount(mCta1, 2, 3);
        verifyTabModelTabCount(mCta2, 3, 2);

        // Move one normal tab in group back to mCta1.
        moveTabsToOtherWindow(mCta2, 1);

        // After move, there is a group of 3 normal tabs in mCta1 and a group of 2 normal tabs in
        // mCta2.
        verifyTabStripFaviconCount(mCta2, 2);
        moveActivityToFront(mCta1);
        verifyTabStripFaviconCount(mCta1, 3);
        verifyTabModelTabCount(mCta1, 3, 3);
        verifyTabModelTabCount(mCta2, 2, 2);
    }

    @Test
    @MediumTest
    public void testMoveLastIncognitoTab() {
        // Initially, we have 1 normal tab (created in #setup()) and 1 incognito tab in mCta1.
        TabUiTestHelper.addBlankTabs(mCta1, true, 1);
        verifyTabModelTabCount(mCta1, 1, 1);

        // Move the incognito tab to mCta2.
        assertTrue(mCta1.getTabModelSelector().getCurrentModel().isIncognito());
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mCta1,
                R.id.move_to_other_window_menu_id);
        mCta2 = waitForSecondChromeTabbedActivity();
        CriteriaHelper.pollUiThread(mCta2.getTabModelSelector()::isTabStateInitialized);

        assertThat(
                mCta1.getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getTabModelFilter(true)
                        .getCount(),
                is(0));
        assertThat(
                mCta2.getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getTabModelFilter(true)
                        .getCount(),
                is(1));
    }

    private void moveTabsToOtherWindow(ChromeTabbedActivity cta, int number) {
        for (int i = 0; i < number; i++) {
            MenuUtils.invokeCustomMenuActionSync(
                    InstrumentationRegistry.getInstrumentation(),
                    cta,
                    R.id.move_to_other_window_menu_id);
            moveActivityToFront(cta);
        }
    }
}
