// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * An end-to-end test of the close all tabs dialog.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.CLOSE_ALL_TABS_MODAL_DIALOG})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CloseAllTabsDialogTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL("about:blank");
    }

    /**
     * Tests that close all tabs works after modal dialog.
     */
    @Test
    @SmallTest
    public void testCloseAllTabs() {
        TabModelSelector selector =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();
        assertEquals(1, selector.getTotalTabCount());

        navigateToCloseAllTabsDialog();
        onViewWaiting(allOf(withId(org.chromium.chrome.test.R.id.positive_button),
                              withText(is("Close all tabs"))))
                .check(matches(isDisplayed()))
                .perform(click());

        assertEquals(0, selector.getTotalTabCount());
    }

    /**
     * Tests that close all tabs stops if dismissing modal dialog.
     */
    @Test
    @SmallTest
    public void testCancelCloseAllTabs() {
        TabModelSelector selector =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();
        assertEquals(1, selector.getTotalTabCount());

        navigateToCloseAllTabsDialog();
        onViewWaiting(allOf(withId(org.chromium.chrome.test.R.id.negative_button),
                              withText(is("Cancel"))))
                .check(matches(isDisplayed()))
                .perform(click());

        assertEquals(1, selector.getTotalTabCount());
    }

    private void navigateToCloseAllTabsDialog() {
        onViewWaiting(withId(org.chromium.chrome.test.R.id.tab_switcher_button))
                .check(matches(isDisplayed()))
                .perform(click());
        onViewWaiting(withId(org.chromium.chrome.test.R.id.tab_switcher_toolbar))
                .check(matches(isDisplayed()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });
        onViewWaiting(withId(org.chromium.chrome.test.R.id.close_all_tabs_menu_id))
                .check(matches(isDisplayed()))
                .perform(click());
    }
}
