// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.pm.ActivityInfo;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Test suite for the TabStrip and making sure it properly represents the TabModel backend.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabStripTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Tests that the initial state of the system is good.  This is so the default TabStrips match
     * the TabModels and we do not have to test this in further tests.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testInitialState() throws Exception {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that pressing the new tab button creates a new tab, properly updating the selected
     * index.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip", "Main"})
    public void testNewTabButtonWithOneTab() throws Exception {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("Expected original tab to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).index(), 0);

        ChromeTabUtils.clickNewTabButton(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("Expected two tabs to exist",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount(),
                2);
        compareAllTabStripsWithModel();
        Assert.assertEquals("Expected second tab to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).index(), 1);
    }

    /**
     * Tests that pressing the new tab button creates a new tab when many exist, properly updating
     * the selected index.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    @FlakyTest(message = "crbug.com/592961")
    public void testNewTabButtonWithManyTabs() throws Exception {
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 3);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(
                        mActivityTestRule.getActivity().getTabModelSelector().getModel(false), 0);
            }
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("Expected original tab to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).index(), 0);
        compareAllTabStripsWithModel();

        ChromeTabUtils.clickNewTabButton(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("Expected five tabs to exist",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount(),
                5);
        Assert.assertEquals("Expected last tab to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).index(), 4);
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that creating a new tab from the menu properly updates the TabStrip.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testNewTabFromMenu() throws Exception {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        compareAllTabStripsWithModel();
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("Expected two tabs to exist",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount(),
                2);
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that creating a new incognito from the menu properly updates the TabStrips and
     * activates the incognito TabStrip.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testNewIncognitoTabFromMenuAtNormalStrip() throws Exception {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse("Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        compareAllTabStripsWithModel();
        Assert.assertTrue("Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        Assert.assertEquals("Expected normal model to have one tab",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount(),
                1);
        Assert.assertEquals("Expected incognito model to have one tab",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount(),
                1);
    }

    /**
     * Tests that selecting a tab properly selects the new tab.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testSelectWithTwoTabs() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("The second tab is not selected",
                mActivityTestRule.getActivity().getCurrentTabModel().index(), 1);
        selectTab(false, mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("The first tab is not selected",
                mActivityTestRule.getActivity().getCurrentTabModel().index(), 0);
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that selecting a tab properly selects the new tab with many present.  This lets us
     * also check that the visible tab ordering is correct.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testSelectWithManyTabs() throws Exception {
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 4);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("The last tab is not selected",
                mActivityTestRule.getActivity().getCurrentTabModel().index(), 4);
        compareAllTabStripsWithModel();
        // Note: if the tab is not visible, this will fail. Currently that's not a problem, because
        // the devices we test on are wide enough.
        selectTab(false, mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("The middle tab is not selected",
                mActivityTestRule.getActivity().getCurrentTabModel().index(), 0);
        compareAllTabStripsWithModel();
    }

    /**
     * Tests closing a tab when there are two tabs open.  The remaining tab should still be
     * selected.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testCloseTabWithTwoTabs() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("There are not two tabs present",
                mActivityTestRule.getActivity().getCurrentTabModel().getCount(), 2);
        Assert.assertEquals("The second tab is not selected",
                mActivityTestRule.getActivity().getCurrentTabModel().index(), 1);
        int initialSelectedId = mActivityTestRule.getActivity().getActivityTab().getId();
        closeTab(false, mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("There is not one tab present",
                mActivityTestRule.getActivity().getCurrentTabModel().getCount(), 1);
        Assert.assertEquals("The wrong tab index is selected after close",
                mActivityTestRule.getActivity().getCurrentTabModel().index(), 0);
        Assert.assertEquals("Same tab not still selected", initialSelectedId,
                mActivityTestRule.getActivity().getActivityTab().getId());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests closing a tab when there are many tabs open.  The remaining tab should still be
     * selected, even if the index has changed.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testCloseTabWithManyTabs() throws Exception {
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 4);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("There are not five tabs present",
                mActivityTestRule.getActivity().getCurrentTabModel().getCount(), 5);
        Assert.assertEquals("The last tab is not selected",
                mActivityTestRule.getActivity().getCurrentTabModel().index(), 4);
        int initialSelectedId = mActivityTestRule.getActivity().getActivityTab().getId();
        // Note: if the tab is not visible, this will fail. Currently that's not a problem, because
        // the devices we test on are wide enough.
        closeTab(false, mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("There are not four tabs present",
                mActivityTestRule.getActivity().getCurrentTabModel().getCount(), 4);
        Assert.assertEquals("The wrong tab index is selected after close",
                mActivityTestRule.getActivity().getCurrentTabModel().index(), 3);
        Assert.assertEquals("Same tab not still selected", initialSelectedId,
                mActivityTestRule.getActivity().getActivityTab().getId());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that closing the selected tab properly closes the current tab and updates to a new
     * selected tab.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testCloseSelectedTab() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("There are not two tabs present",
                mActivityTestRule.getActivity().getCurrentTabModel().getCount(), 2);
        Assert.assertEquals("The second tab is not selected",
                mActivityTestRule.getActivity().getCurrentTabModel().index(), 1);
        int newSelectionId =
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId();
        closeTab(false, mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(1).getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("There is not one tab present",
                mActivityTestRule.getActivity().getCurrentTabModel().getCount(), 1);
        Assert.assertEquals("The wrong tab index is selected after close",
                mActivityTestRule.getActivity().getCurrentTabModel().index(), 0);
        Assert.assertEquals("New tab not selected", newSelectionId,
                mActivityTestRule.getActivity().getActivityTab().getId());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that selecting "Close all tabs" from the tab menu closes all tabs.
     * Also tests that long press on close button selects the tab and displays the menu.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testCloseAllTabsFromTabMenuClosesAllTabs() {
        // 1. Create a second tab
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("There are not two tabs present", 2,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals("The second tab is not selected", 1,
                mActivityTestRule.getActivity().getCurrentTabModel().index());

        // 2. Display tab menu on first tab
        int tabSelectionId =
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId();
        longPressCloseTab(false, tabSelectionId);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("There are not two tabs present", 2,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals("The wrong tab index is selected after long press", 0,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        Assert.assertEquals("Long pressed tab not selected", tabSelectionId,
                mActivityTestRule.getActivity().getActivityTab().getId());

        // 3. Invoke "close all tabs" menu action; block until action is completed
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                    .clickTabMenuItem(StripLayoutHelper.ID_CLOSE_ALL_TABS);
        });

        // 4. Ensure all tabs were closed
        Assert.assertEquals("Expected no tabs to be present", 0,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
    }

    /**
     * Tests that the tab menu is dismissed when the orientation changes and no tabs
     * are closed.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testTabMenuDismissedOnOrientationChange() {
        // 1. Set orientation to portrait
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // 2. Open tab menu
        int tabSelectionId =
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId();
        longPressCloseTab(false, tabSelectionId);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // 3. Set orientation to landscape and assert tab menu is not showing
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse(TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                                   .isTabMenuShowing());
        Assert.assertEquals("Expected 1 tab to be present", 1,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());

        // 4. Reset orientation
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Tests that pressing the incognito toggle button properly switches between the incognito
     * and normal TabStrips.
     */

    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testToggleIncognitoMode() throws Exception {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse("Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue("Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        compareAllTabStripsWithModel();
        clickIncognitoToggleButton();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse("Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        compareAllTabStripsWithModel();
        clickIncognitoToggleButton();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue("Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Tests that closing the last incognito tab properly closes the incognito TabStrip and
     * switches to the normal TabStrip.
     */
    @Test
    @LargeTest
    @Feature({"TabStrip"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    public void testCloseLastIncognitoTab() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse("Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue("Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        closeTab(true,
                TabModelUtils
                        .getCurrentTab(
                                mActivityTestRule.getActivity().getTabModelSelector().getModel(
                                        true))
                        .getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse("Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        Assert.assertEquals("Expected incognito strip to have no tabs",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true).getCount(), 0);
    }

    /**
     * Tests that closing all incognito tab properly closes the incognito TabStrip and
     * switches to the normal TabStrip.
     */
    @Test
    @LargeTest
    @Feature({"TabStrip"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    public void testCloseAllIncognitoTabsFromTabMenu() {
        //1. Create two incognito tabs
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse("Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue("Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        Assert.assertEquals("Expected incognito strip to have 2 tabs", 2,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true).getCount());

        // 2. Open tab menu
        int tabSelectionId =
                TabModelUtils
                        .getCurrentTab(
                                mActivityTestRule.getActivity().getTabModelSelector().getModel(
                                        true))
                        .getId();
        longPressCloseTab(true, tabSelectionId);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // 3. Invoke menu action; block until action is completed
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                    .clickTabMenuItem(StripLayoutHelper.ID_CLOSE_ALL_TABS);
        });

        // 4. Ensure all incognito tabs were closed and TabStrip is switched to normal
        Assert.assertFalse("Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        Assert.assertEquals("Expected normal strip to have 1 tab", 1,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount());
        Assert.assertEquals("Expected incognito strip to have no tabs", 0,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true).getCount());
    }

    /**
     * Test that switching a tab and quickly changing the model stays on the correct new tab/model
     * when the tab finishes loading (when the GL overlay goes away).
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testTabSelectionViewDoesNotBreakModelSwitch() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse("Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue("Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);

        clickIncognitoToggleButton();

        Assert.assertFalse("Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Compares tab strips with models after switching between the ScrollingStripStacker and
     * CascadingStripStacker when an incognito tab is present. Tests tapping the incognito
     * button while the strip is using the ScrollingStripStacker (other tests cover tapping
     * the button while using the CascadingStripStacker), and checks that switching between
     * tab models scrolls to make the selected tab visible.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testSwitchStripStackersWithIncognito() throws Exception {
        // Open an incognito tab to switch to the incognito model.
        mActivityTestRule.newIncognitoTabFromMenu();

        // Open enough regular tabs to cause the tabs to cascade or the strip to scroll depending
        // on which stacker is being used.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 10);

        // Switch to the ScrollingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(false);

        // Scroll so the selected tab is not visible.
        assertSetTabStripScrollOffset(0);
        TabModel model = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        StripLayoutTab tab = TabStripUtils.findStripLayoutTab(
                mActivityTestRule.getActivity(), false, model.getTabAt(10).getId());
        assertTabVisibility(false, tab);

        // Create visibility callback helper.
        final CallbackHelper helper = new CallbackHelper();
        tab.addObserver(new StripLayoutTab.Observer() {
            @Override
            public void onVisibilityChanged(boolean visible) {
                // Notify the callback when tab becomes visible.
                if (visible) helper.notifyCalled();
            }
        });

        // Open another incognito tab to switch to the incognito model.
        mActivityTestRule.newIncognitoTabFromMenu();

        // Switch tab models to switch back to the regular tab strip.
        clickIncognitoToggleButton();

        // Wait for selected tab to be visible.
        helper.waitForCallback(0);

        // Switch to the CascadingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(true);
    }

    /**
     * Compares tab strip with model after switching between the ScrollingStripStacker and
     * CascadingStripStacker when the last tab is selected. This also verifies that the strip
     * scrolls correctly and the correct index is selected after switching.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testSwitchStripStackersWithLastTabSelected() throws Exception {
        // Open enough regular tabs to cause the tabs to cascade or the strip to scroll depending
        // on which stacker is being used.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 10);

        // Switch to the ScrollingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(false);

        // Switch to the CascadingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(true);
    }

    /**
     * Compares tab strip with model after switching between the ScrollingStripStacker and
     * CascadingStripStacker when the first tab is selected. This also verifies that the strip
     * scrolls correctly and the correct index is selected after switching.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testSwitchStripStackersWithFirstTabSelected() throws Exception {
        // Open enough regular tabs to cause the tabs to cascade or the strip to scroll depending
        // on which stacker is being used.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 10);

        // Select the first tab by setting the index directly. It may not be visible, so don't
        // try to tap on it.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);

        // Switch to the ScrollingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(false);

        // Scroll so the first tab is off screen to verify that switching to the
        // CascadingStripStacker makes it visible again. The selected tab should always be visible
        // when using the CascadingStripStacker but may not be visible when using the
        // ScrollingStripStacker.
        assertSetTabStripScrollOffset(
                (int) TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                        .getMinimumScrollOffset());
        StripLayoutTab selectedLayoutTab =
                TabStripUtils.findStripLayoutTab(mActivityTestRule.getActivity(), false,
                        mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());
        assertTabVisibility(false, selectedLayoutTab);

        // Switch to the CascadingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(true);
    }

    /**
     * Compares tab strip with model after switching between the ScrollingStripStacker and
     * CascadingStripStacker when a middle tab is selected. This also verifies that the strip
     * scrolls correctly and the correct index is selected after switching.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testSwitchStripStackersWithMiddleTabSelected() throws Exception {
        // Open enough regular tabs to cause the tabs to cascade or the strip to scroll depending
        // on which stacker is being used.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 10);

        // Select the sixth tab by setting the index directly. It may not be visible, so don't
        // try to tap on it.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 5);

        // Switch to the ScrollingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(false);

        // Switch to the CascadingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(true);
    }

    /**
     * Test that the right and left tab strip fades are fully visible, partially visible or
     * hidden at various scroll positions.
     * TODO(twellington): Also test these expectations in RTL.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testScrollingStripStackerFadeOpacity() throws Exception {
        // Switch to the ScrollingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(false);

        // Open enough regular tabs to cause the strip to scroll.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 10);

        // In RTL the expectation for left/right fade opacities is swapped.
        boolean isLeft = !LocalizationUtils.isLayoutRtl();

        // Initially the right fade (in LTR) should be hidden and the left fade should be visible.
        assertTabStripFadeFullyHidden(!isLeft);
        assertTabStripFadeFullyVisible(isLeft);

        // Scroll a little below the minimum scroll offset causing the right fade (in LTR) to be
        // at partial opacity.
        assertSetTabStripScrollOffset(
                (int) (TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                                .getMinimumScrollOffset()
                        + StripLayoutHelper.FADE_FULL_OPACITY_THRESHOLD_DP / 2));
        assertTabStripFadePartiallyVisible(!isLeft);
        assertTabStripFadeFullyVisible(isLeft);

        // Scroll a little above 0 causing the left fade (in LTR) to be at partial opacity.
        assertSetTabStripScrollOffset(
                (int) (0 - StripLayoutHelper.FADE_FULL_OPACITY_THRESHOLD_DP / 2));
        assertTabStripFadeFullyVisible(!isLeft);
        assertTabStripFadePartiallyVisible(isLeft);

        // Scroll to 0 causing the left fade (in LTR) to be hidden.
        assertSetTabStripScrollOffset(0);
        assertTabStripFadeFullyHidden(isLeft);
        assertTabStripFadeFullyVisible(!isLeft);
    }

    /**
     * Test that selecting a tab that isn't currently visible causes the ScrollingStripStacker
     * to scroll to make it visible.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testScrollingStripStackerScrollsToSelectedTab() throws Exception {
        // Switch to the ScrollingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(false);

        // Open enough regular tabs to cause the strip to scroll.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 10);

        // Get tab at index 0 and assert it is not visible.
        TabModel model = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        final StripLayoutTab tab = TabStripUtils.findStripLayoutTab(
                mActivityTestRule.getActivity(), false, model.getTabAt(0).getId());
        assertTabVisibility(false, tab);

        // Create visibility callback helper.
        final CallbackHelper helper = new CallbackHelper();
        tab.addObserver(new StripLayoutTab.Observer() {
            @Override
            public void onVisibilityChanged(boolean visible) {
                // Notify the helper when tab becomes visible.
                if (visible) helper.notifyCalled();
            }
        });

        // Select tab 0.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);

        // Tab should now be visible.
        helper.waitForCallback(0);
    }

    /**
     * Test that the draw positions for tabs match expectations at various scroll positions
     * when using the ScrollingStripStacker.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testScrollingStripStackerTabOffsets() throws Exception {
        // Switch to the ScrollingStripStacker.
        setShouldCascadeTabsAndCheckTabStrips(false);

        // Open enough regular tabs to cause the strip to scroll and select the first tab.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 10);

        // Set up some variables.
        StripLayoutHelper strip =
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        StripLayoutTab[] tabs = strip.getStripLayoutTabs();
        float tabDrawWidth = tabs[0].getWidth() - strip.getTabOverlapWidth();

        // Disable animations. The animation that normally runs when scrolling the tab strip makes
        // this test flaky.
        strip.disableAnimationsForTesting();

        // Create callback helper to be notified when first tab becomes visible.
        final CallbackHelper visibleHelper = new CallbackHelper();
        tabs[0].addObserver(new StripLayoutTab.Observer() {
            @Override
            public void onVisibilityChanged(boolean visible) {
                if (visible) visibleHelper.notifyCalled();
            }
        });

        // Switch to the first tab and wait until it's visible.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);
        visibleHelper.waitForCallback(0);

        // Check initial model validity.
        compareAllTabStripsWithModel();

        // Assert getStripLayoutTabs() returns the expected number of tabs.
        Assert.assertEquals("Unexpected number of StripLayoutTabs.", 11, tabs.length);

        // Create callback helper to be notified when first tab is no longer visible.
        final CallbackHelper notVisibleHelper = new CallbackHelper();
        tabs[0].addObserver(new StripLayoutTab.Observer() {
            @Override
            public void onVisibilityChanged(boolean visible) {
                if (!visible) notVisibleHelper.notifyCalled();
            }
        });

        // Scroll tab strip to 0 and check tab positions.
        assertSetTabStripScrollOffset(0);
        for (int i = 0; i < tabs.length; i++) {
            assertTabDrawX(i * tabDrawWidth, tabs[i], i);
        }

        // Scroll tab strip a little and check tab draw positions.
        assertSetTabStripScrollOffset(-25);
        for (int i = 0; i < tabs.length; i++) {
            assertTabDrawX(i * tabDrawWidth - 25.f, tabs[i], i);
        }

        // Scroll tab strip a lot and check tab draw positions.
        assertSetTabStripScrollOffset(-500);
        for (int i = 0; i < tabs.length; i++) {
            assertTabDrawX(i * tabDrawWidth - 500.f, tabs[i], i);
        }

        // Wait for the first tab in the strip to no longer be visible.
        notVisibleHelper.waitForCallback(0);
    }

    /**
     * Tests that switching tabs hides keyboard.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip", "IME"})
    public void testSwitchingTabsHidesKeyboard() throws Throwable {
        mActivityTestRule.loadUrl("data:text/html;charset=utf-8,<html><head></head><body><form>"
                + "<input type='text' id='input0'></form></body></html>");
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getWebContents(), "input0");
        assertWaitForKeyboardStatus(true);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        ChromeTabUtils.clickNewTabButton(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals("Expected two tabs to exist",
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount(),
                2);

        assertWaitForKeyboardStatus(false);
    }

    /**
     * Take a model index and figure out which index it will be in the TabStrip's view hierarchy.
     * @param tabCount The number of tabs.
     * @param selectedIndex The index of the selected tab.
     * @param modelPos The position in the model we want to map.
     * @return The position in the view hierarchy that represents the tab at modelPos.
     */
    private int mapModelToViewIndex(int tabCount, int selectedIndex, int modelPos) {
        if (modelPos < selectedIndex) {
            return modelPos;
        } else if (modelPos == selectedIndex) {
            return tabCount - 1;
        } else {
            return tabCount - 1 - modelPos + selectedIndex;
        }
    }

    /**
     * Simulates a click to the incognito toggle button.
     */
    protected void clickIncognitoToggleButton() {
        final CallbackHelper tabModelSelectedCallback = new CallbackHelper();
        TabModelSelectorObserver observer = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                tabModelSelectedCallback.notifyCalled();
            }
        };
        mActivityTestRule.getActivity().getTabModelSelector().addObserver(observer);
        StripLayoutHelperManager manager =
                TabStripUtils.getStripLayoutHelperManager(mActivityTestRule.getActivity());
        TabStripUtils.clickCompositorButton(manager.getModelSelectorButton(),
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        try {
            tabModelSelectedCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Tab model selected event never occurred.");
        }
        mActivityTestRule.getActivity().getTabModelSelector().removeObserver(observer);
    }

    /**
     * Simulates a click on a tab, selecting it.
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void selectTab(final boolean incognito, final int id) {
        ChromeTabUtils.selectTabWithAction(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), new Runnable() {
                    @Override
                    public void run() {
                        TabStripUtils.clickTab(
                                TabStripUtils.findStripLayoutTab(
                                        mActivityTestRule.getActivity(), incognito, id),
                                InstrumentationRegistry.getInstrumentation(),
                                mActivityTestRule.getActivity());
                    }
                });
    }

    /**
     * Simulates a click on the close button of a tab.
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void closeTab(final boolean incognito, final int id) {
        ChromeTabUtils.closeTabWithAction(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), new Runnable() {
                    @Override
                    public void run() {
                        StripLayoutTab tab = TabStripUtils.findStripLayoutTab(
                                mActivityTestRule.getActivity(), incognito, id);
                        TabStripUtils.clickCompositorButton(tab.getCloseButton(),
                                InstrumentationRegistry.getInstrumentation(),
                                mActivityTestRule.getActivity());
                    }
                });
    }

    /**
     * Simulates a long press on the close button of a tab. Asserts that the tab
     * is selected and the tab menu is showing.
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void longPressCloseTab(final boolean incognito, final int id) {
        ChromeTabUtils.selectTabWithAction(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), new Runnable() {
                    @Override
                    public void run() {
                        StripLayoutTab tab = TabStripUtils.findStripLayoutTab(
                                mActivityTestRule.getActivity(), incognito, id);
                        TabStripUtils.longPressCompositorButton(tab.getCloseButton(),
                                InstrumentationRegistry.getInstrumentation(),
                                mActivityTestRule.getActivity());
                    }
                });
        Assert.assertTrue(TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                                  .isTabMenuShowing());
    }

    /**
     * Compares a TabView with the corresponding model Tab.  This tries to compare as many
     * features as possible making sure the TabView properly mirrors the Tab it represents.
     * @param incognito Whether or not this tab is incognito or not.
     * @param id The id of the tab to compare.
     */
    protected void compareTabViewWithModel(boolean incognito, int id) throws ExecutionException {
        TabModel model = mActivityTestRule.getActivity().getTabModelSelector().getModel(incognito);
        Tab tab = TabModelUtils.getTabById(model, id);
        StripLayoutHelper tabStrip =
                TabStripUtils.getStripLayoutHelper(mActivityTestRule.getActivity(), incognito);
        StripLayoutTab tabView =
                TabStripUtils.findStripLayoutTab(mActivityTestRule.getActivity(), incognito, id);

        Assert.assertTrue("One of Tab and TabView does not exist",
                (tabView == null && tab == null) || (tabView != null && tab != null));

        if (tabView == null || tab == null) return;

        Assert.assertEquals("The IDs are not identical", tabView.getId(), tab.getId());

        int assumedTabViewIndex = mapModelToViewIndex(model.getCount(), model.index(),
                model.indexOf(tab));

        Assert.assertEquals("The tab is not in the proper position ", assumedTabViewIndex,
                tabStrip.visualIndexOfTab(tabView));

        if (TabModelUtils.getCurrentTab(model) == tab
                && mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected()
                        == incognito) {
            Assert.assertTrue("ChromeTab is not in the proper selection state",
                    tabStrip.isForegroundTab(tabView));
            if (tabStrip.shouldCascadeTabs()) {
                Assert.assertEquals(
                        "ChromeTab is not completely visible, but is selected. The selected "
                                + "tab should be visible when the CascadingStripStacker is in use.",
                        tabView.getVisiblePercentage(), 1.0f, 0);
            }
        }

        if (!tabStrip.shouldCascadeTabs()) {
            assertTabVisibilityForScrollingStripStacker(tabStrip, tabView);
        }

        // TODO(dtrainor): Compare favicon bitmaps?  Only compare a few pixels.
    }

    /**
     * Compares an entire TabStrip with the corresponding TabModel. This tries to compare
     * as many features as possible, including checking all of the tabs through
     * compareTabViewWithModel.  It also checks that the incognito indicator is visible if the
     * incognito tab is showing.
     * @param incognito Whether or not to check the incognito or normal TabStrip.
     */
    protected void compareTabStripWithModel(boolean incognito) throws ExecutionException {
        TabModel model = mActivityTestRule.getActivity().getTabModelSelector().getModel(incognito);
        StripLayoutHelper strip =
                TabStripUtils.getStripLayoutHelper(mActivityTestRule.getActivity(), incognito);
        StripLayoutHelper activeStrip =
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        TabModel activeModel = mActivityTestRule.getActivity().getCurrentTabModel();

        if (activeModel.isIncognito() == incognito) {
            Assert.assertEquals("TabStrip is not in the right visible state", strip, activeStrip);
            Assert.assertEquals("TabStrip does not have the same number of views as the model",
                    strip.getTabCount(), model.getCount());
        } else {
            Assert.assertTrue("TabStrip is not in the right visible state", model != activeModel);
        }

        CompositorButton incognitoIndicator =
                TabStripUtils.getStripLayoutHelperManager(mActivityTestRule.getActivity())
                        .getModelSelectorButton();
        if (activeModel.isIncognito()) {
            Assert.assertNotNull("Incognito indicator null in incognito mode", incognitoIndicator);
            Assert.assertTrue("Incognito indicator not visible in incognito mode",
                    incognitoIndicator.isVisible());
        } else if (mActivityTestRule.getActivity().getTabModelSelector().getModel(true).getCount()
                == 0) {
            Assert.assertFalse("Incognito indicator visible in non incognito mode",
                    incognitoIndicator.isVisible());
        }

        for (int i = 0; i < model.getCount(); ++i) {
            compareTabViewWithModel(incognito, model.getTabAt(i).getId());
        }
    }

    /**
     * Compares all TabStrips with the corresponding TabModels.  This also checks if the incognito
     * toggle is visible if necessary.
     */
    protected void compareAllTabStripsWithModel() throws ExecutionException {
        compareTabStripWithModel(true);
        compareTabStripWithModel(false);
    }

    /**
     * Sets whether the strip should cascade tabs and checks for validity.
     *
     * @param shouldCascadeTabs Whether the {@link CascadingStripStacker} should be used. If false,
     *                          the {@link ScrollingStripStacker} will be used instead.
     */
    private void setShouldCascadeTabsAndCheckTabStrips(final boolean shouldCascadeTabs)
            throws ExecutionException {
        TabModel model = mActivityTestRule.getActivity().getCurrentTabModel();
        int selectedTabIndex = model.index();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabStripUtils.getStripLayoutHelper(mActivityTestRule.getActivity(), true)
                    .setShouldCascadeTabs(shouldCascadeTabs);
            TabStripUtils.getStripLayoutHelper(mActivityTestRule.getActivity(), false)
                    .setShouldCascadeTabs(shouldCascadeTabs);
        });

        // Assert that the correct StripStacker is being used.
        Assert.assertEquals(shouldCascadeTabs
                        ? "Expected CascadingStripStacker but was ScrollingStripStacker."
                        : "Expected ScrollingStripStacker but was CascadingStripStacker.",
                shouldCascadeTabs,
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                        .shouldCascadeTabs());

        // Assert that the same tab is still selected.
        Assert.assertEquals("The correct tab is not selected.", selectedTabIndex, model.index());

        // Compare all TabStrips with corresponding TabModels.
        compareAllTabStripsWithModel();

        // The selected tab should always be visible in the CascadingStripStacker and switching to
        // the ScrollingStripStacker should auto-scroll to make the selected tab visible.
        StripLayoutTab selectedLayoutTab =
                TabStripUtils.findStripLayoutTab(mActivityTestRule.getActivity(),
                        model.isIncognito(), model.getTabAt(selectedTabIndex).getId());
        assertTabVisibility(true, selectedLayoutTab);
    }

    /**
     * Scrolls the tab strip to the desired position and checks for validity.
     *
     * @param scrollOffset The end scroll position for the tab strip.
     */
    private void assertSetTabStripScrollOffset(final int scrollOffset) throws ExecutionException {
        final StripLayoutHelper strip =
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { strip.setScrollOffsetForTesting(scrollOffset); });

        Assert.assertEquals("Tab strip scroll incorrect.", scrollOffset, strip.getScrollOffset());
        compareAllTabStripsWithModel();
    }

    /**
     * Asserts that the left or right fade is fully hidden.
     * @param isLeft Whether the left fade should be checked.
     */
    private void assertTabStripFadeFullyHidden(boolean isLeft) {
        StripLayoutHelper strip =
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        if (isLeft) {
            Assert.assertEquals("Left tab strip fade visibility is incorrect.", 0.f,
                    strip.getLeftFadeOpacity(), 0);
        } else {
            Assert.assertEquals("Right tab strip fade visibility is incorrect.", 0.f,
                    strip.getRightFadeOpacity(), 0);
        }
    }

    /**
     * Asserts that the left or right fade is fully visible.
     * @param isLeft Whether the left fade should be checked.
     */
    private void assertTabStripFadeFullyVisible(boolean isLeft) {
        StripLayoutHelper strip =
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        if (isLeft) {
            Assert.assertEquals("Left tab strip fade visibility is incorrect.", 1.f,
                    strip.getLeftFadeOpacity(), 0);
        } else {
            Assert.assertEquals("Right tab strip fade visibility is incorrect.", 1.f,
                    strip.getRightFadeOpacity(), 0);
        }
    }

    /**
     * Asserts that the left or right fade is partially visible.
     * @param isLeft Whether the left fade should be checked.
     */
    private void assertTabStripFadePartiallyVisible(boolean isLeft) {
        StripLayoutHelper strip =
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        if (isLeft) {
            boolean isPartiallyVisible = strip.getLeftFadeOpacity() > 0.f
                    && strip.getLeftFadeOpacity() < 1.f;
            Assert.assertEquals("Left tab strip fade expected to be partially visible.", true,
                    isPartiallyVisible);
        } else {
            boolean isPartiallyVisible = strip.getRightFadeOpacity() > 0.f
                    && strip.getRightFadeOpacity() < 1.f;
            Assert.assertEquals("Right tab strip fade expected to be partially visible.", true,
                    isPartiallyVisible);
        }
    }

    /**
     * Checks visible percentage and visibility for the given tab. Should only be called when the
     * ScrollingStripStacker is in use.
     *
     * @param tabStrip The StripLayoutHelper that owns the tab.
     * @param tabView The StripLayoutTab associated with the tab to check.
     */
    private void assertTabVisibilityForScrollingStripStacker(final StripLayoutHelper tabStrip,
            final StripLayoutTab tabView) throws ExecutionException {
        // The visible percent for all tabs is 1.0 in the ScrollingStripStacker.
        Assert.assertEquals("ChromeTab is not completely visible. All tabs should be visible when "
                        + "the ScrollingStripStacker is in use.",
                tabView.getVisiblePercentage(), 1.0f, 0);

        // Only tabs that can currently be seen on the screen should be visible.
        Boolean shouldBeVisible = TestThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return (tabView.getDrawX() + tabView.getWidth()) >= 0
                        && tabView.getDrawX() <= tabStrip.getWidth();
            }
        });
        assertTabVisibility(shouldBeVisible, tabView);
    }

    /**
     * Asserts whether a tab should be visible.
     * @param shouldBeVisible Whether the tab should be visible.
     * @param tabView The StripLayoutTab associated with the tab to check.
     */
    private void assertTabVisibility(final Boolean shouldBeVisible, final StripLayoutTab tabView)
            throws ExecutionException {
        Boolean isVisible = TestThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return tabView.isVisible();
            }
        });

        Assert.assertEquals(
                "ChromeTab " + (shouldBeVisible ? "should" : "should not") + " be visible.",
                shouldBeVisible, isVisible);
    }

    /**
     * Asserts that the tab has the expected draw X position.
     * @param expectedDrawX The expected draw X position.
     * @param tabView The StripLayoutTab associated with the tab to check.
     * @param index The index for the tab.
     */
    private void assertTabDrawX(float expectedDrawX, final StripLayoutTab tabView, int index)
            throws ExecutionException {
        Float tabDrawX = TestThreadUtils.runOnUiThreadBlocking(new Callable<Float>() {
            @Override
            public Float call() {
                return tabView.getDrawX();
            }
        });

        Assert.assertEquals(
                "Incorrect draw position for tab at " + index, expectedDrawX, tabDrawX, 0);
    }

    /**
     * Asserts that we get the keyboard status to be shown or hidden.
     * @param expectsShown Whether shown status is expected.
     */
    private void assertWaitForKeyboardStatus(final boolean expectsShown) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                updateFailureReason("expectsShown: " + expectsShown);
                return expectsShown
                        == mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                                   mActivityTestRule.getActivity(),
                                   mActivityTestRule.getActivity().getActivityTab().getView());
            }
        });
    }
}
