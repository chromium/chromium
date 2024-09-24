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
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.leaveTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.switchTabModel;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabModelTabCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.os.Build;
import android.os.Build.VERSION_CODES;

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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for Multi-window related behavior in grid tab switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING
})
@Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@DisableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
@DisableIf.Build(sdk_is_greater_than = VERSION_CODES.S_V2) // https://crbug.com/1297370
// TODO(crbug.com/344669867): Failing when batched, batch this again.
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

        // Reset back nav so clean up is able to exit the tab switcher.
        clickFirstCardFromTabSwitcher(mCta1);
        enterTabSwitcher(mCta1);
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
