// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests undo and restoring of tabs in a {@link TabModel}. These tests require native initialization
 * or multiple activities. For additional tests see {@link UndoTabModelUnitTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class UndoTabModelTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Before
    public void setUp() throws InterruptedException {
        // Disable snackbars from the {@link UndoBarController} which can break this test.
        sActivityTestRule.getActivity().getSnackbarManager().disableForTesting();
    }

    private static final Tab[] EMPTY = new Tab[] {};
    private static final String TEST_URL_0 = UrlUtils.encodeHtmlDataUri("<html>test_url_0.</html>");
    private static final String TEST_URL_1 = UrlUtils.encodeHtmlDataUri("<html>test_url_1.</html>");

    private void checkState(
            final TabModel model,
            final Tab[] tabsList,
            final Tab selectedTab,
            final Tab[] closingTabs,
            final Tab[] fullTabsList,
            final Tab fullSelectedTab) {
        // Keeping these checks on the test thread so the stacks are useful for identifying
        // failures.

        // Check the selected tab.
        Assert.assertEquals("Wrong selected tab", selectedTab, TabModelUtils.getCurrentTab(model));

        // Check the list of tabs.
        Assert.assertEquals("Incorrect number of tabs", tabsList.length, model.getCount());
        for (int i = 0; i < tabsList.length; i++) {
            Assert.assertEquals(
                    "Unexpected tab at " + i, tabsList[i].getId(), model.getTabAt(i).getId());
        }

        // Check the list of tabs we expect to be closing.
        for (int i = 0; i < closingTabs.length; i++) {
            int id = closingTabs[i].getId();
            Assert.assertTrue("Tab " + id + " not in closing list", model.isClosurePending(id));
        }

        TabList fullModel = model.getComprehensiveModel();

        // Check the comprehensive selected tab.
        Assert.assertEquals(
                "Wrong selected tab", fullSelectedTab, TabModelUtils.getCurrentTab(fullModel));

        // Check the comprehensive list of tabs.
        Assert.assertEquals("Incorrect number of tabs", fullTabsList.length, fullModel.getCount());
        for (int i = 0; i < fullModel.getCount(); i++) {
            int id = fullModel.getTabAt(i).getId();
            Assert.assertEquals("Unexpected tab at " + i, fullTabsList[i].getId(), id);
        }
    }

    private void createTabOnUiThread(final ChromeTabCreator tabCreator) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabCreator.createNewTab(
                            new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
                });
    }

    private void closeTabOnUiThread(final TabModel model, final Tab tab, final boolean undoable)
            throws TimeoutException {
        // Check preconditions.
        Assert.assertFalse(tab.isClosing());
        Assert.assertTrue(tab.isInitialized());
        Assert.assertFalse(model.isClosurePending(tab.getId()));
        Assert.assertNotNull(model.getTabById(tab.getId()));

        final CallbackHelper didReceivePendingClosureHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void tabPendingClosure(Tab tab) {
                                    didReceivePendingClosureHelper.notifyCalled();
                                }
                            });

                    // Take action.
                    model.closeTabs(TabClosureParams.closeTab(tab).allowUndo(undoable).build());
                });

        boolean didMakePending = undoable && model.supportsPendingClosures();

        // Make sure the TabModel throws a tabPendingClosure callback if necessary.
        if (didMakePending) didReceivePendingClosureHelper.waitForCallback(0);

        // Check post conditions
        Assert.assertEquals(didMakePending, model.isClosurePending(tab.getId()));
        Assert.assertNull(model.getTabById(tab.getId()));
        Assert.assertTrue(tab.isClosing());
        Assert.assertEquals(didMakePending, tab.isInitialized());
    }

    private void saveStateOnUiThread(final TabModelOrchestrator orchestrator) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    orchestrator.saveState();
                });

        TabModelSelector selector = orchestrator.getTabModelSelector();
        for (int i = 0; i < selector.getModels().size(); i++) {
            TabModel model = selector.getModels().get(i);
            TabList tabs = model.getComprehensiveModel();
            for (int j = 0; j < tabs.getCount(); j++) {
                Assert.assertFalse(model.isClosurePending(tabs.getTabAt(j).getId()));
            }
        }
    }

    private void openMostRecentlyClosedTabOnUiThread(final TabModelSelector selector) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.getCurrentModel().openMostRecentlyClosedEntry();
                });
    }

    // Helper class that notifies after the tab is closed, and a tab restore service entry has been
    // created in tab restore service.
    private static class TabClosedObserver implements TabModelObserver {
        private CallbackHelper mTabClosedCallback;

        public TabClosedObserver(CallbackHelper closedCallback) {
            mTabClosedCallback = closedCallback;
        }

        @Override
        public void onFinishingTabClosure(Tab tab) {
            mTabClosedCallback.notifyCalled();
        }
    }

    /**
     * Test calling {@link TabModelOrchestrator#saveState()} commits all pending closures: Action
     * Model List Close List Comprehensive List 1. Initial State [ 0 1s ] - [ 0 1s ] 2. CloseTab(0,
     * allow undo) [ 1s ] [ 0 ] [ 0 1s ] 3. SaveState [ 1s ] - [ 1s ]
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // See crbug.com/633607
    public void testSaveStateCommitsUndos() throws TimeoutException, ExecutionException {
        TabModelOrchestrator orchestrator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelOrchestratorSupplier()
                                        .get());
        TabModelSelector selector = orchestrator.getTabModelSelector();
        TabModel model = selector.getModel(false);
        ChromeTabCreator tabCreator =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);

        Tab[] fullList = new Tab[] {tab0, tab1};

        // 1.
        checkState(model, new Tab[] {tab0, tab1}, tab1, EMPTY, fullList, tab1);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, EMPTY, fullList, tab1);

        // 3.
        saveStateOnUiThread(orchestrator);
        fullList = new Tab[] {tab1};
        checkState(model, new Tab[] {tab1}, tab1, EMPTY, fullList, tab1);
        Assert.assertTrue(tab0.isClosing());
        Assert.assertFalse(tab0.isInitialized());
    }

    /** Test opening recently closed tab using native tab restore service. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTabNative() throws TimeoutException {
        final TabModelSelector selector = sActivityTestRule.getActivity().getTabModelSelector();
        final TabModel model = selector.getModel(false);

        // Create new tab and wait until it's loaded.
        // Native can only successfully recover the tab after a page load has finished and
        // it has navigation history.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(),
                TEST_URL_0,
                false);

        // Close the tab, and commit pending closure.
        Assert.assertEquals(model.getCount(), 2);
        closeTabOnUiThread(model, model.getTabAt(1), false);
        Assert.assertEquals(1, model.getCount());
        Tab tab0 = model.getTabAt(0);
        Tab[] tabs = new Tab[] {tab0};
        checkState(model, tabs, tab0, EMPTY, tabs, tab0);

        // Recover the page.
        openMostRecentlyClosedTabOnUiThread(selector);

        Assert.assertEquals(2, model.getCount());
        tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        tabs = new Tab[] {tab0, tab1};
        Assert.assertEquals(TEST_URL_0, ChromeTabUtils.getUrlStringOnUiThread(tab1));
        checkState(model, tabs, tab1, EMPTY, tabs, tab1);
    }

    /**
     * Test opening recently closed tab when we have multiple windows. | Action | Result 1. Create
     * second window. | 2. Open tab in window 1. | 3. Open tab in window 2. | 4. Close tab in window
     * 1. | 5. Close tab in window 2. | 6. Restore tab. | Tab restored in window 2. 7. Restore tab.
     * | Tab restored in window 1.
     */
    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338997949
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.S_V2) // https://crbug.com/1297370
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    public void testOpenRecentlyClosedTabMultiWindow() throws TimeoutException {
        final ChromeTabbedActivity2 secondActivity =
                MultiWindowTestHelper.createSecondChromeTabbedActivity(
                        sActivityTestRule.getActivity());

        // Wait for the second window to be fully initialized.
        CriteriaHelper.pollUiThread(
                () -> secondActivity.getTabModelSelector().isTabStateInitialized());
        // First window context.
        final TabModelSelector firstSelector =
                sActivityTestRule.getActivity().getTabModelSelector();
        final TabModel firstModel = firstSelector.getModel(false);

        // Second window context.
        final TabModelSelector secondSelector = secondActivity.getTabModelSelector();
        final TabModel secondModel = secondSelector.getModel(false);

        // Create tabs.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(),
                TEST_URL_0,
                false);
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), secondActivity, TEST_URL_1, false);

        Assert.assertEquals("Unexpected number of tabs in first window.", 2, firstModel.getCount());
        Assert.assertEquals(
                "Unexpected number of tabs in second window.", 2, secondModel.getCount());

        // Close one tab in the first window.
        closeTabOnUiThread(firstModel, firstModel.getTabAt(1), false);
        Assert.assertEquals("Unexpected number of tabs in first window.", 1, firstModel.getCount());
        Assert.assertEquals(
                "Unexpected number of tabs in second window.", 2, secondModel.getCount());

        // Close one tab in the second window.
        closeTabOnUiThread(secondModel, secondModel.getTabAt(1), false);
        Assert.assertEquals("Unexpected number of tabs in first window.", 1, firstModel.getCount());
        Assert.assertEquals(
                "Unexpected number of tabs in second window.", 1, secondModel.getCount());

        // Restore one tab to the second selector.
        openMostRecentlyClosedTabOnUiThread(secondSelector);
        Assert.assertEquals("Unexpected number of tabs in first window.", 1, firstModel.getCount());
        Assert.assertEquals(
                "Unexpected number of tabs in second window.", 2, secondModel.getCount());

        // Restore one more tab to the first selector.
        openMostRecentlyClosedTabOnUiThread(firstSelector);

        // Check final states of both windows.
        Tab firstModelTab = firstModel.getTabAt(0);
        Tab secondModelTab = secondModel.getTabAt(0);
        Tab[] firstWindowTabs = new Tab[] {firstModelTab, firstModel.getTabAt(1)};
        Tab[] secondWindowTabs = new Tab[] {secondModelTab, secondModel.getTabAt(1)};
        checkState(
                firstModel,
                firstWindowTabs,
                firstModel.getTabAt(1),
                EMPTY,
                firstWindowTabs,
                firstModel.getTabAt(1));
        checkState(
                secondModel,
                secondWindowTabs,
                secondModel.getTabAt(1),
                EMPTY,
                secondWindowTabs,
                secondModel.getTabAt(1));
        Assert.assertEquals(TEST_URL_0, ChromeTabUtils.getUrlStringOnUiThread(firstWindowTabs[1]));
        Assert.assertEquals(TEST_URL_1, ChromeTabUtils.getUrlStringOnUiThread(secondWindowTabs[1]));

        secondActivity.finishAndRemoveTask();
    }

    /**
     * Test restoring closed tab from a closed window. | Action | Result 1. Create second window. |
     * 2. Open tab in window 2. | 3. Close tab in window 2. | 4. Close second window. | 5. Restore
     * tab. | Tab restored in first window.
     */
    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338997949
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.S_V2) // https://crbug.com/1297370
    @MinAndroidSdkLevel(24)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    public void testOpenRecentlyClosedTabMultiWindowFallback() throws TimeoutException {
        final ChromeTabbedActivity2 secondActivity =
                MultiWindowTestHelper.createSecondChromeTabbedActivity(
                        sActivityTestRule.getActivity());
        // Wait for the second window to be fully initialized.
        CriteriaHelper.pollUiThread(
                () -> secondActivity.getTabModelSelector().isTabStateInitialized());

        // First window context.
        final TabModelSelector firstSelector =
                sActivityTestRule.getActivity().getTabModelSelector();
        final TabModel firstModel = firstSelector.getModel(false);

        // Second window context.
        final TabModel secondModel = secondActivity.getTabModelSelector().getModel(false);

        // Create tab on second window.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), secondActivity, TEST_URL_1, false);
        Assert.assertEquals("Window 2 should have 2 tab.", 2, secondModel.getCount());

        // Close tab in second window, wait until tab restore service history is created.
        CallbackHelper closedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> secondModel.addObserver(new TabClosedObserver(closedCallback)));
        closeTabOnUiThread(secondModel, secondModel.getTabAt(1), false);
        closedCallback.waitForCallback(0);

        Assert.assertEquals("Window 2 should have 1 tab.", 1, secondModel.getCount());

        // Closed the second window. Must wait until it's totally closed.
        int numExpectedActivities = ApplicationStatus.getRunningActivities().size() - 1;
        secondActivity.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getRunningActivities().size(),
                            Matchers.is(numExpectedActivities));
                });
        Assert.assertEquals("Window 1 should have 1 tab.", 1, firstModel.getCount());

        // Restore closed tab from second window. It should be created in first window.
        openMostRecentlyClosedTabOnUiThread(firstSelector);
        Assert.assertEquals(
                "Closed tab in second window should be restored in the first window.",
                2,
                firstModel.getCount());
        Tab tab0 = firstModel.getTabAt(0);
        Tab tab1 = firstModel.getTabAt(1);
        Tab[] firstWindowTabs = new Tab[] {tab0, tab1};
        // After restoring tab1, it should selected as the current tab.
        checkState(firstModel, firstWindowTabs, tab1, EMPTY, firstWindowTabs, tab1);
        Assert.assertEquals(TEST_URL_1, ChromeTabUtils.getUrlStringOnUiThread(tab1));
    }
}
