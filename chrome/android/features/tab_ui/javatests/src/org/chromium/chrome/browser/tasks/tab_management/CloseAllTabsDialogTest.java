// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Arrays;
import java.util.List;

/**
 * An end-to-end test of the close all tabs dialog.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CloseAllTabsDialogTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false).name("NonIncognito"),
                    new ParameterSet().value(true).name("Incognito"));

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private final boolean mIsIncognito;

    public CloseAllTabsDialogTest(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL("about:blank");
    }

    /**
     * Tests that close all tabs works after modal dialog.
     */
    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testCloseAllTabs() {
        TabModelSelector selector =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();

        navigateToCloseAllTabsDialog(selector);
        onViewWaiting(withId(org.chromium.chrome.test.R.id.positive_button)).perform(click());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { assertEquals(0, selector.getModel(mIsIncognito).getCount()); });
    }

    /**
     * Tests that close all tabs stops if dismissing modal dialog.
     */
    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testCancelCloseAllTabs() {
        TabModelSelector selector =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();

        navigateToCloseAllTabsDialog(selector);
        onViewWaiting(withId(org.chromium.chrome.test.R.id.negative_button)).perform(click());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { assertEquals(1, selector.getModel(mIsIncognito).getCount()); });
    }

    private void navigateToCloseAllTabsDialog(TabModelSelector selector) {
        // Create incognito tab if in incognito version.
        if (mIsIncognito) mActivityTestRule.newIncognitoTabFromMenu();

        assertEquals(1, selector.getModel(mIsIncognito).getCount());

        // Open the AppMenu in the Tab Switcher and ensure it shows.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        onViewWaiting(withId(org.chromium.chrome.test.R.id.tab_switcher_toolbar))
                .check(matches(isDisplayed()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });
        onViewWaiting(withId(org.chromium.chrome.test.R.id.app_menu_list))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        // Click close all tabs.
        if (mIsIncognito) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                AppMenuTestSupport.callOnItemClick(mActivityTestRule.getAppMenuCoordinator(),
                        org.chromium.chrome.test.R.id.close_all_incognito_tabs_menu_id);
            });
            return;
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.callOnItemClick(mActivityTestRule.getAppMenuCoordinator(),
                    org.chromium.chrome.test.R.id.close_all_tabs_menu_id);
        });
    }
}
