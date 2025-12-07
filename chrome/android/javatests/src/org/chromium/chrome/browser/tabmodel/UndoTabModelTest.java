// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.leaveTabSwitcher;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitor;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
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
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private static final ActivityLifecycleMonitor sMonitor =
            ActivityLifecycleMonitorRegistry.getInstance();

    private WebPageStation mPage;

    @Before
    public void setUp() throws InterruptedException {
        mPage = mActivityTestRule.startOnBlankPage();
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getTabModelSelector().isTabStateInitialized());
        // When closing the last tab we enter the tab switcher. Just start there to ensure
        // determinism.
        enterTabSwitcher(mActivityTestRule.getActivity());

        // Ensure the 0th tab in the regular tab model is selected.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelSelector selector =
                            mActivityTestRule.getActivity().getTabModelSelector();
                    selector.selectModel(false);
                    selector.getModel(false).setIndex(0, TabSelectionType.FROM_USER);
                    assertEquals(1, selector.getModel(false).getCount());
                    assertTrue(selector.getModel(false).isActiveModel());
                });

        // Disable snackbars from the {@link UndoBarController} which can break this test.
        mActivityTestRule.getActivity().getSnackbarManager().disableForTesting();
    }

    @After
    public void tearDown() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabModelSelector selector = cta.getTabModelSelector();
        int regularTabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            selector.selectModel(false);
                            selector.commitAllTabClosures();
                            selector.getModel(true)
                                    .getTabRemover()
                                    .closeTabs(
                                            TabClosureParams.closeAllTabs()
                                                    .allowUndo(false)
                                                    .build(),
                                            /* allowDialog= */ false);
                            return selector.getModel(false).getCount();
                        });
        if (regularTabCount == 0) {
            Tab tab = createTab(/* isIncognito= */ false);
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        selector.selectModel(false);
                        selector.getModel(false).setIndex(0, TabSelectionType.FROM_USER);
                    });
        }
        LayoutManager layoutManager = cta.getLayoutManager();
        boolean shouldLeaveTabSwitcher =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)
                                    && !layoutManager.isLayoutStartingToHide(
                                            LayoutType.TAB_SWITCHER);
                        });
        if (shouldLeaveTabSwitcher) {
            leaveTabSwitcher(cta);
        } else {
            LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        }
    }

    private static final Tab[] EMPTY = new Tab[] {};
    private static final String TEST_URL_0 = UrlUtils.encodeHtmlDataUri("<html>test_url_0.</html>");
    private static final String TEST_URL_1 = UrlUtils.encodeHtmlDataUri("<html>test_url_1.</html>");

    private String getSelectedString(TabList model, Tab tab) {
        return tab == null
                ? "null"
                : String.valueOf(tab.getId())
                        + " at "
                        + String.valueOf(
                                ThreadUtils.runOnUiThreadBlocking(() -> model.indexOf(tab)));
    }

    private void checkState(
            final TabModel model,
            final Tab[] tabsList,
            final Tab selectedTab,
            final Tab[] closingTabs,
            final Tab[] fullTabsList,
            final Tab fullSelectedTab) {
        // Keeping these checks on the test thread so the stacks are useful for identifying
        // failures.

        // Check the list of tabs.
        assertEquals("Incorrect number of tabs", tabsList.length, getTabCountOnUiThread(model));
        for (int i = 0; i < tabsList.length; i++) {
            int j = i;
            Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(j));
            assertEquals("Unexpected tab at " + i, tabsList[i].getId(), tab.getId());
        }

        // Check the selected tab.
        Tab currentTab =
                ThreadUtils.runOnUiThreadBlocking(() -> TabModelUtils.getCurrentTab(model));
        assertEquals(
                "Wrong selected tab was "
                        + getSelectedString(model, currentTab)
                        + " expected "
                        + getSelectedString(model, selectedTab),
                selectedTab,
                currentTab);

        // Check the list of tabs we expect to be closing.
        for (int i = 0; i < closingTabs.length; i++) {
            int id = closingTabs[i].getId();
            boolean isClosurePending =
                    ThreadUtils.runOnUiThreadBlocking(() -> model.isClosurePending(id));
            assertTrue("Tab " + id + " not in closing list", isClosurePending);
        }

        TabList fullModel = ThreadUtils.runOnUiThreadBlocking(() -> model.getComprehensiveModel());

        // Check the comprehensive list of tabs.
        int fullTabCount = ThreadUtils.runOnUiThreadBlocking(() -> fullModel.getCount());
        assertEquals(
                "Incorrect number of tabs in comprehensive model",
                fullTabsList.length,
                fullTabCount);
        for (int i = 0; i < fullTabsList.length; i++) {
            int j = i;
            int id = ThreadUtils.runOnUiThreadBlocking(() -> fullModel.getTabAt(j).getId());
            assertEquals(
                    "Unexpected tab in comprehensive model at " + i, fullTabsList[i].getId(), id);
        }

        // Check the comprehensive selected tab.
        Tab fullCurrentTab =
                ThreadUtils.runOnUiThreadBlocking(() -> TabModelUtils.getCurrentTab(fullModel));
        assertEquals(
                "Wrong selected tab in comprehensive model actual "
                        + getSelectedString(fullModel, fullCurrentTab)
                        + " expected "
                        + getSelectedString(fullModel, fullSelectedTab),
                fullSelectedTab,
                fullCurrentTab);
    }

    private void createTabOnUiThread(final ChromeTabCreator tabCreator) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabCreator.createNewTab(
                            new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
                });
    }

    private void closeTab(final TabModel model, final Tab tab, final boolean undoable)
            throws TimeoutException {
        // Check preconditions.
        assertFalse(tab.isClosing());
        assertTrue(tab.isInitialized());
        assertFalse(ThreadUtils.runOnUiThreadBlocking(() -> model.isClosurePending(tab.getId())));
        assertNotNull(ThreadUtils.runOnUiThreadBlocking(() -> model.getTabById(tab.getId())));

        final CallbackHelper didReceivePendingClosureHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void onTabClosePending(
                                        List<Tab> tabs,
                                        boolean isAllTabs,
                                        @TabClosingSource int closingSource) {
                                    didReceivePendingClosureHelper.notifyCalled();
                                }
                            });

                    // Take action.
                    model.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(undoable).build(),
                                    /* allowDialog= */ false);
                });

        boolean didMakePending =
                undoable
                        && ThreadUtils.runOnUiThreadBlocking(() -> model.supportsPendingClosures());

        // Make sure the TabModel throws a onTabClosePending callback if necessary.
        if (didMakePending) didReceivePendingClosureHelper.waitForCallback(0);

        // Check post conditions
        assertEquals(
                didMakePending,
                ThreadUtils.runOnUiThreadBlocking(() -> model.isClosurePending(tab.getId())));
        assertNull(ThreadUtils.runOnUiThreadBlocking(() -> model.getTabById(tab.getId())));
        assertTrue(tab.isClosing());
        assertEquals(didMakePending, tab.isInitialized());
    }

    private void saveStateOnUiThread(final TabModelOrchestrator orchestrator) {
        ThreadUtils.runOnUiThreadBlocking(() -> orchestrator.saveState());

        TabModelSelector selector = orchestrator.getTabModelSelector();
        for (int i = 0; i < selector.getModels().size(); i++) {
            TabModel model = selector.getModels().get(i);
            TabList tabs = ThreadUtils.runOnUiThreadBlocking(() -> model.getComprehensiveModel());
            int numTabs = ThreadUtils.runOnUiThreadBlocking(() -> tabs.getCount());
            for (int j = 0; j < numTabs; j++) {
                int k = j;
                assertFalse(
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> model.isClosurePending(tabs.getTabAt(k).getId())));
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
        private final CallbackHelper mTabClosedCallback;

        public TabClosedObserver(CallbackHelper closedCallback) {
            mTabClosedCallback = closedCallback;
        }

        @Override
        public void onFinishingTabClosure(Tab tab, @TabClosingSource int closingSource) {
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
                                mActivityTestRule
                                        .getActivity()
                                        .getTabModelOrchestratorSupplier()
                                        .get());
        TabModelSelector selector = orchestrator.getTabModelSelector();
        TabModel model = selector.getModel(false);
        ChromeTabCreator tabCreator =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(1));

        Tab[] fullList = new Tab[] {tab0, tab1};

        // 1.
        checkState(model, new Tab[] {tab0, tab1}, tab1, EMPTY, fullList, tab1);

        // 2.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, EMPTY, fullList, tab1);

        // 3.
        saveStateOnUiThread(orchestrator);
        fullList = new Tab[] {tab1};
        checkState(model, new Tab[] {tab1}, tab1, EMPTY, fullList, tab1);
        assertTrue(tab0.isClosing());
        assertFalse(tab0.isInitialized());
    }

    /** Test opening recently closed tab using native tab restore service. */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTabNative() throws TimeoutException {
        final TabModelSelector selector = mActivityTestRule.getActivity().getTabModelSelector();
        final TabModel model = selector.getModel(false);

        // Create new tab and wait until it's loaded.
        // Native can only successfully recover the tab after a page load has finished and
        // it has navigation history.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                TEST_URL_0,
                false);

        // Close the tab, and commit pending closure.
        assertEquals(getTabCountOnUiThread(model), 2);
        closeTab(
                model, ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(1)), false);
        assertEquals(1, getTabCountOnUiThread(model));
        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab[] tabs = new Tab[] {tab0};
        checkState(model, tabs, tab0, EMPTY, tabs, tab0);

        // Recover the page.
        openMostRecentlyClosedTabOnUiThread(selector);

        assertEquals(2, getTabCountOnUiThread(model));
        tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(1));
        tabs = new Tab[] {tab0, tab1};
        assertEquals(TEST_URL_0, ChromeTabUtils.getUrlStringOnUiThread(tab1));
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
    @DisableIf.Device(DeviceFormFactor.ONLY_TABLET) // https://crbug.com/338997949
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.R) // https://crbug.com/1297370
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    public void testOpenRecentlyClosedTabMultiWindow() throws TimeoutException {
        final ChromeTabbedActivity2 secondActivity =
                MultiWindowTestHelper.createSecondChromeTabbedActivity(
                        mActivityTestRule.getActivity());

        // Wait for the second window to be fully initialized.
        CriteriaHelper.pollUiThread(
                () -> secondActivity.getTabModelSelector().isTabStateInitialized());
        // First window context.
        final TabModelSelector firstSelector =
                mActivityTestRule.getActivity().getTabModelSelector();
        final TabModel firstModel = firstSelector.getModel(false);

        // Second window context.
        final TabModelSelector secondSelector = secondActivity.getTabModelSelector();
        final TabModel secondModel = secondSelector.getModel(false);

        // Create tabs.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                TEST_URL_0,
                false);
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), secondActivity, TEST_URL_1, false);

        assertEquals(
                "Unexpected number of tabs in first window.", 2, getTabCountOnUiThread(firstModel));
        assertEquals(
                "Unexpected number of tabs in second window.",
                2,
                getTabCountOnUiThread(secondModel));

        // Close one tab in the first window.
        closeTab(
                firstModel, ThreadUtils.runOnUiThreadBlocking(() -> firstModel.getTabAt(1)), false);
        assertEquals(
                "Unexpected number of tabs in first window.", 1, getTabCountOnUiThread(firstModel));
        assertEquals(
                "Unexpected number of tabs in second window.",
                2,
                getTabCountOnUiThread(secondModel));

        // Close one tab in the second window.
        closeTab(
                secondModel,
                ThreadUtils.runOnUiThreadBlocking(() -> secondModel.getTabAt(1)),
                false);
        assertEquals(
                "Unexpected number of tabs in first window.", 1, getTabCountOnUiThread(firstModel));
        assertEquals(
                "Unexpected number of tabs in second window.",
                1,
                getTabCountOnUiThread(secondModel));

        // Restore one tab to the second selector.
        openMostRecentlyClosedTabOnUiThread(secondSelector);
        assertEquals(
                "Unexpected number of tabs in first window.", 1, getTabCountOnUiThread(firstModel));
        assertEquals(
                "Unexpected number of tabs in second window.",
                2,
                getTabCountOnUiThread(secondModel));

        // Restore one more tab to the first selector.
        openMostRecentlyClosedTabOnUiThread(firstSelector);

        // Check final states of both windows.
        Tab firstModelTab = ThreadUtils.runOnUiThreadBlocking(() -> firstModel.getTabAt(0));
        Tab secondModelTab = ThreadUtils.runOnUiThreadBlocking(() -> secondModel.getTabAt(0));
        Tab firstModelTab1 = ThreadUtils.runOnUiThreadBlocking(() -> firstModel.getTabAt(1));
        Tab secondModelTab1 = ThreadUtils.runOnUiThreadBlocking(() -> secondModel.getTabAt(1));
        Tab[] firstWindowTabs = new Tab[] {firstModelTab, firstModelTab1};
        Tab[] secondWindowTabs = new Tab[] {secondModelTab, secondModelTab1};
        checkState(
                firstModel,
                firstWindowTabs,
                firstModelTab1,
                EMPTY,
                firstWindowTabs,
                firstModelTab1);
        checkState(
                secondModel,
                secondWindowTabs,
                secondModelTab1,
                EMPTY,
                secondWindowTabs,
                secondModelTab1);
        assertEquals(TEST_URL_0, ChromeTabUtils.getUrlStringOnUiThread(firstWindowTabs[1]));
        assertEquals(TEST_URL_1, ChromeTabUtils.getUrlStringOnUiThread(secondWindowTabs[1]));

        secondActivity.finishAndRemoveTask();
    }

    /**
     * Test restoring closed tab from a closed window. | Action | Result 1. Create second window. |
     * 2. Open tab in window 2. | 3. Close tab in window 2. | 4. Close second window. | 5. Restore
     * tab. | Tab restored in first window.
     */
    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.ONLY_TABLET) // https://crbug.com/338997949
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.R) // https://crbug.com/1297370
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    public void testOpenRecentlyClosedTabMultiWindowFallback() throws TimeoutException {
        final ChromeTabbedActivity2 secondActivity =
                MultiWindowTestHelper.createSecondChromeTabbedActivity(
                        mActivityTestRule.getActivity());
        // Wait for the second window to be fully initialized.
        CriteriaHelper.pollUiThread(
                () -> secondActivity.getTabModelSelector().isTabStateInitialized());

        // First window context.
        final TabModelSelector firstSelector =
                mActivityTestRule.getActivity().getTabModelSelector();
        final TabModel firstModel = firstSelector.getModel(false);

        // Second window context.
        final TabModel secondModel = secondActivity.getTabModelSelector().getModel(false);

        // Create tab on second window.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), secondActivity, TEST_URL_1, false);
        assertEquals("Window 2 should have 2 tab.", 2, getTabCountOnUiThread(secondModel));

        // Close tab in second window, wait until tab restore service history is created.
        CallbackHelper closedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> secondModel.addObserver(new TabClosedObserver(closedCallback)));
        closeTab(
                secondModel,
                ThreadUtils.runOnUiThreadBlocking(() -> secondModel.getTabAt(1)),
                false);
        closedCallback.waitForCallback(0);

        assertEquals("Window 2 should have 1 tab.", 1, getTabCountOnUiThread(secondModel));

        // Closed the second window. Must wait until it's totally closed.
        int numExpectedActivities = ApplicationStatus.getRunningActivities().size() - 1;
        secondActivity.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getRunningActivities().size(),
                            Matchers.is(numExpectedActivities));
                });
        assertEquals("Window 1 should have 1 tab.", 1, getTabCountOnUiThread(firstModel));

        // Restore closed tab from second window. It should be created in first window.
        openMostRecentlyClosedTabOnUiThread(firstSelector);
        assertEquals(
                "Closed tab in second window should be restored in the first window.",
                2,
                getTabCountOnUiThread(firstModel));
        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> firstModel.getTabAt(0));
        Tab tab1 = ThreadUtils.runOnUiThreadBlocking(() -> firstModel.getTabAt(1));
        Tab[] firstWindowTabs = new Tab[] {tab0, tab1};
        // After restoring tab1, it should selected as the current tab.
        checkState(firstModel, firstWindowTabs, tab1, EMPTY, firstWindowTabs, tab1);
        assertEquals(TEST_URL_1, ChromeTabUtils.getUrlStringOnUiThread(tab1));
    }

    private Tab createTab(boolean isIncognito) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab =
                            mActivityTestRule
                                    .getActivity()
                                    .getTabCreator(isIncognito)
                                    .createNewTab(
                                            new LoadUrlParams("about:blank"),
                                            TabLaunchType.FROM_CHROME_UI,
                                            null);
                    TabModelSelector selector =
                            mActivityTestRule.getActivity().getTabModelSelector();
                    selector.selectModel(isIncognito);
                    TabModel model = selector.getModel(isIncognito);
                    model.setIndex(model.indexOf(tab), TabSelectionType.FROM_USER);
                    return tab;
                });
    }

    private void selectTab(final TabModel model, final Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.setIndex(
                            TabModelUtils.getTabIndexById(model, tab.getId()),
                            TabSelectionType.FROM_USER);
                });
    }

    private void closeMultipleTabs(
            final TabModel model, final List<Tab> tabs, final boolean undoable)
            throws TimeoutException {
        closeMultipleTabsInternal(
                model,
                () ->
                        model.getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTabs(tabs)
                                                .allowUndo(undoable)
                                                .build(),
                                        /* allowDialog= */ false),
                undoable);
    }

    private void closeAllTabs(final TabModel model, final boolean undoable)
            throws TimeoutException {
        closeMultipleTabsInternal(
                model,
                () ->
                        model.getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeAllTabs().allowUndo(undoable).build(),
                                        /* allowDialog= */ false),
                undoable);
    }

    private void closeMultipleTabsInternal(
            final TabModel model, final Runnable closeRunnable, final boolean undoable)
            throws TimeoutException {
        final CallbackHelper didReceivePendingClosureHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void onTabClosePending(
                                        List<Tab> tabs,
                                        boolean isAllTabs,
                                        @TabClosingSource int closingSource) {
                                    didReceivePendingClosureHelper.notifyCalled();
                                }
                            });
                    closeRunnable.run();
                });

        boolean didMakePending =
                undoable
                        && ThreadUtils.runOnUiThreadBlocking(() -> model.supportsPendingClosures());

        if (didMakePending) didReceivePendingClosureHelper.waitForCallback(0);
    }

    private void cancelTabClosure(final TabModel model, final Tab tab) throws TimeoutException {
        // Check preconditions.
        assertTrue(tab.isClosing());
        assertTrue(tab.isInitialized());
        assertTrue(ThreadUtils.runOnUiThreadBlocking(() -> model.isClosurePending(tab.getId())));
        assertNull(ThreadUtils.runOnUiThreadBlocking(() -> model.getTabById(tab.getId())));

        final CallbackHelper didReceiveWillCancelClosureHelper = new CallbackHelper();
        final CallbackHelper didReceiveClosureCancelledHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void willUndoTabClosure(List<Tab> tabs, boolean isAllTabs) {
                                    didReceiveWillCancelClosureHelper.notifyCalled();
                                }

                                @Override
                                public void tabClosureUndone(Tab tab) {
                                    didReceiveClosureCancelledHelper.notifyCalled();
                                }
                            });

                    // Take action.
                    model.cancelTabClosure(tab.getId());
                });

        // Make sure the TabModel throws a willUndoTabClosure and tabClosureUndone.
        didReceiveWillCancelClosureHelper.waitForCallback(0);
        didReceiveClosureCancelledHelper.waitForCallback(0);

        // Check post conditions.
        assertFalse(ThreadUtils.runOnUiThreadBlocking(() -> model.isClosurePending(tab.getId())));
        assertNotNull(ThreadUtils.runOnUiThreadBlocking(() -> model.getTabById(tab.getId())));
        assertFalse(tab.isClosing());
        assertTrue(tab.isInitialized());
    }

    private void cancelAllTabClosures(final TabModel model, final Tab[] expectedToClose)
            throws TimeoutException {
        final CallbackHelper willUndoTabClosureHelper = new CallbackHelper();
        final CallbackHelper tabClosureUndoneHelper = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void willUndoTabClosure(List<Tab> tabs, boolean isAllTabs) {
                                    for (int i = 0; i < tabs.size(); i++) {
                                        willUndoTabClosureHelper.notifyCalled();
                                    }
                                }

                                @Override
                                public void tabClosureUndone(Tab currentTab) {
                                    tabClosureUndoneHelper.notifyCalled();
                                }
                            });
                });

        for (int i = 0; i < expectedToClose.length; i++) {
            Tab tab = expectedToClose[i];
            assertTrue(tab.isClosing());
            assertTrue(tab.isInitialized());
            assertTrue(
                    ThreadUtils.runOnUiThreadBlocking(() -> model.isClosurePending(tab.getId())));
            assertNull(ThreadUtils.runOnUiThreadBlocking(() -> model.getTabById(tab.getId())));
        }

        for (int i = 0; i < expectedToClose.length; i++) {
            Tab tab = expectedToClose[i];
            int finalI = i;
            ThreadUtils.runOnUiThreadBlocking(
                    () -> model.cancelTabClosure(expectedToClose[finalI].getId()));
        }

        willUndoTabClosureHelper.waitForCallback(0, expectedToClose.length);
        tabClosureUndoneHelper.waitForCallback(0, expectedToClose.length);

        for (int i = 0; i < expectedToClose.length; i++) {
            final Tab tab = expectedToClose[i];
            assertFalse(
                    ThreadUtils.runOnUiThreadBlocking(() -> model.isClosurePending(tab.getId())));
            assertNotNull(ThreadUtils.runOnUiThreadBlocking(() -> model.getTabById(tab.getId())));
            assertFalse(tab.isClosing());
            assertTrue(tab.isInitialized());
        }
    }

    private void commitTabClosure(final TabModel model, final Tab tab) throws TimeoutException {
        // Check preconditions.
        assertTrue(tab.isClosing());
        assertTrue(tab.isInitialized());
        assertTrue(ThreadUtils.runOnUiThreadBlocking(() -> model.isClosurePending(tab.getId())));
        assertNull(ThreadUtils.runOnUiThreadBlocking(() -> model.getTabById(tab.getId())));

        final CallbackHelper didReceiveClosureCommittedHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void tabClosureCommitted(Tab tab) {
                                    didReceiveClosureCommittedHelper.notifyCalled();
                                }
                            });

                    // Take action.
                    model.commitTabClosure(tab.getId());
                });

        // Make sure the TabModel throws a tabClosureCommitted.
        didReceiveClosureCommittedHelper.waitForCallback(0);

        // Check post conditions
        assertFalse(ThreadUtils.runOnUiThreadBlocking(() -> model.isClosurePending(tab.getId())));
        assertNull(ThreadUtils.runOnUiThreadBlocking(() -> model.getTabById(tab.getId())));
        assertTrue(tab.isClosing());
        assertFalse(tab.isInitialized());
    }

    private void commitAllTabClosures(final TabModel model, Tab[] expectedToClose)
            throws TimeoutException {
        final CallbackHelper tabClosureCommittedHelper = new CallbackHelper();
        int callCount = tabClosureCommittedHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int i = 0; i < expectedToClose.length; i++) {
                        Tab tab = expectedToClose[i];
                        assertTrue(tab.isClosing());
                        assertTrue(tab.isInitialized());
                        assertTrue(model.isClosurePending(tab.getId()));
                    }

                    // Make sure that this TabModel throws the right events.
                    model.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void tabClosureCommitted(Tab currentTab) {
                                    tabClosureCommittedHelper.notifyCalled();
                                }
                            });

                    model.commitAllTabClosures();
                });

        tabClosureCommittedHelper.waitForCallback(callCount, expectedToClose.length);
        for (int i = 0; i < expectedToClose.length; i++) {
            final Tab tab = expectedToClose[i];
            assertTrue(tab.isClosing());
            assertFalse(tab.isInitialized());
            assertFalse(
                    ThreadUtils.runOnUiThreadBlocking(() -> model.isClosurePending(tab.getId())));
        }
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testSingleTab() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));

        Tab[] fullList = new Tab[] {tab0};

        // 1.
        checkState(model, new Tab[] {tab0}, tab0, EMPTY, fullList, tab0);

        // 2.
        closeTab(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] {tab0}, fullList, tab0);

        // 3.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, EMPTY, fullList, tab0);

        // 4.
        closeTab(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] {tab0}, fullList, tab0);

        // 5.
        commitTabClosure(model, tab0);
        fullList = EMPTY;
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);

        // 6.
        tab0 = createTab(isIncognito);
        fullList = new Tab[] {tab0};
        checkState(model, new Tab[] {tab0}, tab0, EMPTY, fullList, tab0);

        // 7.
        closeTab(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] {tab0}, fullList, tab0);

        // 8.
        commitAllTabClosures(model, new Tab[] {tab0});
        fullList = EMPTY;
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);

        // 9.
        tab0 = createTab(isIncognito);
        fullList = new Tab[] {tab0};
        checkState(model, new Tab[] {tab0}, tab0, EMPTY, fullList, tab0);

        // 10.
        closeTab(model, tab0, false);
        fullList = EMPTY;
        checkState(model, EMPTY, null, EMPTY, fullList, null);
        assertTrue(tab0.isClosing());
        assertFalse(tab0.isInitialized());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testTwoTabs() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1};

        // 1.
        checkState(model, new Tab[] {tab0, tab1}, tab1, EMPTY, fullList, tab1);

        // 2.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 3.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1}, tab1, EMPTY, fullList, tab1);

        // 4.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 5.
        closeTab(model, tab1, true);
        checkState(model, EMPTY, null, new Tab[] {tab0, tab1}, fullList, tab0);

        // 6.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 7.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1}, tab1, EMPTY, fullList, tab1);

        // 8.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1}, fullList, tab0);

        // 9.
        closeTab(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] {tab0, tab1}, fullList, tab0);

        // 10.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 11.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1}, tab0, EMPTY, fullList, tab0);

        // 12.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1}, fullList, tab0);

        // 13.
        closeTab(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] {tab0, tab1}, fullList, tab0);

        // 14.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1}, fullList, tab0);

        // 15.
        closeTab(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] {tab0, tab1}, fullList, tab0);

        // 16.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1}, fullList, tab0);

        // 17.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab0, tab1}, tab0, EMPTY, fullList, tab0);

        // 18.
        closeTab(model, tab0, false);
        fullList = new Tab[] {tab1};
        checkState(model, new Tab[] {tab1}, tab1, EMPTY, fullList, tab1);

        // 19.
        tab0 = createTab(isIncognito);
        fullList = new Tab[] {tab1, tab0};
        checkState(model, new Tab[] {tab1, tab0}, tab0, EMPTY, fullList, tab0);

        // 20.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 21.
        commitTabClosure(model, tab0);
        fullList = new Tab[] {tab1};
        checkState(model, new Tab[] {tab1}, tab1, EMPTY, fullList, tab1);

        // 22.
        tab0 = createTab(isIncognito);
        fullList = new Tab[] {tab1, tab0};
        checkState(model, new Tab[] {tab1, tab0}, tab0, EMPTY, fullList, tab0);

        // 23.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 24.
        closeTab(model, tab1, true);
        checkState(model, EMPTY, null, new Tab[] {tab1, tab0}, fullList, tab1);

        // 25.
        commitAllTabClosures(model, new Tab[] {tab1, tab0});
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testTwoTabsOneNonUndoableOperation() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1};

        checkState(model, new Tab[] {tab0, tab1}, tab1, EMPTY, fullList, tab1);

        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        closeTab(model, tab1, false);
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testInOrderRestore() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);

        final Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1, tab2, tab3}, tab3, new Tab[] {tab0}, fullList, tab3);

        // 3.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab2, tab3}, tab3, new Tab[] {tab1, tab0}, fullList, tab3);

        // 4.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab3}, tab3, new Tab[] {tab2, tab1, tab0}, fullList, tab3);

        // 5.
        closeTab(model, tab3, true);
        checkState(model, EMPTY, null, new Tab[] {tab3, tab2, tab1, tab0}, fullList, tab0);

        // 6.
        cancelTabClosure(model, tab3);
        checkState(model, new Tab[] {tab3}, tab3, new Tab[] {tab2, tab1, tab0}, fullList, tab3);

        // 7.
        cancelTabClosure(model, tab2);
        checkState(model, new Tab[] {tab2, tab3}, tab3, new Tab[] {tab1, tab0}, fullList, tab3);

        // 8.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab1, tab2, tab3}, tab3, new Tab[] {tab0}, fullList, tab3);

        // 9.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 10.
        selectTab(model, tab3);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 11.
        closeTab(model, tab3, true);
        checkState(model, new Tab[] {tab0, tab1, tab2}, tab2, new Tab[] {tab3}, fullList, tab2);

        // 12.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab1}, tab1, new Tab[] {tab2, tab3}, fullList, tab1);

        // 13.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1, tab2, tab3}, fullList, tab0);

        // 14.
        closeTab(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] {tab0, tab1, tab2, tab3}, fullList, tab0);

        // 15.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1, tab2, tab3}, fullList, tab0);

        // 16.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab0, tab1}, tab0, new Tab[] {tab2, tab3}, fullList, tab0);

        // 17.
        cancelTabClosure(model, tab2);
        checkState(model, new Tab[] {tab0, tab1, tab2}, tab0, new Tab[] {tab3}, fullList, tab0);

        // 18.
        cancelTabClosure(model, tab3);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab0, EMPTY, fullList, tab0);

        // 19.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab1, tab3}, tab0, new Tab[] {tab2}, fullList, tab0);

        // 20.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1, tab3}, tab1, new Tab[] {tab0, tab2}, fullList, tab1);

        // 21.
        closeTab(model, tab3, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab3, tab0, tab2}, fullList, tab1);

        // 22.
        cancelTabClosure(model, tab3);
        checkState(model, new Tab[] {tab1, tab3}, tab1, new Tab[] {tab0, tab2}, fullList, tab1);

        // 23.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1, tab3}, tab0, new Tab[] {tab2}, fullList, tab0);

        // 24.
        cancelTabClosure(model, tab2);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab0, EMPTY, fullList, tab0);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testReverseOrderRestore() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1, tab2, tab3}, tab3, new Tab[] {tab0}, fullList, tab3);

        // 3.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab2, tab3}, tab3, new Tab[] {tab1, tab0}, fullList, tab3);

        // 4.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab3}, tab3, new Tab[] {tab2, tab1, tab0}, fullList, tab3);

        // 5.
        closeTab(model, tab3, true);
        checkState(model, EMPTY, null, new Tab[] {tab3, tab2, tab1, tab0}, fullList, tab0);

        // 6.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab3, tab2, tab1}, fullList, tab0);

        // 7.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab0, tab1}, tab0, new Tab[] {tab3, tab2}, fullList, tab0);

        // 8.
        cancelTabClosure(model, tab2);
        checkState(model, new Tab[] {tab0, tab1, tab2}, tab0, new Tab[] {tab3}, fullList, tab0);

        // 9.
        cancelTabClosure(model, tab3);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 10.
        closeTab(model, tab3, true);
        checkState(model, new Tab[] {tab0, tab1, tab2}, tab2, new Tab[] {tab3}, fullList, tab2);

        // 11.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab1}, tab1, new Tab[] {tab2, tab3}, fullList, tab1);

        // 12.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1, tab2, tab3}, fullList, tab0);

        // 13.
        closeTab(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] {tab0, tab1, tab2, tab3}, fullList, tab0);

        // 14.
        cancelTabClosure(model, tab3);
        checkState(model, new Tab[] {tab3}, tab3, new Tab[] {tab0, tab1, tab2}, fullList, tab3);

        // 15.
        cancelTabClosure(model, tab2);
        checkState(model, new Tab[] {tab2, tab3}, tab3, new Tab[] {tab0, tab1}, fullList, tab3);

        // 16.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab1, tab2, tab3}, tab3, new Tab[] {tab0}, fullList, tab3);

        // 17.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab0, EMPTY, fullList, tab0);

        // 18.
        selectTab(model, tab3);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 19.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab1, tab3}, tab3, new Tab[] {tab2}, fullList, tab3);

        // 20.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1, tab3}, tab3, new Tab[] {tab0, tab2}, fullList, tab3);

        // 21.
        closeTab(model, tab3, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab3, tab0, tab2}, fullList, tab1);

        // 22.
        cancelTabClosure(model, tab2);
        checkState(model, new Tab[] {tab1, tab2}, tab1, new Tab[] {tab3, tab0}, fullList, tab1);

        // 23.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1, tab2}, tab1, new Tab[] {tab3}, fullList, tab1);

        // 24.
        cancelTabClosure(model, tab3);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testOutOfOrder1() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1, tab2, tab3}, tab3, new Tab[] {tab0}, fullList, tab3);

        // 3.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab2, tab3}, tab3, new Tab[] {tab1, tab0}, fullList, tab3);

        // 4.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab3}, tab3, new Tab[] {tab2, tab1, tab0}, fullList, tab3);

        // 5.
        closeTab(model, tab3, true);
        checkState(model, EMPTY, null, new Tab[] {tab3, tab2, tab1, tab0}, fullList, tab0);

        // 6.
        cancelTabClosure(model, tab2);
        checkState(model, new Tab[] {tab2}, tab2, new Tab[] {tab3, tab1, tab0}, fullList, tab2);

        // 7.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab1, tab2}, tab2, new Tab[] {tab3, tab0}, fullList, tab2);

        // 8.
        cancelTabClosure(model, tab3);
        checkState(model, new Tab[] {tab1, tab2, tab3}, tab3, new Tab[] {tab0}, fullList, tab3);

        // 9.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 10.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, new Tab[] {tab1}, fullList, tab3);

        // 11.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 12.
        closeTab(model, tab3, false);
        fullList = new Tab[] {tab0, tab1, tab2};
        checkState(model, new Tab[] {tab0, tab1, tab2}, tab2, EMPTY, fullList, tab2);

        // 13.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2}, tab2, new Tab[] {tab1}, fullList, tab2);

        // 14.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab2}, tab2, new Tab[] {tab0, tab1}, fullList, tab2);

        // 15.
        commitTabClosure(model, tab0);
        fullList = new Tab[] {tab1, tab2};
        checkState(model, new Tab[] {tab2}, tab2, new Tab[] {tab1}, fullList, tab2);

        // 16.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab1, tab2}, tab2, EMPTY, fullList, tab2);

        // 17.
        closeTab(model, tab2, false);
        fullList = new Tab[] {tab1};
        checkState(model, new Tab[] {tab1}, tab1, EMPTY, fullList, tab1);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testOutOfOrder2() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, new Tab[] {tab1}, fullList, tab3);

        // 3.
        closeTab(model, tab3, true);
        checkState(model, new Tab[] {tab0, tab2}, tab2, new Tab[] {tab3, tab1}, fullList, tab2);

        // 4.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab0, tab1, tab2}, tab2, new Tab[] {tab3}, fullList, tab2);

        // 5.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab1}, tab1, new Tab[] {tab2, tab3}, fullList, tab1);

        // 6.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0, tab2, tab3}, fullList, tab1);

        // 7.
        commitTabClosure(model, tab0);
        fullList = new Tab[] {tab1, tab2, tab3};
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab2, tab3}, fullList, tab1);

        // 8.
        cancelTabClosure(model, tab3);
        checkState(model, new Tab[] {tab1, tab3}, tab1, new Tab[] {tab2}, fullList, tab1);

        // 9.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab3}, tab3, new Tab[] {tab1, tab2}, fullList, tab3);

        // 10.
        commitTabClosure(model, tab2);
        fullList = new Tab[] {tab1, tab3};
        checkState(model, new Tab[] {tab3}, tab3, new Tab[] {tab1}, fullList, tab3);

        // 11.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab1, tab3}, tab1, EMPTY, fullList, tab1);

        // 12.
        closeTab(model, tab3, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab3}, fullList, tab1);

        // 13.
        closeTab(model, tab1, true);
        checkState(model, EMPTY, null, new Tab[] {tab1, tab3}, fullList, tab1);

        // 14.
        commitAllTabClosures(model, new Tab[] {tab1, tab3});
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testCloseAll_UndoSupported() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, fullList, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTab(model, tab1, /* undoable= */ true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, new Tab[] {tab1}, fullList, tab3);

        // 3.
        closeTab(model, tab2, /* undoable= */ true);
        checkState(model, new Tab[] {tab0, tab3}, tab3, new Tab[] {tab1, tab2}, fullList, tab3);

        // 4.
        closeAllTabs(model, /* undoable= */ true);
        checkState(model, EMPTY, null, fullList, fullList, tab0);

        // 5.
        cancelAllTabClosures(model, fullList);
        checkState(model, fullList, tab3, EMPTY, fullList, tab3);

        // 6.
        closeAllTabs(model, /* undoable= */ true);
        checkState(model, EMPTY, null, fullList, fullList, tab0);

        // 7.
        commitAllTabClosures(model, fullList);
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
        assertTrue(tab0.isClosing());
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertTrue(tab3.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab1.isInitialized());
        assertFalse(tab2.isInitialized());
        assertFalse(tab3.isInitialized());

        // 8.
        tab0 = createTab(isIncognito);
        fullList = new Tab[] {tab0};
        checkState(model, new Tab[] {tab0}, tab0, EMPTY, fullList, tab0);

        // 9.
        closeAllTabs(model, /* undoable= */ true);
        checkState(model, EMPTY, null, fullList, fullList, tab0);
        assertTrue(tab0.isClosing());
        assertTrue(tab0.isInitialized());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testCloseAll_UndoNotSupported() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(
                model,
                /* tabsList= */ fullList,
                /* selectedTab= */ tab3,
                /* closingTabs= */ EMPTY,
                /* fullTabsList= */ fullList,
                /* fullSelectedTab= */ tab3);

        // 2.
        closeTab(model, tab1, /* undoable= */ true);
        checkState(
                model,
                /* tabsList= */ new Tab[] {tab0, tab2, tab3},
                /* selectedTab= */ tab3,
                /* closingTabs= */ new Tab[] {tab1},
                /* fullTabsList= */ fullList,
                /* fullSelectedTab= */ tab3);

        // 3.
        closeTab(model, tab2, /* undoable= */ true);
        checkState(
                model,
                /* tabsList= */ new Tab[] {tab0, tab3},
                /* selectedTab= */ tab3,
                /* closingTabs= */ new Tab[] {tab1, tab2},
                /* fullTabsList= */ fullList,
                /* fullSelectedTab= */ tab3);

        // 4.
        closeAllTabs(model, /* undoable= */ false);
        checkState(
                model,
                /* tabsList= */ EMPTY,
                /* selectedTab= */ null,
                /* closingTabs= */ EMPTY,
                /* fullTabsList= */ EMPTY,
                /* fullSelectedTab= */ null);
        assertTrue(tab0.isClosing());
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertTrue(tab3.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab1.isInitialized());
        assertFalse(tab2.isInitialized());
        assertFalse(tab3.isInitialized());

        // 5.
        tab0 = createTab(isIncognito);
        fullList = new Tab[] {tab0};
        checkState(
                model,
                /* tabsList= */ new Tab[] {tab0},
                /* selectedTab= */ tab0,
                /* closingTabs= */ EMPTY,
                /* fullTabsList= */ fullList,
                /* fullSelectedTab= */ tab0);

        // 6.
        closeAllTabs(model, /* undoable= */ false);
        checkState(
                model,
                /* tabsList= */ EMPTY,
                /* selectedTab= */ null,
                /* closingTabs= */ EMPTY,
                /* fullTabsList= */ EMPTY,
                /* fullSelectedTab= */ null);
        assertTrue(tab0.isClosing());
        assertFalse(tab0.isInitialized());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testCloseTab() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, new Tab[] {tab1}, fullList, tab3);

        // 3.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab3}, tab3, new Tab[] {tab1, tab2}, fullList, tab3);

        // 4.
        closeTab(model, tab3, false);
        fullList = new Tab[] {tab0};
        checkState(model, new Tab[] {tab0}, tab0, EMPTY, fullList, tab0);
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertFalse(tab1.isInitialized());
        assertFalse(tab2.isInitialized());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testMoveTab() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, new Tab[] {tab1}, fullList, tab3);

        // 3.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab3}, tab3, new Tab[] {tab1, tab2}, fullList, tab3);

        // 4.
        ThreadUtils.runOnUiThreadBlocking(() -> model.moveTab(tab0.getId(), 1));
        fullList = new Tab[] {tab3, tab0};
        checkState(model, new Tab[] {tab3, tab0}, tab3, EMPTY, fullList, tab3);
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertFalse(tab1.isInitialized());
        assertFalse(tab2.isInitialized());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testAddTab() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, new Tab[] {tab1}, fullList, tab3);

        // 3.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab3}, tab3, new Tab[] {tab1, tab2}, fullList, tab3);

        // 4.
        Tab tab4 = createTab(isIncognito);
        fullList = new Tab[] {tab0, tab3, tab4};
        checkState(model, new Tab[] {tab0, tab3, tab4}, tab4, EMPTY, fullList, tab4);
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertFalse(tab1.isInitialized());
        assertFalse(tab2.isInitialized());

        // 5.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab3, tab4}, tab4, new Tab[] {tab0}, fullList, tab4);

        // 6.
        closeTab(model, tab3, true);
        checkState(model, new Tab[] {tab4}, tab4, new Tab[] {tab3, tab0}, fullList, tab4);

        // 7.
        closeTab(model, tab4, true);
        checkState(model, EMPTY, null, new Tab[] {tab4, tab3, tab0}, fullList, tab0);

        // 8.
        Tab tab5 = createTab(isIncognito);
        fullList = new Tab[] {tab5};
        checkState(model, new Tab[] {tab5}, tab5, EMPTY, fullList, tab5);
        assertTrue(tab0.isClosing());
        assertTrue(tab3.isClosing());
        assertTrue(tab4.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab3.isInitialized());
        assertFalse(tab4.isInitialized());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testIncognito_UndoAlwaysNotSupported() throws TimeoutException {
        final boolean isIncognito = true;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = createTab(isIncognito);
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, EMPTY, fullList, tab3);
        assertFalse(
                "Incognito model should not support pending closures",
                ThreadUtils.runOnUiThreadBlocking(() -> model.supportsPendingClosures()));

        // 2.
        // Note: Despite the "undoable=true" setup, incognito tabs won't support undo.
        closeTab(model, tab1, /* undoable= */ true);
        fullList = new Tab[] {tab0, tab2, tab3};
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, EMPTY, fullList, tab3);
        assertTrue(tab1.isClosing());
        assertFalse(tab1.isInitialized());

        // 3.
        // Note: Despite the "undoable=true" setup, incognito tabs won't support undo.
        closeAllTabs(model, /* undoable= */ true);
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
        assertTrue(tab0.isClosing());
        assertTrue(tab2.isClosing());
        assertTrue(tab3.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab2.isInitialized());
        assertFalse(tab3.isInitialized());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testIncognito_UndoAlwaysNotSupportedOnFinishingMultipleTabClosure()
            throws TimeoutException {
        final boolean isIncognito = true;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = createTab(isIncognito);
        Tab tab1 = createTab(isIncognito);
        Tab tab2 = createTab(isIncognito);
        Tab tab3 = createTab(isIncognito);
        Tab tab4 = createTab(isIncognito);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3, tab4};

        // 1.
        checkState(model, fullList, tab4, EMPTY, fullList, tab4);
        assertFalse(
                "Incognito model should not support pending closures",
                ThreadUtils.runOnUiThreadBlocking(() -> model.supportsPendingClosures()));

        final ArrayList<Tab> lastClosedTabs = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void onFinishingMultipleTabClosure(
                                        List<Tab> tabs, boolean canRestore) {
                                    lastClosedTabs.clear();
                                    lastClosedTabs.addAll(tabs);
                                }
                            });
                });

        // 2.
        // Note: Despite the "undoable=true" setup, incognito tabs won't support undo.
        closeTab(model, tab1, /* undoable= */ true);
        fullList = new Tab[] {tab0, tab2, tab3, tab4};
        checkState(model, fullList, tab4, EMPTY, fullList, tab4);
        assertTrue(tab1.isClosing());
        assertFalse(tab1.isInitialized());
        assertArrayEquals(new Tab[] {tab1}, lastClosedTabs.toArray(new Tab[0]));

        // 3.
        // Note: Despite the "undoable=true" setup, incognito tabs won't support undo.
        closeMultipleTabs(model, Arrays.asList(tab2, tab4), /* undoable= */ true);
        fullList = new Tab[] {tab0, tab3};
        checkState(model, fullList, tab0, EMPTY, fullList, tab0);
        assertTrue(tab2.isClosing());
        assertTrue(tab4.isClosing());
        assertFalse(tab2.isInitialized());
        assertFalse(tab4.isInitialized());
        assertArrayEquals(new Tab[] {tab2, tab4}, lastClosedTabs.toArray(new Tab[0]));

        // 4.
        // Note: Despite the "undoable=true" setup, incognito tabs won't support undo.
        closeAllTabs(model, /* undoable= */ true);
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
        assertTrue(tab0.isClosing());
        assertTrue(tab3.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab3.isInitialized());
        assertArrayEquals(new Tab[] {tab0, tab3}, lastClosedTabs.toArray(new Tab[0]));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testOpenRecentlyClosedTab() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);
        Tab[] allTabs = new Tab[] {tab0, tab1};

        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1}, allTabs, tab0);

        // Ensure tab recovery, and reuse of {@link Tab} objects in Java.
        openMostRecentlyClosedTabOnUiThread(mActivityTestRule.getActivity().getTabModelSelector());
        checkState(model, allTabs, tab1, EMPTY, allTabs, tab1);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testActiveModelCloseAndUndoForTabSupplier() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        assertEquals(
                1,
                (int) ThreadUtils.runOnUiThreadBlocking(() -> model.getTabCountSupplier().get()));
        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));

        final CallbackHelper tabSupplierObserver = new CallbackHelper();
        Callback<Tab> observer = (tab) -> tabSupplierObserver.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.getCurrentTabSupplier().addObserver(observer));

        Tab[] fullList = new Tab[] {tab0};

        assertEquals(
                tab0, ThreadUtils.runOnUiThreadBlocking(() -> model.getCurrentTabSupplier().get()));
        assertEquals(1, tabSupplierObserver.getCallCount());
        checkState(model, new Tab[] {tab0}, tab0, EMPTY, fullList, tab0);
        assertEquals(
                1,
                (int) ThreadUtils.runOnUiThreadBlocking(() -> model.getTabCountSupplier().get()));

        closeTab(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] {tab0}, fullList, tab0);
        assertNull(ThreadUtils.runOnUiThreadBlocking(() -> model.getCurrentTabSupplier().get()));
        assertEquals(2, tabSupplierObserver.getCallCount());
        assertEquals(
                0,
                (int) ThreadUtils.runOnUiThreadBlocking(() -> model.getTabCountSupplier().get()));

        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, EMPTY, fullList, tab0);
        assertEquals(
                tab0, ThreadUtils.runOnUiThreadBlocking(() -> model.getCurrentTabSupplier().get()));
        assertEquals(5, tabSupplierObserver.getCallCount());
        assertEquals(
                1,
                (int) ThreadUtils.runOnUiThreadBlocking(() -> model.getTabCountSupplier().get()));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    @RequiresRestart // This test causes cascading batch failures.
    public void testInactiveModelCloseAndUndoForTabSupplier() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        assertEquals(
                1,
                (int) ThreadUtils.runOnUiThreadBlocking(() -> model.getTabCountSupplier().get()));
        final CallbackHelper tabSupplierObserver = new CallbackHelper();
        Callback<Tab> observer = (tab) -> tabSupplierObserver.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.getCurrentTabSupplier().addSyncObserverAndCallIfNonNull(observer);
                    ((TabModelInternal) model).setActive(false);
                });

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));

        Tab[] fullList = new Tab[] {tab0};

        assertEquals(
                tab0, ThreadUtils.runOnUiThreadBlocking(() -> model.getCurrentTabSupplier().get()));
        assertEquals(1, tabSupplierObserver.getCallCount());
        checkState(model, new Tab[] {tab0}, tab0, EMPTY, fullList, tab0);

        closeTab(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] {tab0}, fullList, tab0);
        assertNull(ThreadUtils.runOnUiThreadBlocking(() -> model.getCurrentTabSupplier().get()));
        assertEquals(2, tabSupplierObserver.getCallCount());
        assertEquals(
                0,
                (int) ThreadUtils.runOnUiThreadBlocking(() -> model.getTabCountSupplier().get()));

        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, EMPTY, fullList, tab0);
        assertEquals(
                tab0, ThreadUtils.runOnUiThreadBlocking(() -> model.getCurrentTabSupplier().get()));
        assertEquals(3, tabSupplierObserver.getCallCount());
        assertEquals(
                1,
                (int) ThreadUtils.runOnUiThreadBlocking(() -> model.getTabCountSupplier().get()));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Closing all tabs closes the window on tablets.
    public void testUndoRunnable() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(isIncognito);

        Tab tab0 = ThreadUtils.runOnUiThreadBlocking(() -> model.getTabAt(0));
        Tab tab1 = createTab(isIncognito);

        final CallbackHelper undoRunnableHelper = new CallbackHelper();
        Runnable undoRunnable = () -> undoRunnableHelper.notifyCalled();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab0)
                                            .allowUndo(true)
                                            .withUndoRunnable(undoRunnable)
                                            .build(),
                                    /* allowDialog= */ false);
                });
        cancelTabClosure(model, tab0);
        assertEquals(1, undoRunnableHelper.getCallCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTabs(List.of(tab0, tab1))
                                            .allowUndo(true)
                                            .withUndoRunnable(undoRunnable)
                                            .build(),
                                    /* allowDialog= */ false);
                });
        cancelTabClosure(model, tab0);
        // Should not increment yet.
        assertEquals(1, undoRunnableHelper.getCallCount());
        cancelTabClosure(model, tab1);
        assertEquals(2, undoRunnableHelper.getCallCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeAllTabs()
                                            .allowUndo(true)
                                            .withUndoRunnable(undoRunnable)
                                            .build(),
                                    /* allowDialog= */ false);
                });
        cancelTabClosure(model, tab0);
        // Should not increment yet.
        assertEquals(2, undoRunnableHelper.getCallCount());
        cancelTabClosure(model, tab1);
        assertEquals(3, undoRunnableHelper.getCallCount());
    }
}
