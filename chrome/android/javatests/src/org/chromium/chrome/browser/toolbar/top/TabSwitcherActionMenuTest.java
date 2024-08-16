// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;

/** Instrumentation tests for tab switcher long-press menu popup */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabSwitcherActionMenuTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        onViewWaiting(allOf(withId(R.id.tab_switcher_button), isDisplayed()));
    }

    @Test
    @SmallTest
    public void testCloseTab() {
        int tabCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        onView(withId(R.id.tab_switcher_button)).perform(longClick());

        // withId does not work, cause android:id is not set on the view
        onView(withText(R.string.close_tab)).check(matches(isDisplayed()));
        onView(withText(R.string.close_tab)).perform(click());
        Assert.assertEquals(
                tabCount - 1, mActivityTestRule.getActivity().getCurrentTabModel().getCount());
    }

    @Test
    @SmallTest
    public void testOpenNewTab() {
        int tabCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        onView(withId(R.id.tab_switcher_button)).perform(longClick());

        onView(withText(R.string.menu_new_tab)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_new_tab)).perform(click());

        Assert.assertEquals(
                tabCount + 1, mActivityTestRule.getActivity().getCurrentTabModel().getCount());
    }

    @Test
    @SmallTest
    public void testOpenNewIncognitoTab() {
        onView(withId(R.id.tab_switcher_button)).perform(longClick());

        onView(withText(R.string.menu_new_incognito_tab)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_new_incognito_tab)).perform(click());

        // only one incognito tab opened
        Assert.assertEquals(1, mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        assertTrue(mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /** Regression test for crbug.com/1448791 */
    @Test
    @SmallTest
    public void testClosingAllRegularTabs_DoNotFinishActivity() {
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);

        int tabsToClose =
                mActivityTestRule
                        .getActivity()
                        .getTabModelSelector()
                        .getModel(/* incognito= */ false)
                        .getCount();
        while (tabsToClose-- > 0) {
            onView(withId(R.id.tab_switcher_button)).perform(longClick());
            onView(withText(R.string.close_tab)).check(matches(isDisplayed()));
            onView(withText(R.string.close_tab)).perform(click());
        }

        // Incognito tabs should still remain.
        assertTrue(
                mActivityTestRule
                                .getActivity()
                                .getTabModelSelector()
                                .getModel(/* incognito= */ true)
                                .getCount()
                        > 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    @SmallTest
    public void testCloseAllIncognitoTabs() {
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        onView(withId(R.id.tab_switcher_button)).perform(longClick());
        onView(withText(R.string.menu_close_all_incognito_tabs)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_close_all_incognito_tabs)).perform(click());
        // Confirm on dialog.
        onViewWaiting(withId(R.id.positive_button), true).perform(click());

        // Incognito tabs closed.
        assertTrue(
                mActivityTestRule
                                .getActivity()
                                .getTabModelSelector()
                                .getModel(/* incognito= */ true)
                                .getCount()
                        == 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    @SmallTest
    public void testSwitchToIncognito() {
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);
        onView(withId(R.id.tab_switcher_button)).perform(longClick());
        onView(withText(R.string.menu_switch_to_incognito)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_switch_to_incognito)).perform(click());

        // Incognito model selected.
        assertTrue(
                mActivityTestRule
                        .getActivity()
                        .getTabModelSelector()
                        .isIncognitoBrandedModelSelected());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    @SmallTest
    public void testSwitchOutOfIncognito() {
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        // Start state - Incognito model selected.
        assertTrue(
                mActivityTestRule
                        .getActivity()
                        .getTabModelSelector()
                        .isIncognitoBrandedModelSelected());

        onView(withId(R.id.tab_switcher_button)).perform(longClick());
        onView(withText(R.string.menu_switch_out_of_incognito)).check(matches(isDisplayed()));
        onView(withText(R.string.menu_switch_out_of_incognito)).perform(click());

        // End state - Standard model selected.
        assertFalse(
                mActivityTestRule
                        .getActivity()
                        .getTabModelSelector()
                        .isIncognitoBrandedModelSelected());
    }
}
