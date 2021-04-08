// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.os.Build;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests undo and restoring of tabs in a {@link TabModel}.
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
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private static final Tab[] EMPTY = new Tab[] { };
    private static final String TEST_URL_0 = UrlUtils.encodeHtmlDataUri("<html>test_url_0.</html>");
    private static final String TEST_URL_1 = UrlUtils.encodeHtmlDataUri("<html>test_url_1.</html>");

    private void checkState(
            final TabModel model, final Tab[] tabsList, final Tab selectedTab,
            final Tab[] closingTabs, final Tab[] fullTabsList,
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tabCreator.createNewTab(
                    new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
        });
    }

    private void selectTabOnUiThread(final TabModel model, final Tab tab) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { model.setIndex(model.indexOf(tab), TabSelectionType.FROM_USER); });
    }

    private void closeTabOnUiThread(final TabModel model, final Tab tab, final boolean undoable)
            throws TimeoutException {
        // Check preconditions.
        Assert.assertFalse(tab.isClosing());
        Assert.assertTrue(tab.isInitialized());
        Assert.assertFalse(model.isClosurePending(tab.getId()));
        Assert.assertNotNull(TabModelUtils.getTabById(model, tab.getId()));

        final CallbackHelper didReceivePendingClosureHelper = new CallbackHelper();
        model.addObserver(new TabModelObserver() {
            @Override
            public void tabPendingClosure(Tab tab) {
                didReceivePendingClosureHelper.notifyCalled();
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Take action.
            model.closeTab(tab, true, false, undoable);
        });

        boolean didUndo = undoable && model.supportsPendingClosures();

        // Make sure the TabModel throws a tabPendingClosure callback if necessary.
        if (didUndo) didReceivePendingClosureHelper.waitForCallback(0);

        // Check post conditions
        Assert.assertEquals(didUndo, model.isClosurePending(tab.getId()));
        Assert.assertNull(TabModelUtils.getTabById(model, tab.getId()));
        Assert.assertTrue(tab.isClosing());
        Assert.assertEquals(didUndo, tab.isInitialized());
    }

    private void closeAllTabsOnUiThread(final TabModel model) {
        TestThreadUtils.runOnUiThreadBlocking(() -> { model.closeAllTabs(); });
    }

    private void moveTabOnUiThread(final TabModel model, final Tab tab, final int newIndex) {
        TestThreadUtils.runOnUiThreadBlocking(() -> { model.moveTab(tab.getId(), newIndex); });
    }

    private void cancelTabClosureOnUiThread(final TabModel model, final Tab tab)
            throws TimeoutException {
        // Check preconditions.
        Assert.assertTrue(tab.isClosing());
        Assert.assertTrue(tab.isInitialized());
        Assert.assertTrue(model.isClosurePending(tab.getId()));
        Assert.assertNull(TabModelUtils.getTabById(model, tab.getId()));

        final CallbackHelper didReceiveClosureCancelledHelper = new CallbackHelper();
        model.addObserver(new TabModelObserver() {
            @Override
            public void tabClosureUndone(Tab tab) {
                didReceiveClosureCancelledHelper.notifyCalled();
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Take action.
            model.cancelTabClosure(tab.getId());
        });

        // Make sure the TabModel throws a tabClosureUndone.
        didReceiveClosureCancelledHelper.waitForCallback(0);

        // Check post conditions.
        Assert.assertFalse(model.isClosurePending(tab.getId()));
        Assert.assertNotNull(TabModelUtils.getTabById(model, tab.getId()));
        Assert.assertFalse(tab.isClosing());
        Assert.assertTrue(tab.isInitialized());
    }

    private void cancelAllTabClosuresOnUiThread(final TabModel model, final Tab[] expectedToClose)
            throws TimeoutException {
        final CallbackHelper tabClosureUndoneHelper = new CallbackHelper();

        for (int i = 0; i < expectedToClose.length; i++) {
            Tab tab = expectedToClose[i];
            Assert.assertTrue(tab.isClosing());
            Assert.assertTrue(tab.isInitialized());
            Assert.assertTrue(model.isClosurePending(tab.getId()));
            Assert.assertNull(TabModelUtils.getTabById(model, tab.getId()));

            // Make sure that this TabModel throws the right events.
            model.addObserver(new TabModelObserver() {
                @Override
                public void tabClosureUndone(Tab currentTab) {
                    tabClosureUndoneHelper.notifyCalled();
                }
            });
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < expectedToClose.length; i++) {
                Tab tab = expectedToClose[i];
                model.cancelTabClosure(tab.getId());
            }
        });

        tabClosureUndoneHelper.waitForCallback(0, expectedToClose.length);

        for (int i = 0; i < expectedToClose.length; i++) {
            final Tab tab = expectedToClose[i];
            Assert.assertFalse(model.isClosurePending(tab.getId()));
            Assert.assertNotNull(TabModelUtils.getTabById(model, tab.getId()));
            Assert.assertFalse(tab.isClosing());
            Assert.assertTrue(tab.isInitialized());
        }
    }

    private void commitTabClosureOnUiThread(final TabModel model, final Tab tab)
            throws TimeoutException {
        // Check preconditions.
        Assert.assertTrue(tab.isClosing());
        Assert.assertTrue(tab.isInitialized());
        Assert.assertTrue(model.isClosurePending(tab.getId()));
        Assert.assertNull(TabModelUtils.getTabById(model, tab.getId()));

        final CallbackHelper didReceiveClosureCommittedHelper = new CallbackHelper();
        model.addObserver(new TabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                didReceiveClosureCommittedHelper.notifyCalled();
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Take action.
            model.commitTabClosure(tab.getId());
        });

        // Make sure the TabModel throws a tabClosureCommitted.
        didReceiveClosureCommittedHelper.waitForCallback(0);

        // Check post conditions
        Assert.assertFalse(model.isClosurePending(tab.getId()));
        Assert.assertNull(TabModelUtils.getTabById(model, tab.getId()));
        Assert.assertTrue(tab.isClosing());
        Assert.assertFalse(tab.isInitialized());
    }

    private void commitAllTabClosuresOnUiThread(final TabModel model, Tab[] expectedToClose)
            throws TimeoutException {
        final CallbackHelper tabClosureCommittedHelper = new CallbackHelper();

        for (int i = 0; i < expectedToClose.length; i++) {
            Tab tab = expectedToClose[i];
            Assert.assertTrue(tab.isClosing());
            Assert.assertTrue(tab.isInitialized());
            Assert.assertTrue(model.isClosurePending(tab.getId()));

            // Make sure that this TabModel throws the right events.
            model.addObserver(new TabModelObserver() {
                @Override
                public void tabClosureCommitted(Tab currentTab) {
                    tabClosureCommittedHelper.notifyCalled();
                }
            });
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> { model.commitAllTabClosures(); });

        tabClosureCommittedHelper.waitForCallback(0, expectedToClose.length);
        for (int i = 0; i < expectedToClose.length; i++) {
            final Tab tab = expectedToClose[i];
            Assert.assertTrue(tab.isClosing());
            Assert.assertFalse(tab.isInitialized());
            Assert.assertFalse(model.isClosurePending(tab.getId()));
        }
    }

    private void saveStateOnUiThread(final TabModelOrchestrator orchestrator) {
        TestThreadUtils.runOnUiThreadBlocking(() -> { orchestrator.saveState(); });

        TabModelSelector selector = orchestrator.getTabModelSelector();
        for (int i = 0; i < selector.getModels().size(); i++) {
            TabList tabs = selector.getModels().get(i).getComprehensiveModel();
            for (int j = 0; j < tabs.getCount(); j++) {
                Assert.assertFalse(tabs.isClosurePending(tabs.getTabAt(j).getId()));
            }
        }
    }

    private void openMostRecentlyClosedTabOnUiThread(final TabModelSelector selector) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { selector.getCurrentModel().openMostRecentlyClosedTab(); });
    }

    // Helper class that notifies after the tab is closed, and a tab restore service entry has been
    // created in tab restore service.
    private static class TabClosedObserver implements TabModelObserver {
        private CallbackHelper mTabClosedCallback;

        public TabClosedObserver(CallbackHelper closedCallback) {
            mTabClosedCallback = closedCallback;
        }

        @Override
        public void didCloseTab(int tabId, boolean incognito) {
            mTabClosedCallback.notifyCalled();
        }
    }

    /**
     * Test undo with a single tab with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0s ]             -                 [ 0s ]
     * 2.  CloseTab(0, allow undo)    -                  [ 0 ]             [ 0s ]
     * 3.  CancelClose(0)             [ 0s ]             -                 [ 0s ]
     * 4.  CloseTab(0, allow undo)    -                  [ 0 ]             [ 0s ]
     * 5.  CommitClose(0)             -                  -                 -
     * 6.  CreateTab(0)               [ 0s ]             -                 [ 0s ]
     * 7.  CloseTab(0, allow undo)    -                  [ 0 ]             [ 0s ]
     * 8.  CommitAllClose             -                  -                 -
     * 9.  CreateTab(0)               [ 0s ]             -                 [ 0s ]
     * 10. CloseTab(0, disallow undo) -                  -                 -
     *
     */
    @Test
    @MediumTest
    public void testSingleTab() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));

        Tab tab0 = model.getTabAt(0);

        Tab[] fullList = new Tab[] { tab0 };

        // 1.
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0 }, fullList, tab0);

        // 3.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);

        // 4.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0 }, fullList, tab0);

        // 5.
        commitTabClosureOnUiThread(model, tab0);
        fullList = EMPTY;
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);

        // 6.
        createTabOnUiThread(tabCreator);
        tab0 = model.getTabAt(0);
        fullList = new Tab[] { tab0 };
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);

        // 7.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0 }, fullList, tab0);

        // 8.
        commitAllTabClosuresOnUiThread(model, new Tab[] { tab0 });
        fullList = EMPTY;
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);

        // 9.
        createTabOnUiThread(tabCreator);
        tab0 = model.getTabAt(0);
        fullList = new Tab[] { tab0 };
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);

        // 10.
        closeTabOnUiThread(model, tab0, false);
        fullList = EMPTY;
        checkState(model, EMPTY, null, EMPTY, fullList, null);
        Assert.assertTrue(tab0.isClosing());
        Assert.assertFalse(tab0.isInitialized());
    }

    /**
     * Test undo with two tabs with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1s ]           -                 [ 0 1s ]
     * 2.  CloseTab(0, allow undo)    [ 1s ]             [ 0 ]             [ 0 1s ]
     * 3.  CancelClose(0)             [ 0 1s ]           -                 [ 0 1s ]
     * 4.  CloseTab(0, allow undo)    [ 1s ]             [ 0 ]             [ 0 1s ]
     * 5.  CloseTab(1, allow undo)    -                  [ 1 0 ]           [ 0s 1 ]
     * 6.  CancelClose(1)             [ 1s ]             [ 0 ]             [ 0 1s ]
     * 7.  CancelClose(0)             [ 0 1s ]           -                 [ 0 1s ]
     * 8.  CloseTab(1, allow undo)    [ 0s ]             [ 1 ]             [ 0s 1 ]
     * 9.  CloseTab(0, allow undo)    -                  [ 0 1 ]           [ 0s 1 ]
     * 10. CancelClose(1)             [ 1s ]             [ 0 ]             [ 0 1s ]
     * 11. CancelClose(0)             [ 0 1s ]           -                 [ 0 1s ]
     * 12. CloseTab(1, allow undo)    [ 0s ]             [ 1 ]             [ 0s 1 ]
     * 13. CloseTab(0, allow undo)    -                  [ 0 1 ]           [ 0s 1 ]
     * 14. CancelClose(0)             [ 0s ]             [ 1 ]             [ 0s 1 ]
     * 15. CloseTab(0, allow undo)    -                  [ 0 1 ]           [ 0s 1 ]
     * 16. CancelClose(0)             [ 0s ]             [ 1 ]             [ 0s 1 ]
     * 17. CancelClose(1)             [ 0s 1 ]           -                 [ 0s 1 ]
     * 18. CloseTab(0, disallow undo) [ 1s ]             -                 [ 1s ]
     * 19. CreateTab(0)               [ 1 0s ]           -                 [ 1 0s ]
     * 20. CloseTab(0, allow undo)    [ 1s ]             [ 0 ]             [ 1s 0 ]
     * 21. CommitClose(0)             [ 1s ]             -                 [ 1s ]
     * 22. CreateTab(0)               [ 1 0s ]           -                 [ 1 0s ]
     * 23. CloseTab(0, allow undo)    [ 1s ]             [ 0 ]             [ 1s 0 ]
     * 24. CloseTab(1, allow undo)    -                  [ 1 0 ]           [ 1s 0 ]
     * 25. CommitAllClose             -                  -                 -
     *
     */
    @Test
    // @MediumTest
    @DisabledTest(
            message = "Flaky on all Android configurations except Swarming.  See crbug.com/620014.")
    public void
    testTwoTabs() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);

        Tab[] fullList = new Tab[] { tab0, tab1 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1 }, tab1, EMPTY, fullList, tab1);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 3.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1 }, tab1, EMPTY, fullList, tab1);

        // 4.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 5.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1 }, fullList, tab0);

        // 6.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 7.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1 }, tab1, EMPTY, fullList, tab1);

        // 8.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1 }, fullList, tab0);

        // 9.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1 }, fullList, tab0);

        // 10.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 11.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1 }, tab1, EMPTY, fullList, tab1);

        // 12.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1 }, fullList, tab0);

        // 13.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1 }, fullList, tab0);

        // 14.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1 }, fullList, tab0);

        // 15.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1 }, fullList, tab0);

        // 16.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1 }, fullList, tab0);

        // 17.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab0, tab1 }, tab0, EMPTY, fullList, tab0);

        // 18.
        closeTabOnUiThread(model, tab0, false);
        fullList = new Tab[] { tab1 };
        checkState(model, new Tab[] { tab1 }, tab1, EMPTY, fullList, tab1);

        // 19.
        createTabOnUiThread(tabCreator);
        tab0 = model.getTabAt(1);
        fullList = new Tab[] { tab1, tab0 };
        checkState(model, new Tab[] { tab1, tab0 }, tab0, EMPTY, fullList, tab0);

        // 20.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 21.
        commitTabClosureOnUiThread(model, tab0);
        fullList = new Tab[] { tab1 };
        checkState(model, new Tab[] { tab1 }, tab1, EMPTY, fullList, tab1);

        // 22.
        createTabOnUiThread(tabCreator);
        tab0 = model.getTabAt(1);
        fullList = new Tab[] { tab1, tab0 };
        checkState(model, new Tab[] { tab1, tab0 }, tab0, EMPTY, fullList, tab0);

        // 23.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 24.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, EMPTY, null, new Tab[] { tab1, tab0 }, fullList, tab1);

        // 25.
        commitAllTabClosuresOnUiThread(model, new Tab[] { tab1, tab0 });
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
    }

    /**
     * Test restoring in the same order of closing with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(0, allow undo)    [ 1 2 3s ]         [ 0 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(1, allow undo)    [ 2 3s ]           [ 1 0 ]           [ 0 1 2 3s ]
     * 4.  CloseTab(2, allow undo)    [ 3s ]             [ 2 1 0 ]         [ 0 1 2 3s ]
     * 5.  CloseTab(3, allow undo)    -                  [ 3 2 1 0 ]       [ 0s 1 2 3 ]
     * 6.  CancelClose(3)             [ 3s ]             [ 2 1 0 ]         [ 0 1 2 3s ]
     * 7.  CancelClose(2)             [ 2 3s ]           [ 1 0 ]           [ 0 1 2 3s ]
     * 8.  CancelClose(1)             [ 1 2 3s ]         [ 0 ]             [ 0 1 2 3s ]
     * 9.  CancelClose(0)             [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 10. SelectTab(3)               [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 11. CloseTab(3, allow undo)    [ 0 1 2s ]         [ 3 ]             [ 0 1 2s 3 ]
     * 12. CloseTab(2, allow undo)    [ 0 1s ]           [ 2 3 ]           [ 0 1s 2 3 ]
     * 13. CloseTab(1, allow undo)    [ 0s ]             [ 1 2 3 ]         [ 0s 1 2 3 ]
     * 14. CloseTab(0, allow undo)    -                  [ 0 1 2 3 ]       [ 0s 1 2 3 ]
     * 15. CancelClose(0)             [ 0s ]             [ 1 2 3 ]         [ 0s 1 2 3 ]
     * 16. CancelClose(1)             [ 0s 1 ]           [ 2 3 ]           [ 0s 1 2 3 ]
     * 17. CancelClose(2)             [ 0s 1 2 ]         [ 3 ]             [ 0s 1 2 3 ]
     * 18. CancelClose(3)             [ 0s 1 2 3 ]       -                 [ 0s 1 2 3 ]
     * 19. CloseTab(2, allow undo)    [ 0s 1 3 ]         [ 2 ]             [ 0s 1 2 3 ]
     * 20. CloseTab(0, allow undo)    [ 1s 3 ]           [ 0 2 ]           [ 0 1s 2 3 ]
     * 21. CloseTab(3, allow undo)    [ 1s ]             [ 3 0 2 ]         [ 0 1s 2 3 ]
     * 22. CancelClose(3)             [ 1s 3 ]           [ 0 2 ]           [ 0 1s 2 3 ]
     * 23. CancelClose(0)             [ 0 1s 3 ]         [ 2 ]             [ 0 1s 2 3 ]
     * 24. CancelClose(2)             [ 0 1s 2 3 ]       -                 [ 0 1s 2 3 ]
     *
     */
    @Test
    @MediumTest
    @FlakyTest(message = "crbug.com/1184155")
    public void testInOrderRestore() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        final Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab3, new Tab[] { tab0 },
                fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab2, tab3 }, tab3, new Tab[] { tab1, tab0 },
                fullList, tab3);

        // 4.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab2, tab1, tab0 },
                fullList, tab3);

        // 5.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, EMPTY, null, new Tab[] { tab3, tab2, tab1, tab0 }, fullList, tab0);

        // 6.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab2, tab1, tab0 },
                fullList, tab3);

        // 7.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab2, tab3 }, tab3, new Tab[] { tab1, tab0 },
                fullList, tab3);

        // 8.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab3, new Tab[] { tab0 },
                fullList, tab3);

        // 9.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 10.
        selectTabOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 11.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab2, new Tab[] { tab3 },
                fullList, tab2);

        // 12.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab1 }, tab1, new Tab[] { tab2, tab3 },
                fullList, tab1);

        // 13.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1, tab2, tab3 },
                fullList, tab0);

        // 14.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1, tab2, tab3 }, fullList, tab0);

        // 15.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1, tab2, tab3 },
                fullList, tab0);

        // 16.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab0, tab1 }, tab0, new Tab[] { tab2, tab3 },
                fullList, tab0);

        // 17.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab0, new Tab[] { tab3 },
                fullList, tab0);

        // 18.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab0, EMPTY, fullList, tab0);

        // 19.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab1, tab3 }, tab0, new Tab[] { tab2 },
                fullList, tab0);

        // 20.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1, tab3 }, tab1, new Tab[] { tab0, tab2 },
                fullList, tab1);

        // 21.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab3, tab0, tab2 },
                fullList, tab1);

        // 22.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab1, tab3 }, tab1, new Tab[] { tab0, tab2 },
                fullList, tab1);

        // 23.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1, tab3 }, tab1, new Tab[] { tab2 },
                fullList, tab1);

        // 24.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab1, EMPTY, fullList, tab1);
    }

    /**
     * Test restoring in the reverse of closing with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(0, allow undo)    [ 1 2 3s ]         [ 0 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(1, allow undo)    [ 2 3s ]           [ 1 0 ]           [ 0 1 2 3s ]
     * 4.  CloseTab(2, allow undo)    [ 3s ]             [ 2 1 0 ]         [ 0 1 2 3s ]
     * 5.  CloseTab(3, allow undo)    -                  [ 3 2 1 0 ]       [ 0s 1 2 3 ]
     * 6.  CancelClose(0)             [ 0s ]             [ 3 2 1 ]         [ 0s 1 2 3 ]
     * 7.  CancelClose(1)             [ 0s 1 ]           [ 3 2 ]           [ 0s 1 2 3 ]
     * 8.  CancelClose(2)             [ 0s 1 2 ]         [ 3 ]             [ 0s 1 2 3 ]
     * 9.  CancelClose(3)             [ 0s 1 2 3 ]       -                 [ 0s 1 2 3 ]
     * 10. CloseTab(3, allow undo)    [ 0s 1 2 ]         [ 3 ]             [ 0s 1 2 3 ]
     * 11. CloseTab(2, allow undo)    [ 0s 1 ]           [ 2 3 ]           [ 0s 1 2 3 ]
     * 12. CloseTab(1, allow undo)    [ 0s ]             [ 1 2 3 ]         [ 0s 1 2 3 ]
     * 13. CloseTab(0, allow undo)    -                  [ 0 1 2 3 ]       [ 0s 1 2 3 ]
     * 14. CancelClose(3)             [ 3s ]             [ 0 1 2 ]         [ 0 1 2 3s ]
     * 15. CancelClose(2)             [ 2 3s ]           [ 0 1 ]           [ 0 1 2 3s ]
     * 16. CancelClose(1)             [ 1 2 3s ]         [ 0 ]             [ 0 1 2 3s ]
     * 17. CancelClose(0)             [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 18. SelectTab(3)               [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 19. CloseTab(2, allow undo)    [ 0 1 3s ]         [ 2 ]             [ 0 1 2 3s ]
     * 20. CloseTab(0, allow undo)    [ 1 3s ]           [ 0 2 ]           [ 0 1 2 3s ]
     * 21. CloseTab(3, allow undo)    [ 1s ]             [ 3 0 2 ]         [ 0 1s 2 3 ]
     * 22. CancelClose(2)             [ 1s 2 ]           [ 3 0 ]           [ 0 1s 2 3 ]
     * 23. CancelClose(0)             [ 0 1s 2 ]         [ 3 ]             [ 0 1s 2 3 ]
     * 24. CancelClose(3)             [ 0 1s 2 3 ]       -                 [ 0 1s 2 3 ]
     *
     */
    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // See crbug.com/633607
    public void testReverseOrderRestore() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab3, new Tab[] { tab0 },
                fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab2, tab3 }, tab3, new Tab[] { tab1, tab0 },
                fullList, tab3);

        // 4.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab2, tab1, tab0 },
                fullList, tab3);

        // 5.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, EMPTY, null, new Tab[] { tab3, tab2, tab1, tab0 }, fullList, tab0);

        // 6.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab3, tab2, tab1 },
                fullList, tab0);

        // 7.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab0, tab1 }, tab0, new Tab[] { tab3, tab2 },
                fullList, tab0);

        // 8.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab0, new Tab[] { tab3 },
                fullList, tab0);

        // 9.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab0, EMPTY, fullList, tab0);

        // 10.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab0, new Tab[] { tab3 },
                fullList, tab0);

        // 11.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab1 }, tab0, new Tab[] { tab2, tab3 },
                fullList, tab0);

        // 12.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1, tab2, tab3 },
                fullList, tab0);

        // 13.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1, tab2, tab3 }, fullList, tab0);

        // 14.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab0, tab1, tab2 },
                fullList, tab3);

        // 15.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab2, tab3 }, tab3, new Tab[] { tab0, tab1 },
                fullList, tab3);

        // 16.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab3, new Tab[] { tab0 },
                fullList, tab3);

        // 17.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 18.
        selectTabOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 19.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab1, tab3 }, tab3, new Tab[] { tab2 },
                fullList, tab3);

        // 20.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1, tab3 }, tab3, new Tab[] { tab0, tab2 },
                fullList, tab3);

        // 21.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab3, tab0, tab2 },
                fullList, tab1);

        // 22.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab1, tab2 }, tab1, new Tab[] { tab3, tab0 },
                fullList, tab1);

        // 23.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab1, new Tab[] { tab3 },
                fullList, tab1);

        // 24.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab1, EMPTY, fullList, tab1);
    }

    /**
     * Test restoring out of order with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(0, allow undo)    [ 1 2 3s ]         [ 0 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(1, allow undo)    [ 2 3s ]           [ 1 0 ]           [ 0 1 2 3s ]
     * 4.  CloseTab(2, allow undo)    [ 3s ]             [ 2 1 0 ]         [ 0 1 2 3s ]
     * 5.  CloseTab(3, allow undo)    -                  [ 3 2 1 0 ]       [ 0s 1 2 3 ]
     * 6.  CancelClose(2)             [ 2s ]             [ 3 1 0 ]         [ 0 1 2s 3 ]
     * 7.  CancelClose(1)             [ 1 2s ]           [ 3 0 ]           [ 0 1 2s 3 ]
     * 8.  CancelClose(3)             [ 1 2s 3 ]         [ 0 ]             [ 0 1 2s 3 ]
     * 9.  CancelClose(0)             [ 0 1 2s 3 ]       -                 [ 0 1 2s 3 ]
     * 10. CloseTab(1, allow undo)    [ 0 2s 3 ]         [ 1 ]             [ 0 1 2s 3 ]
     * 11. CancelClose(1)             [ 0 1 2s 3 ]       -                 [ 0 1 2s 3 ]
     * 12. CloseTab(3, disallow undo) [ 0 1 2s ]         -                 [ 0 1 2s ]
     * 13. CloseTab(1, allow undo)    [ 0 2s ]           [ 1 ]             [ 0 1 2s ]
     * 14. CloseTab(0, allow undo)    [ 2s ]             [ 0 1 ]           [ 0 1 2s ]
     * 15. CommitClose(0)             [ 2s ]             [ 1 ]             [ 1 2s ]
     * 16. CancelClose(1)             [ 1 2s ]           -                 [ 1 2s ]
     * 17. CloseTab(2, disallow undo) [ 1s ]             -                 [ 1s ]
     *
     */
    @Test
    @MediumTest
    public void testOutOfOrder1() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab3, new Tab[] { tab0 },
                fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab2, tab3 }, tab3, new Tab[] { tab1, tab0 },
                fullList, tab3);

        // 4.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab2, tab1, tab0 },
                fullList, tab3);

        // 5.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, EMPTY, null, new Tab[] { tab3, tab2, tab1, tab0 }, fullList, tab0);

        // 6.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab2 }, tab2, new Tab[] { tab3, tab1, tab0 },
                fullList, tab2);

        // 7.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1, tab2 }, tab2, new Tab[] { tab3, tab0 },
                fullList, tab2);

        // 8.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab2, new Tab[] { tab0 },
                fullList, tab2);

        // 9.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab2, EMPTY, fullList, tab2);

        // 10.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab2, new Tab[] { tab1 },
                fullList, tab2);

        // 11.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab2, EMPTY, fullList, tab2);

        // 12.
        closeTabOnUiThread(model, tab3, false);
        fullList = new Tab[] { tab0, tab1, tab2 };
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab2, EMPTY, fullList, tab2);

        // 13.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2 }, tab2, new Tab[] { tab1 }, fullList,
                tab2);

        // 14.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab2 }, tab2, new Tab[] { tab0, tab1 }, fullList,
                tab2);

        // 15.
        commitTabClosureOnUiThread(model, tab0);
        fullList = new Tab[] { tab1, tab2 };
        checkState(model, new Tab[] { tab2 }, tab2, new Tab[] { tab1 }, fullList, tab2);

        // 16.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1, tab2 }, tab2, EMPTY, fullList, tab2);

        // 17.
        closeTabOnUiThread(model, tab2, false);
        fullList = new Tab[] { tab1 };
        checkState(model, new Tab[] { tab1 },  tab1, EMPTY, fullList, tab1);
    }

    /**
     * Test restoring out of order with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         [ 1 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(3, allow undo)    [ 0 2s ]           [ 3 1 ]           [ 0 1 2s 3 ]
     * 4.  CancelClose(1)             [ 0 1 2s ]         [ 3 ]             [ 0 1 2s 3 ]
     * 5.  CloseTab(2, allow undo)    [ 0 1s ]           [ 2 3 ]           [ 0 1s 2 3 ]
     * 6.  CloseTab(0, allow undo)    [ 1s ]             [ 0 2 3 ]         [ 0 1s 2 3 ]
     * 7.  CommitClose(0)             [ 1s ]             [ 2 3 ]           [ 1s 2 3 ]
     * 8.  CancelClose(3)             [ 1s 3 ]           [ 2 ]             [ 1s 2 3 ]
     * 9.  CloseTab(1, allow undo)    [ 3s ]             [ 1 2 ]           [ 1 2 3s ]
     * 10. CommitClose(2)             [ 3s ]             [ 1 ]             [ 1 3s ]
     * 11. CancelClose(1)             [ 1 3s ]           -                 [ 1 3s ]
     * 12. CloseTab(3, allow undo)    [ 1s ]             [ 3 ]             [ 1s 3 ]
     * 13. CloseTab(1, allow undo)    -                  [ 1 3 ]           [ 1s 3 ]
     * 14. CommitAll                  -                  -                 -
     *
     */
    @Test
    @MediumTest
    @FlakyTest(message = "crbug.com/592969")
    public void testOutOfOrder2() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, new Tab[] { tab1 },
                fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab0, tab2 }, tab2, new Tab[] { tab3, tab1 },
                fullList, tab2);

        // 4.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab2, new Tab[] { tab3 },
                fullList, tab2);

        // 5.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab1 }, tab1, new Tab[] { tab2, tab3 },
                fullList, tab1);

        // 6.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0, tab2, tab3 },
                fullList, tab1);

        // 7.
        commitTabClosureOnUiThread(model, tab0);
        fullList = new Tab[] { tab1, tab2, tab3 };
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab2, tab3 }, fullList,
                tab1);

        // 8.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab1, tab3 }, tab1, new Tab[] { tab2 }, fullList,
                tab1);

        // 9.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab1, tab2 }, fullList,
                tab3);

        // 10.
        commitTabClosureOnUiThread(model, tab2);
        fullList = new Tab[] { tab1, tab3 };
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab1 }, fullList, tab3);

        // 11.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1, tab3 }, tab3, EMPTY, fullList, tab3);

        // 12.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab3 }, fullList, tab1);

        // 13.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, EMPTY, null, new Tab[] { tab1, tab3 }, fullList, tab1);

        // 14.
        commitAllTabClosuresOnUiThread(model, new Tab[] { tab1, tab3 });
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
    }

    /**
     * Test undo {@link TabModel#closeAllTabs()} with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0  1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         [ 1 ]             [ 0  1 2 3s ]
     * 3.  CloseTab(2, allow undo)    [ 0 3s ]           [ 2 1 ]           [ 0  1 2 3s ]
     * 4.  CloseAll                   -                  [ 0 3 2 1 ]       [ 0s 1 2 3  ]
     * 5.  CancelAllClose             [ 0 1 2 3s ]       -                 [ 0  1 2 3s ]
     * 6.  CloseAll                   -                  [ 0 1 2 3 ]       [ 0s 1 2 3  ]
     * 7.  CommitAllClose             -                  -                 -
     * 8.  CreateTab(0)               [ 0s ]             -                 [ 0s ]
     * 9.  CloseAll                   -                  [ 0 ]             [ 0s ]
     *
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/633607")
    public void testCloseAll() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, fullList, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, new Tab[] { tab1 }, fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab3 }, tab3, new Tab[] { tab1, tab2 }, fullList, tab3);

        // 4.
        closeAllTabsOnUiThread(model);
        checkState(model, EMPTY, null, fullList, fullList, tab0);

        // 5.
        cancelAllTabClosuresOnUiThread(model, fullList);
        checkState(model, fullList, tab0, EMPTY, fullList, tab0);

        // 6.
        closeAllTabsOnUiThread(model);
        checkState(model, EMPTY, null, fullList, fullList, tab0);

        // 7.
        commitAllTabClosuresOnUiThread(model, fullList);
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
        Assert.assertTrue(tab0.isClosing());
        Assert.assertTrue(tab1.isClosing());
        Assert.assertTrue(tab2.isClosing());
        Assert.assertTrue(tab3.isClosing());
        Assert.assertFalse(tab0.isInitialized());
        Assert.assertFalse(tab1.isInitialized());
        Assert.assertFalse(tab2.isInitialized());
        Assert.assertFalse(tab3.isInitialized());

        // 8.
        createTabOnUiThread(tabCreator);
        tab0 = model.getTabAt(0);
        fullList = new Tab[] { tab0 };
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);

        // 9.
        closeAllTabsOnUiThread(model);
        checkState(model, EMPTY, null, fullList, fullList, tab0);
        Assert.assertTrue(tab0.isClosing());
        Assert.assertTrue(tab0.isInitialized());
    }

    /**
     * Test {@link TabModel#closeTab(Tab)} when not allowing a close commits all pending
     * closes:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         [ 1 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(2, allow undo)    [ 0 3s ]           [ 2 1 ]           [ 0 1 2 3s ]
     * 4.  CloseTab(3, disallow undo) [ 0s ]             -                 [ 0s ]
     *
     *
     * TODO(crbug.com/1165954) Investigate and resolve failure on testCloseTab when batching.
     * RequiresRestart is used as a workaround.
     */
    @Test
    @MediumTest
    @RequiresRestart
    public void testCloseTab() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab3 }, tab3, EMPTY, fullList, tab3);

        // 4.
        closeTabOnUiThread(model, tab3, false);
        fullList = new Tab[] { tab0 };
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);
        Assert.assertTrue(tab1.isClosing());
        Assert.assertTrue(tab2.isClosing());
        Assert.assertFalse(tab1.isInitialized());
        Assert.assertFalse(tab2.isInitialized());
    }

    /**
     * Test {@link TabModel#moveTab(int, int)} commits all pending closes:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         [ 1 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(2, allow undo)    [ 0 3s ]           [ 2 1 ]           [ 0 1 2 3s ]
     * 4.  MoveTab(0, 2)              [ 3s 0 ]           -                 [ 3s 0 ]
     *
     */
    @Test
    @MediumTest
    @DisabledTest
    public void testMoveTab() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab3 }, tab3, EMPTY, fullList, tab3);

        // 4.
        moveTabOnUiThread(model, tab0, 2);
        fullList = new Tab[] { tab3, tab0 };
        checkState(model, new Tab[] { tab3, tab0 }, tab3, EMPTY, fullList, tab3);
        Assert.assertTrue(tab1.isClosing());
        Assert.assertTrue(tab2.isClosing());
        Assert.assertFalse(tab1.isInitialized());
        Assert.assertFalse(tab1.isInitialized());
    }

    /**
     * Test adding a {@link Tab} to a {@link TabModel} commits all pending closes:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         [ 1 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(2, allow undo)    [ 0 3s ]           [ 2 1 ]           [ 0 1 2 3s ]
     * 4.  CreateTab(4)               [ 0 3 4s ]         -                 [ 0 3 4s ]
     * 5.  CloseTab(0, allow undo)    [ 3 4s ]           [ 0 ]             [ 0 3 4s ]
     * 6.  CloseTab(3, allow undo)    [ 4s ]             [ 3 0 ]           [ 0 3 4s ]
     * 7.  CloseTab(4, allow undo)    -                  [ 4 3 0 ]         [ 0s 3 4 ]
     * 8.  CreateTab(5)               [ 5s ]             -                 [ 5s ]
     */
    //@MediumTest
    //@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // See crbug.com/633607
    // Disabled due to flakiness on linux_android_rel_ng (crbug.com/661429)
    @Test
    @DisabledTest
    public void testAddTab() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab3 }, tab3, EMPTY, fullList, tab3);

        // 4.
        createTabOnUiThread(tabCreator);
        Tab tab4 = model.getTabAt(2);
        fullList = new Tab[] { tab0, tab3, tab4 };
        checkState(model, new Tab[] { tab0, tab3, tab4 }, tab4, EMPTY, fullList, tab4);
        Assert.assertTrue(tab1.isClosing());
        Assert.assertTrue(tab2.isClosing());
        Assert.assertFalse(tab1.isInitialized());
        Assert.assertFalse(tab2.isInitialized());

        // 5.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab3, tab4 }, tab4, new Tab[] { tab0 }, fullList,
                tab4);

        // 6.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab4 }, tab4, new Tab[] { tab3, tab0 }, fullList,
                tab4);

        // 7.
        closeTabOnUiThread(model, tab4, true);
        checkState(model, EMPTY, null, new Tab[] { tab4, tab3, tab0 }, fullList, tab0);

        // 8.
        createTabOnUiThread(tabCreator);
        Tab tab5 = model.getTabAt(0);
        fullList = new Tab[] { tab5 };
        checkState(model, new Tab[] { tab5 }, tab5, EMPTY, fullList, tab5);
        Assert.assertTrue(tab0.isClosing());
        Assert.assertTrue(tab3.isClosing());
        Assert.assertTrue(tab4.isClosing());
        Assert.assertFalse(tab0.isInitialized());
        Assert.assertFalse(tab3.isInitialized());
        Assert.assertFalse(tab4.isInitialized());
    }

    /**
     * Test a {@link TabModel} where undo is not supported:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         -                 [ 0 2 3s ]
     * 3.  CloseAll                   -                  -                 -
     *
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1042168")
    public void testUndoNotSupported() throws TimeoutException {
        TabModel model = sActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        ChromeTabCreator tabCreator = sActivityTestRule.getActivity().getTabCreator(true);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);
        Assert.assertFalse(model.supportsPendingClosures());

        // 2.
        closeTabOnUiThread(model, tab1, true);
        fullList = new Tab[] { tab0, tab2, tab3 };
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, EMPTY, fullList, tab3);
        Assert.assertTrue(tab1.isClosing());
        Assert.assertFalse(tab1.isInitialized());

        // 3.
        closeAllTabsOnUiThread(model);
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
        Assert.assertTrue(tab0.isClosing());
        Assert.assertTrue(tab2.isClosing());
        Assert.assertTrue(tab3.isClosing());
        Assert.assertFalse(tab0.isInitialized());
        Assert.assertFalse(tab2.isInitialized());
        Assert.assertFalse(tab3.isInitialized());
    }

    /**
     * Test calling {@link TabModelOrchestrator#saveState()} commits all pending closures:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1s ]           -                 [ 0 1s ]
     * 2.  CloseTab(0, allow undo)    [ 1s ]             [ 0 ]             [ 0 1s ]
     * 3.  SaveState                  [ 1s ]             -                 [ 1s ]
     */
    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // See crbug.com/633607
    public void testSaveStateCommitsUndos() throws TimeoutException, ExecutionException {
        TabModelOrchestrator orchestrator = TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().getTabModelOrchestratorSupplier().get());
        TabModelSelector selector = orchestrator.getTabModelSelector();
        TabModel model = selector.getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);

        Tab[] fullList = new Tab[] { tab0, tab1 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1 }, tab1, EMPTY, fullList, tab1);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, EMPTY, fullList, tab1);

        // 3.
        saveStateOnUiThread(orchestrator);
        fullList = new Tab[] { tab1 };
        checkState(model, new Tab[] { tab1 }, tab1, EMPTY, fullList, tab1);
        Assert.assertTrue(tab0.isClosing());
        Assert.assertFalse(tab0.isInitialized());
    }

    /**
     * Test opening recently closed tabs using the rewound list in Java.
     */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTab() throws TimeoutException {
        TabModelSelector selector = sActivityTestRule.getActivity().getTabModelSelector();
        TabModel model = selector.getModel(false);
        ChromeTabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity().getTabCreator(false));

        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab[] allTabs = new Tab[]{tab0, tab1};

        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[]{tab0}, tab0, new Tab[]{tab1}, allTabs, tab0);

        // Ensure tab recovery, and reuse of {@link Tab} objects in Java.
        openMostRecentlyClosedTabOnUiThread(selector);
        checkState(model, allTabs, tab0, EMPTY, allTabs, tab0);
    }

    /**
     * Test opening recently closed tab using native tab restore service.
     */
    @Test
    @MediumTest
    public void testOpenRecentlyClosedTabNative() throws TimeoutException {
        final TabModelSelector selector = sActivityTestRule.getActivity().getTabModelSelector();
        final TabModel model = selector.getModel(false);

        // Create new tab and wait until it's loaded.
        // Native can only successfully recover the tab after a page load has finished and
        // it has navigation history.
        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), TEST_URL_0, false);

        // Close the tab, and commit pending closure.
        Assert.assertEquals(model.getCount(), 2);
        closeTabOnUiThread(model, model.getTabAt(1), false);
        Assert.assertEquals(1, model.getCount());
        Tab tab0 = model.getTabAt(0);
        Tab[] tabs = new Tab[]{tab0};
        checkState(model, tabs, tab0, EMPTY, tabs, tab0);

        // Recover the page.
        openMostRecentlyClosedTabOnUiThread(selector);

        Assert.assertEquals(2, model.getCount());
        tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        tabs = new Tab[]{tab0, tab1};
        Assert.assertEquals(TEST_URL_0, ChromeTabUtils.getUrlStringOnUiThread(tab1));
        checkState(model, tabs, tab0, EMPTY, tabs, tab0);
    }

    /**
     * Test opening recently closed tab when we have multiple windows.
     * |  Action                    |   Result
     * 1. Create second window.     |
     * 2. Open tab in window 1.     |
     * 3. Open tab in window 2.     |
     * 4. Close tab in window 1.    |
     * 5. Close tab in window 2.    |
     * 6. Restore tab.              | Tab restored in window 2.
     * 7. Restore tab.              | Tab restored in window 1.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
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
        final TabModel secondModel = secondActivity.getTabModelSelector().getModel(false);

        // Create tabs.
        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), TEST_URL_0, false);
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

        // Restore one tab.
        openMostRecentlyClosedTabOnUiThread(firstSelector);
        Assert.assertEquals("Unexpected number of tabs in first window.", 1, firstModel.getCount());
        Assert.assertEquals(
                "Unexpected number of tabs in second window.", 2, secondModel.getCount());

        // Restore one more tab.
        openMostRecentlyClosedTabOnUiThread(firstSelector);

        // Check final states of both windows.
        Tab firstModelTab = firstModel.getTabAt(0);
        Tab secondModelTab = secondModel.getTabAt(0);
        Tab[] firstWindowTabs = new Tab[]{firstModelTab, firstModel.getTabAt(1)};
        Tab[] secondWindowTabs = new Tab[]{secondModelTab, secondModel.getTabAt(1)};
        checkState(firstModel, firstWindowTabs, firstModelTab, EMPTY, firstWindowTabs,
                firstModelTab);
        checkState(secondModel, secondWindowTabs, secondModelTab, EMPTY, secondWindowTabs,
                secondModelTab);
        Assert.assertEquals(TEST_URL_0, ChromeTabUtils.getUrlStringOnUiThread(firstWindowTabs[1]));
        Assert.assertEquals(TEST_URL_1, ChromeTabUtils.getUrlStringOnUiThread(secondWindowTabs[1]));

        secondActivity.finishAndRemoveTask();
    }

    /**
     * Test restoring closed tab from a closed window.
     * |  Action                    |   Result
     * 1. Create second window.     |
     * 2. Open tab in window 2.     |
     * 3. Close tab in window 2.    |
     * 4. Close second window.      |
     * 5. Restore tab.              | Tab restored in first window.
     */
    @Test
    @MediumTest
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
        secondModel.addObserver(new TabClosedObserver(closedCallback));
        closeTabOnUiThread(secondModel, secondModel.getTabAt(1), false);
        closedCallback.waitForCallback(0);

        Assert.assertEquals("Window 2 should have 1 tab.", 1, secondModel.getCount());

        // Closed the second window. Must wait until it's totally closed.
        int numExpectedActivities = ApplicationStatus.getRunningActivities().size() - 1;
        secondActivity.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(ApplicationStatus.getRunningActivities().size(),
                    Matchers.is(numExpectedActivities));
        });
        Assert.assertEquals("Window 1 should have 1 tab.", 1, firstModel.getCount());

        // Restore closed tab from second window. It should be created in first window.
        openMostRecentlyClosedTabOnUiThread(firstSelector);
        Assert.assertEquals("Closed tab in second window should be restored in the first window.",
                2, firstModel.getCount());
        Tab tab0 = firstModel.getTabAt(0);
        Tab tab1 = firstModel.getTabAt(1);
        Tab[] firstWindowTabs = new Tab[]{tab0, tab1};
        checkState(firstModel, firstWindowTabs, tab0, EMPTY, firstWindowTabs, tab0);
        Assert.assertEquals(TEST_URL_1, ChromeTabUtils.getUrlStringOnUiThread(tab1));
    }
}
