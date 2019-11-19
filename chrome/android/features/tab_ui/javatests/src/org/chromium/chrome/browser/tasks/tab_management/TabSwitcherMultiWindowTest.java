// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.moveActivityToFront;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForSecondChromeTabbedActivity;

import android.annotation.TargetApi;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.toolbar.IncognitoToggleTabLayout;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/** Tests for Multi-window related behavior in grid tab switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
                ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@MinAndroidSdkLevel(Build.VERSION_CODES.N)
public class TabSwitcherMultiWindowTest {
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        FeatureUtilities.setGridTabSwitcherEnabledForTesting(true);
        mActivityTestRule.startMainActivityFromLauncher();
        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        CriteriaHelper.pollUiThread(Criteria.equals(true,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()::isTabModelRestored));
    }

    @Test
    @MediumTest
    @TargetApi(Build.VERSION_CODES.N)
    @DisableIf.Build(message = "crbug.com/1017141", sdk_is_less_than = Build.VERSION_CODES.P)
    public void testMoveTabsAcrossWindow_GTS_WithoutGroup() throws InterruptedException {
        final ChromeTabbedActivity cta1 = mActivityTestRule.getActivity();
        // Initially, we have 4 normal tabs and 3 incognito tabs in cta1.
        initializeTabModel(cta1, false, 4);
        initializeTabModel(cta1, true, 3);
        verifyTabModelTabCount(cta1, 4, 3);

        // Enter tab switcher in cta1 in incognito mode.
        enterTabSwitcher(cta1);
        assertTrue(cta1.getTabModelSelector().getCurrentModel().isIncognito());

        // Before move, there are 3 incognito tabs in cta1.
        RecyclerView recyclerView1 = cta1.findViewById(R.id.tab_list_view);
        CriteriaHelper.pollUiThread(Criteria.equals(3, recyclerView1::getChildCount));

        // Move 2 incognito tabs to cta2.
        enterFirstTabFromTabSwitcher(cta1);
        moveTabsToOtherWindow(cta1, 2);

        // After move, there are 1 incognito tab in cta1 and 2 incognito tabs in cta2.
        final ChromeTabbedActivity cta2 = waitForSecondChromeTabbedActivity();
        verifyTabModelTabCount(cta1, 4, 1);
        verifyTabModelTabCount(cta2, 0, 2);
        enterTabSwitcher(cta1);
        CriteriaHelper.pollUiThread(Criteria.equals(1, recyclerView1::getChildCount));

        // Enter tab switcher in cta2.
        moveActivityToFront(cta2);
        enterTabSwitcher(cta2);

        // There should be two incognito tabs in tab switcher in cta2.
        RecyclerView recyclerView2 = cta2.findViewById(R.id.tab_list_view);
        CriteriaHelper.pollUiThread(Criteria.equals(2, recyclerView2::getChildCount));

        // Move 1 incognito tab back to cta1.
        enterFirstTabFromTabSwitcher(cta2);
        moveTabsToOtherWindow(cta2, 1);

        // After move, there are 2 incognito tabs in cta1 and 1 incognito tab in cta2.
        verifyTabModelTabCount(cta1, 4, 2);
        verifyTabModelTabCount(cta2, 0, 1);
        enterTabSwitcher(cta2);
        CriteriaHelper.pollUiThread(Criteria.equals(1, recyclerView2::getChildCount));

        // Enter tab switcher in cta1.
        moveActivityToFront(cta1);

        // There should be two incognito tabs in tab switcher in cta1.
        CriteriaHelper.pollUiThread(Criteria.equals(2, recyclerView1::getChildCount));

        // Switch to normal tab list in cta1.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IncognitoToggleTabLayout toggleTabLayout =
                    cta1.findViewById(R.id.incognito_toggle_tabs);
            ViewGroup toggleButtons = (ViewGroup) toggleTabLayout.getChildAt(0);
            toggleButtons.getChildAt(0).performClick();
        });
        assertEquals(false, cta1.getTabModelSelector().getCurrentModel().isIncognito());

        // Move 3 normal tabs to cta2.
        enterFirstTabFromTabSwitcher(cta1);
        moveTabsToOtherWindow(cta1, 3);

        // After move, there are 1 normal tab in cta1 and 3 normal tabs in cta2.
        verifyTabModelTabCount(cta1, 1, 2);
        verifyTabModelTabCount(cta2, 3, 1);
        enterTabSwitcher(cta1);
        CriteriaHelper.pollUiThread(Criteria.equals(1, recyclerView1::getChildCount));

        // Enter tab switcher in cta2.
        moveActivityToFront(cta2);
        enterTabSwitcher(cta2);

        // There should be 3 normal tabs in tab switcher in cta2.
        CriteriaHelper.pollUiThread(Criteria.equals(3, recyclerView2::getChildCount));

        // Move 2 normal tabs back to cta1.
        enterFirstTabFromTabSwitcher(cta2);
        moveTabsToOtherWindow(cta2, 2);

        // After move, there are 3 normal tabs in cta1 and 1 normal tab in cta2.
        verifyTabModelTabCount(cta1, 3, 2);
        verifyTabModelTabCount(cta2, 1, 1);
        enterTabSwitcher(cta2);
        CriteriaHelper.pollUiThread(Criteria.equals(1, recyclerView2::getChildCount));
    }

    private void initializeTabModel(ChromeTabbedActivity cta, boolean isIncognito, int tabsCount) {
        for (int i = 0; i < (isIncognito ? tabsCount : tabsCount - 1); i++) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), cta, isIncognito, true);
        }
    }

    private void moveTabsToOtherWindow(ChromeTabbedActivity cta, int number) {
        for (int i = 0; i < number; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), cta,
                    org.chromium.chrome.R.id.move_to_other_window_menu_id);
        }
    }

    private void enterTabSwitcher(ChromeTabbedActivity cta) {
        assertFalse(cta.getLayoutManager().overviewVisible());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.findViewById(R.id.tab_switcher_button).performClick(); });
        CriteriaHelper.pollUiThread(
                Criteria.equals(true, () -> cta.getLayoutManager().overviewVisible()));
    }

    private void enterFirstTabFromTabSwitcher(ChromeTabbedActivity cta) {
        onView(withId(R.id.tab_list_view))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollInstrumentationThread(() -> !cta.getLayoutManager().overviewVisible());
    }

    private void verifyTabModelTabCount(
            ChromeTabbedActivity cta, int normalTabs, int incognitoTabs) {
        CriteriaHelper.pollUiThread(Criteria.equals(
                normalTabs, () -> cta.getTabModelSelector().getModel(false).getCount()));
        CriteriaHelper.pollUiThread(Criteria.equals(
                incognitoTabs, () -> cta.getTabModelSelector().getModel(true).getCount()));
    }
}
