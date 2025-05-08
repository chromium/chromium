// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.pm.ActivityInfo;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Test suite for the TabStrip and making sure it properly represents the TabModel backend. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "crbug.com/342984901")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
public class TabStripTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    /**
     * Tests that the initial state of the system is good. This is so the default TabStrips match
     * the TabModels and we do not have to test this in further tests.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/342984901")
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
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip", "Main"})
    public void testNewTabButtonWithOneTab() throws Exception {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "Expected original tab to be selected",
                0,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).index());

        ChromeTabUtils.clickNewTabButton(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "Expected two tabs to exist",
                2,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount());
        compareAllTabStripsWithModel();
        Assert.assertEquals(
                "Expected second tab to be selected",
                1,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).index());
    }

    /**
     * Tests that pressing the new tab button creates a new tab when many exist, properly updating
     * the selected index.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    public void testNewTabButtonWithManyTabs() throws Exception {
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 3);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                TabModelUtils.setIndex(
                                        mActivityTestRule
                                                .getActivity()
                                                .getTabModelSelector()
                                                .getModel(false),
                                        0);
                            }
                        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "Expected original tab to be selected",
                0,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).index());
        compareAllTabStripsWithModel();

        ChromeTabUtils.clickNewTabButton(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "Expected five tabs to exist",
                5,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount());
        Assert.assertEquals(
                "Expected last tab to be selected",
                4,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).index());
        compareAllTabStripsWithModel();
    }

    /** Tests that creating a new tab from the menu properly updates the TabStrip. */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/342984901")
    public void testNewTabFromMenu() throws Exception {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        compareAllTabStripsWithModel();
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "Expected two tabs to exist",
                2,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that creating a new incognito from the menu properly updates the TabStrips and
     * activates the incognito TabStrip.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/342984901")
    public void testNewIncognitoTabFromMenuAtNormalStrip() throws Exception {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse(
                "Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        compareAllTabStripsWithModel();
        Assert.assertTrue(
                "Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        Assert.assertEquals(
                "Expected normal model to have one tab",
                1,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount());
        Assert.assertEquals(
                "Expected incognito model to have one tab",
                1,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount());
    }

    /** Tests that selecting a tab properly selects the new tab. */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    public void testSelectWithTwoTabs() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "The second tab is not selected",
                1,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        selectTab(false, mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "The first tab is not selected",
                0,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that selecting a tab properly selects the new tab with many present. This lets us also
     * check that the visible tab ordering is correct.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    public void testSelectWithManyTabs() throws Exception {
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 4);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "The last tab is not selected",
                4,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        compareAllTabStripsWithModel();
        // Note: if the tab is not visible, this will fail. Currently that's not a problem, because
        // the devices we test on are wide enough.
        selectTab(false, mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "The middle tab is not selected",
                0,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests closing a tab when there are two tabs open. The remaining tab should still be selected.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/1348310")
    public void testCloseTabWithTwoTabs() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "There are not two tabs present",
                2,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(
                "The second tab is not selected",
                1,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        int initialSelectedId = mActivityTestRule.getActivity().getActivityTab().getId();
        closeTab(false, mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "There is not one tab present",
                1,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(
                "The wrong tab index is selected after close",
                0,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        Assert.assertEquals(
                "Same tab not still selected",
                initialSelectedId,
                mActivityTestRule.getActivity().getActivityTab().getId());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests closing a tab when there are many tabs open. The remaining tab should still be
     * selected, even if the index has changed.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/1348310")
    public void testCloseTabWithManyTabs() throws Exception {
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 4);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "There are not five tabs present",
                5,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(
                "The last tab is not selected",
                4,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        int initialSelectedId = mActivityTestRule.getActivity().getActivityTab().getId();
        // Note: if the tab is not visible, this will fail. Currently that's not a problem, because
        // the devices we test on are wide enough.
        closeTab(false, mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "There are not four tabs present",
                4,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(
                "The wrong tab index is selected after close",
                3,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        Assert.assertEquals(
                "Same tab not still selected",
                initialSelectedId,
                mActivityTestRule.getActivity().getActivityTab().getId());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that closing the selected tab properly closes the current tab and updates to a new
     * selected tab.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    public void testCloseSelectedTab() throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "There are not two tabs present",
                2,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(
                "The second tab is not selected",
                1,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        int newSelectionId =
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId();
        closeTab(false, mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(1).getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "There is not one tab present",
                1,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(
                "The wrong tab index is selected after close",
                0,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        Assert.assertEquals(
                "New tab not selected",
                newSelectionId,
                mActivityTestRule.getActivity().getActivityTab().getId());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that selecting "Close all tabs" from the tab menu closes all tabs. Also tests that long
     * press on close button selects the tab and displays the menu.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/1348310")
    public void testCloseAllTabsFromTabMenuClosesAllTabs() {
        // 1. Create a second tab
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "There are not two tabs present",
                2,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(
                "The second tab is not selected",
                1,
                mActivityTestRule.getActivity().getCurrentTabModel().index());

        // 2. Display "close all tabs" menu on first tab
        int tabSelectionId =
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId();
        longPressCloseTab(false, tabSelectionId);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "There are not two tabs present",
                2,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(
                "The wrong tab index is selected after long press",
                0,
                mActivityTestRule.getActivity().getCurrentTabModel().index());
        Assert.assertEquals(
                "Long pressed tab not selected",
                tabSelectionId,
                mActivityTestRule.getActivity().getActivityTab().getId());

        // 3. Invoke "close all tabs" menu action; block until action is completed
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                            .clickCloseButtonMenuItemForTesting(
                                    StripLayoutHelper.ID_CLOSE_ALL_TABS);
                });

        // 4. Ensure all tabs were closed
        Assert.assertEquals(
                "Expected no tabs to be present",
                0,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
    }

    /**
     * Tests that the "close all tabs" menu is dismissed when the orientation changes and no tabs
     * are closed.
     */
    @Test
    @LargeTest
    @Restriction({DeviceFormFactor.TABLET, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/342984901")
    public void testTabMenuDismissedOnOrientationChange() {
        // 1. Set orientation to portrait
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // 2. Open "close all tabs" menu
        int tabSelectionId =
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId();
        longPressCloseTab(false, tabSelectionId);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // 3. Set orientation to landscape and assert "close all tabs" menu is not showing
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse(
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                        .isCloseButtonMenuShowingForTesting());
        Assert.assertEquals(
                "Expected 1 tab to be present",
                1,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());

        // 4. Reset orientation
        mActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Tests that pressing the incognito toggle button properly switches between the incognito and
     * normal TabStrips.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/342984901")
    public void testToggleIncognitoMode() throws Exception {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse(
                "Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue(
                "Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        compareAllTabStripsWithModel();
        clickIncognitoToggleButton();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse(
                "Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        compareAllTabStripsWithModel();
        clickIncognitoToggleButton();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue(
                "Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Tests that closing the last incognito tab properly closes the incognito TabStrip and switches
     * to the normal TabStrip.
     */
    @Test
    @LargeTest
    @Feature({"TabStrip"})
    @Restriction(DeviceFormFactor.TABLET)
    public void testCloseLastIncognitoTab() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse(
                "Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue(
                "Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        closeTab(
                true,
                TabModelUtils.getCurrentTab(
                                mActivityTestRule
                                        .getActivity()
                                        .getTabModelSelector()
                                        .getModel(true))
                        .getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse(
                "Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        Assert.assertEquals(
                "Expected incognito strip to have no tabs",
                0,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true).getCount());
    }

    /**
     * Tests that closing all incognito tab properly closes the incognito TabStrip and switches to
     * the normal TabStrip.
     */
    @Test
    @LargeTest
    @Feature({"TabStrip"})
    @Restriction(DeviceFormFactor.TABLET)
    public void testCloseAllIncognitoTabsFromTabMenu() {
        // 1. Create two incognito tabs
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse(
                "Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue(
                "Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        Assert.assertEquals(
                "Expected incognito strip to have 2 tabs",
                2,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true).getCount());

        // 2. Open "close all tabs" menu
        int tabSelectionId =
                TabModelUtils.getCurrentTab(
                                mActivityTestRule
                                        .getActivity()
                                        .getTabModelSelector()
                                        .getModel(true))
                        .getId();
        longPressCloseTab(true, tabSelectionId);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // 3. Invoke menu action; block until action is completed
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                            .clickCloseButtonMenuItemForTesting(
                                    StripLayoutHelper.ID_CLOSE_ALL_TABS);
                });

        // 4. Ensure all incognito tabs were closed and TabStrip is switched to normal
        Assert.assertFalse(
                "Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        Assert.assertEquals(
                "Expected normal strip to have 1 tab",
                1,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount());
        Assert.assertEquals(
                "Expected incognito strip to have no tabs",
                0,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true).getCount());
    }

    /**
     * Test that switching a tab and quickly changing the model stays on the correct new tab/model
     * when the tab finishes loading (when the GL overlay goes away).
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    public void testTabSelectionViewDoesNotBreakModelSwitch() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse(
                "Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertTrue(
                "Expected incognito strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        mActivityTestRule.newIncognitoTabFromMenu();
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);

        clickIncognitoToggleButton();

        Assert.assertFalse(
                "Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Tests tapping the incognito button when an incognito tab is present and checks scrolls to
     * make the selected tab visible.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "https://crbug.com/328302523")
    public void testScrollingStripStackersWithIncognito() throws Exception {
        // Open an incognito tab to switch to the incognito model.
        mActivityTestRule.newIncognitoTabFromMenu();

        // Open enough regular tabs to cause the tab strip to scroll.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 20);

        // Check scrolling tab strip
        checkTabStrips();

        // Scroll so the selected tab is not visible.
        assertSetTabStripScrollOffset(0);
        TabModel model = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        StripLayoutTab tab =
                TabStripUtils.findStripLayoutTab(
                        mActivityTestRule.getActivity(), false, model.getTabAt(20).getId());
        assertTabVisibility(false, tab);

        // Create visibility callback helper.
        final CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(
                            new StripLayoutTab.Observer() {
                                @Override
                                public void onVisibilityChanged(boolean visible) {
                                    // Notify the callback when tab becomes visible.
                                    if (visible) helper.notifyCalled();
                                }
                            });
                });

        // Open another incognito tab to switch to the incognito model.
        mActivityTestRule.newIncognitoTabFromMenu();

        // Switch tab models to switch back to the regular tab strip.
        clickIncognitoToggleButton();

        // Wait for selected tab to be visible.
        helper.waitForCallback(0);
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    public void testScrollingStripStackersWithLastTabSelected() throws Exception {
        testScrollingStripStackersWithLastTabSelected(/* isRtl= */ false);
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip", "RTL"})
    public void testScrollingStripStackersWithLastTabSelectedRtl() throws Exception {
        testScrollingStripStackersWithLastTabSelected(/* isRtl= */ true);
    }

    /**
     * This verifies that the strip scrolls correctly when last tab is selected.
     *
     * @param isRtl whether to test RTL layouts.
     */
    private void testScrollingStripStackersWithLastTabSelected(boolean isRtl)
            throws ExecutionException {
        LocalizationUtils.setRtlForTesting(isRtl);

        // Open enough regular tabs to cause the strip to scroll
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 20);

        // Check scrolling tab strip
        checkTabStrips();
    }

    /** Verifies that the strip scrolls correctly and the correct index is selected. */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest
    // Disable due to flakiness in scrolling to hide the first tab.
    public void testScrollingStripStackersWithFirstTabSelected() throws Exception {
        // Open enough regular tabs to cause the tab strip to scroll.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 20);

        // Select the first tab by setting the index directly. It may not be visible, so don't
        // try to tap on it.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);

        // Check scrolling tab strip
        checkTabStrips();

        StripLayoutTab selectedLayoutTab =
                TabStripUtils.findStripLayoutTab(
                        mActivityTestRule.getActivity(),
                        false,
                        mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());

        // Create visibility callback helper.
        final CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selectedLayoutTab.addObserver(
                            new StripLayoutTab.Observer() {
                                @Override
                                public void onVisibilityChanged(boolean visible) {
                                    // Notify the helper when tab becomes visible.
                                    if (!visible) helper.notifyCalled();
                                }
                            });
                });

        // Flaky in scrolling to hide the first tab.

        // Scroll so the first tab is off screen and the selected tab may or may not be visible with
        // the ScrollingStripStacker.
        assertSetTabStripScrollOffset(
                (int)
                        TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                                .getScrollOffsetLimitForTesting());

        // Tab should now be hidden.
        helper.waitForCallback(0);

        assertTabVisibility(false, selectedLayoutTab);
    }

    /**
     * Verifies that the strip scrolls correctly and the correct index when a middle tab is
     * selected.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/1348310")
    public void testScrollingStripStackersWithMiddleTabSelected() throws Exception {
        // Open enough regular tabs to cause the tab strip to scroll.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 10);

        // Select the sixth tab by setting the index directly. It may not be visible, so don't
        // try to tap on it.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 5);

        // Check scrolling tab strip
        checkTabStrips();
    }

    /**
     * Test that the right and left tab strip fades are fully visible, partially visible or hidden
     * at various scroll positions. TODO(twellington): Also test these expectations in RTL.
     */
    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/338966108")
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    public void testScrollingStripStackerFadeOpacity() throws Exception {
        // Check scrolling tab strip
        checkTabStrips();

        // Open enough regular tabs to cause the strip to scroll.
        StripLayoutHelper tabStrip =
                TabStripUtils.getStripLayoutHelper(mActivityTestRule.getActivity(), false);
        while (tabStrip.getScrollOffsetLimitForTesting() >= 0) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        }

        // In RTL the expectation for left/right fade opacities is swapped.
        boolean isLeft = !LocalizationUtils.isLayoutRtl();

        // Initially the right fade (in LTR) should be hidden and the left fade should be visible.
        assertTabStripFadeFullyHidden(!isLeft);
        assertTabStripFadeFullyVisible(isLeft);

        // Scroll a little below the scroll offset limit causing the right fade (in LTR) to be
        // at partial opacity.
        assertSetTabStripScrollOffset(
                (int)
                        (TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                                        .getScrollOffsetLimitForTesting()
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
     * Test that selecting a tab that isn't currently visible causes the ScrollingStripStacker to
     * scroll to make it visible.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/342984901")
    public void testScrollingStripStackerScrollsToSelectedTab() throws Exception {
        // Check scrolling tab strip
        checkTabStrips();

        // Open enough regular tabs to hide the tab at index 0.
        TabModel model = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        final StripLayoutTab tab =
                TabStripUtils.findStripLayoutTab(
                        mActivityTestRule.getActivity(), false, model.getTabAt(0).getId());
        while (tab.isVisible()) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        }

        // Assert the tab at index 0 is not visible.
        assertTabVisibility(false, tab);

        // Create visibility callback helper.
        final CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(
                            new StripLayoutTab.Observer() {
                                @Override
                                public void onVisibilityChanged(boolean visible) {
                                    // Notify the helper when tab becomes visible.
                                    if (visible) helper.notifyCalled();
                                }
                            });
                });

        // Select tab 0.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);

        // Tab should now be visible.
        helper.waitForCallback(0);
    }

    /**
     * Test that the draw positions for tabs match expectations at various scroll positions when
     * using the ScrollingStripStacker.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip"})
    @DisabledTest(message = "crbug.com/1348310")
    public void testScrollingStripStackerTabOffsets() throws Exception {
        // Check scrolling tab strip
        checkTabStrips();

        // Open enough regular tabs to cause the strip to scroll and select the first tab.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 10);

        // Set up some variables.
        StripLayoutHelper strip =
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        StripLayoutTab[] tabs = strip.getStripLayoutTabsForTesting();
        float tabDrawWidth = tabs[0].getWidth() - StripLayoutUtils.TAB_OVERLAP_WIDTH_DP;

        // Disable animations. The animation that normally runs when scrolling the tab strip makes
        // this test flaky.
        CompositorAnimationHandler.setTestingMode(true);

        // Create callback helper to be notified when first tab becomes visible.
        final CallbackHelper visibleHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabs[0].addObserver(
                            new StripLayoutTab.Observer() {
                                @Override
                                public void onVisibilityChanged(boolean visible) {
                                    if (visible) visibleHelper.notifyCalled();
                                }
                            });
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabs[0].addObserver(
                            new StripLayoutTab.Observer() {
                                @Override
                                public void onVisibilityChanged(boolean visible) {
                                    if (!visible) notVisibleHelper.notifyCalled();
                                }
                            });
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

        // Re-enable animations.
        CompositorAnimationHandler.setTestingMode(false);
    }

    /** Tests that switching tabs hides keyboard. */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"TabStrip", "IME"})
    @DisabledTest(message = "crbug.com/342984901")
    public void testSwitchingTabsHidesKeyboard() throws Throwable {
        mActivityTestRule.loadUrl(
                "data:text/html;charset=utf-8,<html><head></head><body><form>"
                        + "<input type='text' id='input0'></form></body></html>");
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getWebContents(), "input0");
        assertWaitForKeyboardStatus(true);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        ChromeTabUtils.clickNewTabButton(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(
                "Expected two tabs to exist",
                2,
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getCount());

        assertWaitForKeyboardStatus(false);
    }

    /**
     * Tests hover enter/move/exit events associated with the tabs on the tab strip (with the tab
     * strip redesign folio treatment enabled, for maximum coverage).
     */
    @Test
    @LargeTest
    @Feature({"TabStrip"})
    @Restriction(DeviceFormFactor.TABLET)
    public void testHoverOnTabStripTabs() throws Exception {
        // Open a few regular tabs.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 4);

        // Select tabs to hover on.
        TabModel model = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        StripLayoutTab tab1 =
                TabStripUtils.findStripLayoutTab(
                        mActivityTestRule.getActivity(), false, model.getTabAt(1).getId());
        StripLayoutTab tab2 =
                TabStripUtils.findStripLayoutTab(
                        mActivityTestRule.getActivity(), false, model.getTabAt(2).getId());
        assertTabVisibility(true, tab1);
        assertTabVisibility(true, tab2);

        // Simulate a hover into tab1.
        StripLayoutHelperManager stripLayoutHelperManager =
                TabStripUtils.getStripLayoutHelperManager(mActivityTestRule.getActivity());
        Assert.assertNotNull(
                "Hover card ViewStub should not be inflated before first hover event.",
                stripLayoutHelperManager.getTabHoverCardViewStubForTesting().getParent());
        float xEnter = tab1.getDrawX() + tab1.getWidth() / 2;
        float yEnter = tab1.getDrawY() + tab1.getHeight() / 2;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_ENTER, xEnter, yEnter));

        // Verify that the card view is inflated as expected.
        var hoverCardView =
                stripLayoutHelperManager
                        .getActiveStripLayoutHelper()
                        .getTabHoverCardViewForTesting();
        Assert.assertNotNull(
                "Hover card view should be set in normal StripLayoutHelper instance.",
                stripLayoutHelperManager
                        .getStripLayoutHelper(false)
                        .getTabHoverCardViewForTesting());

        // Verify that the card view background color is correctly set.
        Assert.assertEquals(
                "Hover card background color is incorrect.",
                TabUiThemeProvider.getStripTabHoverCardBackgroundTintList(
                        hoverCardView.getContext(), false),
                hoverCardView.getBackgroundTintList());

        StripLayoutTab lastHoveredTab =
                stripLayoutHelperManager.getActiveStripLayoutHelper().getLastHoveredTab();
        Assert.assertEquals("The last hovered tab is not set correctly.", tab1, lastHoveredTab);
        Assert.assertFalse("|mFolioAttached| for tab1 should be false.", tab1.getFolioAttached());
        Assert.assertEquals(
                "tab1 container bottom margin should match.",
                StripLayoutUtils.FOLIO_DETACHED_BOTTOM_MARGIN_DP,
                tab1.getBottomMargin(),
                0.f);

        // Simulate a subsequent hover into the adjacent tab (tab2).
        float xMove = tab2.getDrawX() + tab2.getWidth() / 3;
        float yMove = tab2.getDrawY() + tab2.getHeight() / 3;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_MOVE, xMove, yMove));
        lastHoveredTab = stripLayoutHelperManager.getActiveStripLayoutHelper().getLastHoveredTab();
        Assert.assertEquals("The last hovered tab is not set correctly.", tab2, lastHoveredTab);
        Assert.assertFalse("|mFolioAttached| for tab2 should be false.", tab2.getFolioAttached());
        Assert.assertTrue("|mFolioAttached| for tab1 should be true.", tab1.getFolioAttached());
        Assert.assertEquals(
                "tab1 container bottom margin should match.",
                StripLayoutUtils.FOLIO_ATTACHED_BOTTOM_MARGIN_DP,
                tab1.getBottomMargin(),
                0.f);

        // Simulate a subsequent hover outside tab2.
        float xExit = tab2.getDrawX() + 2 * tab2.getWidth();
        float yExit = tab2.getDrawY() + 2 * tab2.getHeight();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_EXIT, xExit, yExit));
        lastHoveredTab = stripLayoutHelperManager.getActiveStripLayoutHelper().getLastHoveredTab();
        Assert.assertNull("The last hovered tab is not set correctly.", lastHoveredTab);
        Assert.assertTrue("|mFolioAttached| for tab2 should be true.", tab2.getFolioAttached());
    }

    /** Tests hover cards shown in standard as well as incognito tab models. */
    @Test
    @LargeTest
    @Feature({"TabStrip"})
    @Restriction(DeviceFormFactor.TABLET)
    public void testHoverOnTabStrip_switchTabModel() throws Exception {
        // Open regular tabs.
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 2);

        // Select a tab to hover on.
        TabModel model = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        StripLayoutTab standardTab =
                TabStripUtils.findStripLayoutTab(
                        mActivityTestRule.getActivity(), false, model.getTabAt(1).getId());
        assertTabVisibility(true, standardTab);

        // Simulate a hover into standardTab.
        StripLayoutHelperManager stripLayoutHelperManager =
                TabStripUtils.getStripLayoutHelperManager(mActivityTestRule.getActivity());
        float standardXEnter = standardTab.getDrawX() + standardTab.getWidth() / 2;
        float standardYEnter = standardTab.getDrawY() + standardTab.getHeight() / 2;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_ENTER, standardXEnter, standardYEnter));

        // Verify that the card view background color is correctly set.
        var hoverCardView =
                stripLayoutHelperManager
                        .getActiveStripLayoutHelper()
                        .getTabHoverCardViewForTesting();
        Assert.assertNotNull(
                "Hover card view should be set in normal StripLayoutHelper instance.",
                stripLayoutHelperManager
                        .getStripLayoutHelper(false)
                        .getTabHoverCardViewForTesting());
        Assert.assertEquals(
                "Hover card background color is incorrect.",
                TabUiThemeProvider.getStripTabHoverCardBackgroundTintList(
                        hoverCardView.getContext(), false),
                hoverCardView.getBackgroundTintList());

        // Open an incognito tab from the menu.
        Tab tab = mActivityTestRule.newIncognitoTabFromMenu();
        StripLayoutTab incognitoTab =
                TabStripUtils.findStripLayoutTab(
                        mActivityTestRule.getActivity(), true, tab.getId());
        assertTabVisibility(true, incognitoTab);

        // Simulate a hover into incognitoTab.
        float incognitoXEnter = incognitoTab.getDrawX() + incognitoTab.getWidth() / 2;
        float incognitoYEnter = incognitoTab.getDrawY() + incognitoTab.getHeight() / 2;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_ENTER, incognitoXEnter, incognitoYEnter));

        // Verify that the card view background color is correctly set.
        hoverCardView =
                stripLayoutHelperManager
                        .getActiveStripLayoutHelper()
                        .getTabHoverCardViewForTesting();
        Assert.assertNotNull(
                "Hover card view should be set in incognito StripLayoutHelper instance.",
                stripLayoutHelperManager
                        .getStripLayoutHelper(true)
                        .getTabHoverCardViewForTesting());
        Assert.assertEquals(
                "Hover card background color is incorrect.",
                TabUiThemeProvider.getStripTabHoverCardBackgroundTintList(
                        hoverCardView.getContext(), true),
                hoverCardView.getBackgroundTintList());
    }

    /** Tests that the tab hover state is cleared when the activity is paused. */
    @Test
    @LargeTest
    @Feature({"TabStrip"})
    @Restriction(DeviceFormFactor.TABLET)
    @DisabledTest(message = "crbug.com/342984901")
    public void testTabHoverStateClearedOnActivityPause() throws Exception {
        TabModel model = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        StripLayoutTab tab =
                TabStripUtils.findStripLayoutTab(
                        mActivityTestRule.getActivity(), false, model.getTabAt(0).getId());
        assertTabVisibility(true, tab);

        // Simulate a hover into the tab.
        StripLayoutHelperManager stripLayoutHelperManager =
                TabStripUtils.getStripLayoutHelperManager(mActivityTestRule.getActivity());
        float xEnter = tab.getDrawX() + tab.getWidth() / 2;
        float yEnter = tab.getDrawY() + tab.getHeight() / 2;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_ENTER, xEnter, yEnter));

        // Verify that the card is visible.
        var hoverCardView =
                stripLayoutHelperManager
                        .getActiveStripLayoutHelper()
                        .getTabHoverCardViewForTesting();
        Assert.assertEquals(
                "Hover card should be visible.", View.VISIBLE, hoverCardView.getVisibility());

        // Simulate activity pause.
        // Note: This doesn't really pause the activity; it just triggers the code
        // that *would* be called if the activity were to be paused. We'll need to
        // balance this with an onResumeWithNative call before ending the test.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().onPauseWithNative();
                });

        // Validate that the hover card disappears when notified that the activity
        // was paused.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Hover card should be hidden.",
                            hoverCardView.getVisibility(),
                            Matchers.is(View.GONE));
                });

        // Simulate activity resume.
        // Note: This doesn't really resume the activity; it just triggers the code
        // that *would* be called if the activity were to be resumed. The code above
        // pretended to pause the activity. We need this simulated resume so that
        // any book-keeping being performed by the activity balances out when the
        // activity is paused, and ultimately destroyed, as this test shuts down.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().onResumeWithNative();
                });
    }

    /** Tests hover enter/move/exit events associated with the tab strip buttons. */
    @Test
    @LargeTest
    @Feature({"TabStrip"})
    @Restriction(DeviceFormFactor.TABLET)
    public void testHoverOnTabStripButtons() throws Exception {
        StripLayoutHelperManager stripLayoutHelperManager =
                TabStripUtils.getStripLayoutHelperManager(mActivityTestRule.getActivity());

        // Select NTB.
        CompositorButton newTabButton = stripLayoutHelperManager.getNewTabButton();
        Assert.assertNotNull(
                "Tooltip ViewStub should not be inflated before first hover event.",
                stripLayoutHelperManager.getTooltipViewStubForTesting().getParent());

        // Simulate a hover into NTB.
        float xEnter = newTabButton.getDrawX() + newTabButton.getWidth() / 2;
        float yEnter = newTabButton.getDrawY() + newTabButton.getHeight() / 2;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_ENTER, xEnter, yEnter));

        // Verify that the tooltip is visible as expected.
        FrameLayout tooltipView =
                stripLayoutHelperManager.getTooltipManagerForTesting().getTooltipViewForTesting();
        Assert.assertNotNull("Tooltip view should be set.", tooltipView);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Tooltip should be visible.",
                            tooltipView.getVisibility(),
                            Matchers.is(View.VISIBLE));
                });

        // Enter incognito mode.
        mActivityTestRule.newIncognitoTabFromMenu();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Select MSB.
        CompositorButton modelSelectorButton = stripLayoutHelperManager.getModelSelectorButton();
        Assert.assertNotNull(
                "The modelSelectorButton should be set in StripLayoutHelperManager instance",
                modelSelectorButton);

        // Simulate a subsequent hover into the MSB.
        float xMove = modelSelectorButton.getDrawX() + modelSelectorButton.getWidth() / 3;
        float yMove = modelSelectorButton.getDrawY() + modelSelectorButton.getHeight() / 3;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_MOVE, xMove, yMove));
        Assert.assertNotNull("Tooltip view should be set.", tooltipView);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Tooltip should be visible.",
                            tooltipView.getVisibility(),
                            Matchers.is(View.VISIBLE));
                });

        // Simulate a subsequent hover outside the MSB.
        float xExit = xMove + modelSelectorButton.getWidth();
        float yExit = yMove + modelSelectorButton.getHeight();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_EXIT, xExit, yExit));
        Assert.assertNotNull("Tooltip view should be set.", tooltipView);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Tooltip should be gone.",
                            tooltipView.getVisibility(),
                            Matchers.is(View.GONE));
                });

        // Exit incognito mode.
        clickIncognitoToggleButton();

        // Simulate a subsequent hover into the MSB outside of incognito mode.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_MOVE, xMove, yMove));
        Assert.assertNotNull("Tooltip view should be set.", tooltipView);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Tooltip should be visible.",
                            tooltipView.getVisibility(),
                            Matchers.is(View.VISIBLE));
                });

        // Simulate the final hover outside the MSB.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        stripLayoutHelperManager.simulateHoverEventForTesting(
                                MotionEvent.ACTION_HOVER_EXIT, xExit, yExit));
        Assert.assertNotNull("Tooltip view should be set.", tooltipView);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Tooltip should be gone.",
                            tooltipView.getVisibility(),
                            Matchers.is(View.GONE));
                });
    }

    /** Simulates a click to the incognito toggle button. */
    protected void clickIncognitoToggleButton() {
        final CallbackHelper tabModelSelectedCallback = new CallbackHelper();
        Callback<TabModel> observer = (tabModel) -> tabModelSelectedCallback.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getTabModelSelector()
                                .getCurrentTabModelSupplier()
                                .addObserver(observer));
        StripLayoutHelperManager manager =
                TabStripUtils.getStripLayoutHelperManager(mActivityTestRule.getActivity());
        TabStripUtils.clickCompositorButton(
                manager.getModelSelectorButton(),
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity());
        try {
            tabModelSelectedCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            throw new AssertionError("Tab model selected event never occurred.", e);
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .getCurrentTabModelSupplier()
                            .removeObserver(observer);
                });
    }

    /**
     * Simulates a click on a tab, selecting it.
     *
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void selectTab(final boolean incognito, final int id) {
        ChromeTabUtils.selectTabWithAction(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                new Runnable() {
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
     *
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void closeTab(final boolean incognito, final int id) {
        ChromeTabUtils.closeTabWithAction(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                new Runnable() {
                    @Override
                    public void run() {
                        StripLayoutTab tab =
                                TabStripUtils.findStripLayoutTab(
                                        mActivityTestRule.getActivity(), incognito, id);
                        TabStripUtils.clickCompositorButton(
                                tab.getCloseButton(),
                                InstrumentationRegistry.getInstrumentation(),
                                mActivityTestRule.getActivity());
                    }
                });
    }

    /**
     * Simulates a long press on the close button of a tab. Asserts that the tab is selected and the
     * "close all tabs" menu is showing.
     *
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void longPressCloseTab(final boolean incognito, final int id) {
        ChromeTabUtils.selectTabWithAction(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                new Runnable() {
                    @Override
                    public void run() {
                        StripLayoutTab tab =
                                TabStripUtils.findStripLayoutTab(
                                        mActivityTestRule.getActivity(), incognito, id);
                        TabStripUtils.longPressCompositorButton(
                                tab.getCloseButton(),
                                InstrumentationRegistry.getInstrumentation(),
                                mActivityTestRule.getActivity());
                    }
                });
        Assert.assertTrue(
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity())
                        .isCloseButtonMenuShowingForTesting());
    }

    /**
     * Compares a TabView with the corresponding model Tab. This tries to compare as many features
     * as possible making sure the TabView properly mirrors the Tab it represents.
     *
     * @param incognito Whether or not this tab is incognito or not.
     * @param id The id of the tab to compare.
     */
    protected void compareTabViewWithModel(boolean incognito, int id) throws ExecutionException {
        TabModel model = mActivityTestRule.getActivity().getTabModelSelector().getModel(incognito);
        Tab tab = model.getTabById(id);
        StripLayoutHelper tabStrip =
                TabStripUtils.getStripLayoutHelper(mActivityTestRule.getActivity(), incognito);
        StripLayoutTab tabView =
                TabStripUtils.findStripLayoutTab(mActivityTestRule.getActivity(), incognito, id);

        Assert.assertTrue(
                "One of Tab and TabView does not exist",
                (tabView == null && tab == null) || (tabView != null && tab != null));

        if (tabView == null || tab == null) return;

        Assert.assertEquals("The IDs are not identical", tabView.getTabId(), tab.getId());

        assertTabVisibility(tabStrip, tabView);

        int tabIndexOnStrip =
                StripLayoutUtils.findIndexForTab(tabStrip.getStripLayoutTabsForTesting(), id);
        Assert.assertEquals(
                "The tab is not in the proper position ", model.indexOf(tab), tabIndexOnStrip);

        if (TabModelUtils.getCurrentTab(model) == tab
                && mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected()
                        == incognito) {
            Assert.assertEquals(
                    "ChromeTab is not in the proper selection state",
                    model.indexOf(tab),
                    tabStrip.getSelectedStripTabIndex());
        }

        // TODO(dtrainor): Compare favicon bitmaps?  Only compare a few pixels.
    }

    /**
     * Compares an entire TabStrip with the corresponding TabModel. This tries to compare as many
     * features as possible, including checking all of the tabs through compareTabViewWithModel. It
     * also checks that the incognito indicator is visible if the incognito tab is showing.
     *
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
        } else {
            Assert.assertTrue("TabStrip is not in the right visible state", model != activeModel);
        }

        CompositorButton incognitoIndicator =
                TabStripUtils.getStripLayoutHelperManager(mActivityTestRule.getActivity())
                        .getModelSelectorButton();
        if (activeModel.isIncognito()) {
            Assert.assertNotNull("Incognito indicator null in incognito mode", incognitoIndicator);
            Assert.assertTrue(
                    "Incognito indicator not visible in incognito mode",
                    incognitoIndicator.isVisible());
        } else if (mActivityTestRule.getActivity().getTabModelSelector().getModel(true).getCount()
                == 0) {
            Assert.assertFalse(
                    "Incognito indicator visible in non incognito mode",
                    incognitoIndicator.isVisible());
        }

        for (int i = 0; i < model.getCount(); ++i) {
            compareTabViewWithModel(incognito, model.getTabAt(i).getId());
        }
    }

    /**
     * Compares all TabStrips with the corresponding TabModels. This also checks if the incognito
     * toggle is visible if necessary.
     */
    protected void compareAllTabStripsWithModel() throws ExecutionException {
        compareTabStripWithModel(true);
        compareTabStripWithModel(false);
    }

    /** Check scrolling tab strip validity and auto-scrolling. */
    private void checkTabStrips() throws ExecutionException {
        TabModel model = mActivityTestRule.getActivity().getCurrentTabModel();
        int selectedTabIndex = model.index();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabStripUtils.getStripLayoutHelper(mActivityTestRule.getActivity(), true)
                            .updateScrollOffsetLimits();
                    TabStripUtils.getStripLayoutHelper(mActivityTestRule.getActivity(), false)
                            .updateScrollOffsetLimits();
                });

        // Assert that the same tab is still selected.
        Assert.assertEquals("The correct tab is not selected.", selectedTabIndex, model.index());

        // Compare all TabStrips with corresponding TabModels.
        compareAllTabStripsWithModel();

        // The scrollingStripStacker should auto-scroll to make the selected tab visible.
        StripLayoutTab selectedLayoutTab =
                TabStripUtils.findStripLayoutTab(
                        mActivityTestRule.getActivity(),
                        model.isIncognito(),
                        model.getTabAt(selectedTabIndex).getId());
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    strip.setScrollOffsetForTesting(scrollOffset);
                });

        Assert.assertEquals(
                "Tab strip scroll incorrect.", scrollOffset, strip.getScrollOffset(), 0);
        compareAllTabStripsWithModel();
    }

    /**
     * Asserts that the left or right fade is fully hidden.
     *
     * @param isLeft Whether the left fade should be checked.
     */
    private void assertTabStripFadeFullyHidden(boolean isLeft) {
        StripLayoutHelper strip =
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        if (isLeft) {
            Assert.assertEquals(
                    "Left tab strip fade visibility is incorrect.",
                    0.f,
                    strip.getLeftFadeOpacity(),
                    0);
        } else {
            Assert.assertEquals(
                    "Right tab strip fade visibility is incorrect.",
                    0.f,
                    strip.getRightFadeOpacity(),
                    0);
        }
    }

    /**
     * Asserts that the left or right fade is fully visible.
     *
     * @param isLeft Whether the left fade should be checked.
     */
    private void assertTabStripFadeFullyVisible(boolean isLeft) {
        StripLayoutHelper strip =
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        if (isLeft) {
            Assert.assertEquals(
                    "Left tab strip fade visibility is incorrect.",
                    1.f,
                    strip.getLeftFadeOpacity(),
                    0);
        } else {
            Assert.assertEquals(
                    "Right tab strip fade visibility is incorrect.",
                    1.f,
                    strip.getRightFadeOpacity(),
                    0);
        }
    }

    /**
     * Asserts that the left or right fade is partially visible.
     *
     * @param isLeft Whether the left fade should be checked.
     */
    private void assertTabStripFadePartiallyVisible(boolean isLeft) {
        StripLayoutHelper strip =
                TabStripUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        if (isLeft) {
            boolean isPartiallyVisible =
                    strip.getLeftFadeOpacity() > 0.f && strip.getLeftFadeOpacity() < 1.f;
            Assert.assertEquals(
                    "Left tab strip fade expected to be partially visible.",
                    true,
                    isPartiallyVisible);
        } else {
            boolean isPartiallyVisible =
                    strip.getRightFadeOpacity() > 0.f && strip.getRightFadeOpacity() < 1.f;
            Assert.assertEquals(
                    "Right tab strip fade expected to be partially visible.",
                    true,
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
    private void assertTabVisibility(final StripLayoutHelper tabStrip, final StripLayoutTab tabView)
            throws ExecutionException {
        // Only tabs that can currently be seen on the screen should be visible.
        Boolean shouldBeVisible =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Boolean>() {
                            @Override
                            public Boolean call() {
                                return (tabView.getDrawX() + tabView.getWidth()) >= 0
                                        && tabView.getDrawX() <= tabStrip.getWidthForTesting();
                            }
                        });
        assertTabVisibility(shouldBeVisible, tabView);
    }

    /**
     * Asserts whether a tab should be visible.
     *
     * @param shouldBeVisible Whether the tab should be visible.
     * @param tabView The StripLayoutTab associated with the tab to check.
     */
    private void assertTabVisibility(final Boolean shouldBeVisible, final StripLayoutTab tabView)
            throws ExecutionException {
        Boolean isVisible =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Boolean>() {
                            @Override
                            public Boolean call() {
                                return tabView.isVisible();
                            }
                        });

        Assert.assertEquals(
                "ChromeTab " + (shouldBeVisible ? "should" : "should not") + " be visible.",
                shouldBeVisible,
                isVisible);
    }

    /**
     * Asserts that the tab has the expected draw X position.
     *
     * @param expectedDrawX The expected draw X position.
     * @param tabView The StripLayoutTab associated with the tab to check.
     * @param index The index for the tab.
     */
    private void assertTabDrawX(float expectedDrawX, final StripLayoutTab tabView, int index)
            throws ExecutionException {
        Float tabDrawX =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Float>() {
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
     *
     * @param expectsShown Whether shown status is expected.
     */
    private void assertWaitForKeyboardStatus(final boolean expectsShown) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule
                                    .getKeyboardDelegate()
                                    .isKeyboardShowing(
                                            mActivityTestRule.getActivity(),
                                            mActivityTestRule
                                                    .getActivity()
                                                    .getActivityTab()
                                                    .getView()),
                            Matchers.is(expectsShown));
                });
    }
}
