// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.TestAnimations.EnableAnimations;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Arrays;
import java.util.List;

/** An end-to-end test of the close all tabs dialog. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CloseAllTabsDialogTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false).name("NonIncognito"),
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

    /** Tests that close all tabs works after modal dialog. */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testCloseAllTabs() {
        TabModelSelector selector =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();

        if (mIsIncognito) mActivityTestRule.newIncognitoTabFromMenu();
        navigateToCloseAllTabsDialog(selector);
        onViewWaiting(withId(org.chromium.chrome.test.R.id.positive_button), true).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(0, selector.getModel(mIsIncognito).getCount());
                });
    }

    /** Tests that close all tabs stops if dismissing modal dialog. */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testCancelCloseAllTabs() {
        TabModelSelector selector =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();

        if (mIsIncognito) mActivityTestRule.newIncognitoTabFromMenu();
        navigateToCloseAllTabsDialog(selector);

        onViewWaiting(withId(org.chromium.chrome.test.R.id.negative_button), true).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(1, selector.getModel(mIsIncognito).getCount());
                });
    }

    /**
     * Tests the custom close all tabs animation will run and close tabs. This test does not test
     * the actual animation logic beyond verifying it runs and does not crash as testing the actual
     * animation data is not possible from here.
     */
    @Test
    @LargeTest
    @EnableAnimations
    @Restriction({DeviceFormFactor.PHONE})
    @EnableFeatures({ChromeFeatureList.GTS_CLOSE_TAB_ANIMATION_KILL_SWITCH})
    public void testCloseAllTabs_CustomAnimation() {
        TabModelSelector selector =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();

        TabUiTestHelper.createTabs(mActivityTestRule.getActivity(), mIsIncognito, 8);
        navigateToCloseAllTabsDialog(selector);
        onViewWaiting(withId(org.chromium.chrome.test.R.id.positive_button), true).perform(click());

        CriteriaHelper.pollUiThread(() -> 0 == selector.getModel(mIsIncognito).getCount());
    }

    private void navigateToCloseAllTabsDialog(TabModelSelector selector) {

        assertThat(selector.getModel(mIsIncognito).getCount(), greaterThanOrEqualTo(1));

        // Open the AppMenu in the Tab Switcher and ensure it shows.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                });
        onViewWaiting(withId(org.chromium.chrome.test.R.id.app_menu_list))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        // Click close all tabs.
        if (mIsIncognito) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        AppMenuTestSupport.callOnItemClick(
                                mActivityTestRule.getAppMenuCoordinator(),
                                org.chromium.chrome.test.R.id.close_all_incognito_tabs_menu_id);
                    });
            return;
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.callOnItemClick(
                            mActivityTestRule.getAppMenuCoordinator(),
                            org.chromium.chrome.test.R.id.close_all_tabs_menu_id);
                });
    }
}
