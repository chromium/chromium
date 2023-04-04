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

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Instrumentation tests for tab switcher long-press menu popup
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
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
        Assert.assertTrue(
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
    }
}
