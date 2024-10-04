// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Unit tests for undo and restoring of tabs in a {@link TabModel}. For additional tests that are
 * impossible or difficult to implement as unit test see {@link UndoTabModelTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class UndoTabModelUnitTest {
    private static final long FAKE_NATIVE_ADDRESS = 123L;
    private static final Tab[] sEmptyList = new Tab[] {};

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    /** Disable native calls from {@link TabModelJniBridge}. */
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private TabModelJniBridge.Natives mTabModelJniBridge;

    /** Required to be non-null for {@link TabModelJniBridge}. */
    @Mock private Profile mProfile;

    @Mock private Profile mIncognitoProfile;

    /** Required to simulate tab thumbnail deletion. */
    @Mock private TabContentManager mTabContentManager;

    /** Required to handle some tab lookup actions. */
    @Mock private TabModelDelegate mTabModelDelegate;

    /** Required to handle some actions and initialize {@link TabModelOrderControllerImpl}. */
    @Mock private TabModelSelector mTabModelSelector;

    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabModelFilter mTabModelFilter;

    @Mock private Callback<Tab> mTabSupplierObserver;

    private int mNextTabId;

    @Before
    public void setUp() {
        // Disable HomepageManager#shouldCloseAppWithZeroTabs() for TabModelImpl#closeAllTabs().
        HomepageManager.getInstance().setPrefHomepageEnabled(false);

        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        mJniMocker.mock(TabModelJniBridgeJni.TEST_HOOKS, mTabModelJniBridge);
        when(mTabModelJniBridge.init(any(), any(), anyInt(), anyBoolean()))
                .thenReturn(FAKE_NATIVE_ADDRESS);

        when(mTabModelDelegate.isReparentingInProgress()).thenReturn(false);

        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mTabModelFilter);
        when(mTabModelFilter.getValidPosition(any(), anyInt()))
                .thenAnswer(i -> i.getArguments()[1]);

        mNextTabId = 0;
    }

    /** Create a {@link TabModel} to use for the test. */
    private TabModelImpl createTabModel(boolean isIncognito) {
        AsyncTabParamsManager realAsyncTabParamsManager =
                AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        TabModelOrderControllerImpl orderController =
                new TabModelOrderControllerImpl(mTabModelSelector);
        TabModelImpl tabModel;
        final boolean supportUndo = !isIncognito;
        if (isIncognito) {
            // TODO(crbug.com/40222755): Consider using an incognito tab model.
            tabModel =
                    new TabModelImpl(
                            mIncognitoProfile,
                            ActivityType.TABBED,
                            /* regularTabCreator= */ null,
                            /* incognitoTabCreator= */ null,
                            orderController,
                            mTabContentManager,
                            () -> NextTabPolicy.HIERARCHICAL,
                            realAsyncTabParamsManager,
                            mTabModelDelegate,
                            supportUndo,
                            /* trackInNativeModelList= */ true);
            when(mTabModelSelector.getModel(true)).thenReturn(tabModel);
        } else {
            tabModel =
                    new TabModelImpl(
                            mProfile,
                            ActivityType.TABBED,
                            /* regularTabCreator= */ null,
                            /* incognitoTabCreator= */ null,
                            orderController,
                            mTabContentManager,
                            () -> NextTabPolicy.HIERARCHICAL,
                            realAsyncTabParamsManager,
                            mTabModelDelegate,
                            supportUndo,
                            /* trackInNativeModelList= */ true);
            when(mTabModelSelector.getModel(false)).thenReturn(tabModel);
        }
        // Assume the model is the current and active model.
        tabModel.setActive(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(tabModel);
        // Avoid NPE in TabModelImpl#findTabInAllTabModels() by assuming two duplicate models exist
        // as it doesn't matter for that method.
        when(mTabModelDelegate.getModel(anyBoolean())).thenReturn(tabModel);
        return tabModel;
    }

    /** Check {@code model} contains the correct tab lists and has the right {@code selectedTab}. */
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
        assertEquals("Wrong selected tab", selectedTab, TabModelUtils.getCurrentTab(model));

        // Check the list of tabs.
        assertEquals("Incorrect number of tabs", tabsList.length, model.getCount());
        for (int i = 0; i < tabsList.length; i++) {
            assertEquals("Unexpected tab at " + i, tabsList[i].getId(), model.getTabAt(i).getId());
        }

        // Check the list of tabs we expect to be closing.
        for (int i = 0; i < closingTabs.length; i++) {
            int id = closingTabs[i].getId();
            assertTrue("Tab " + id + " not in closing list", model.isClosurePending(id));
        }

        TabList fullModel = model.getComprehensiveModel();

        // Check the comprehensive selected tab.
        assertEquals("Wrong selected tab", fullSelectedTab, TabModelUtils.getCurrentTab(fullModel));

        // Check the comprehensive list of tabs.
        assertEquals("Incorrect number of tabs", fullTabsList.length, fullModel.getCount());
        for (int i = 0; i < fullModel.getCount(); i++) {
            int id = fullModel.getTabAt(i).getId();
            assertEquals("Unexpected tab at " + i, fullTabsList[i].getId(), id);
        }
    }

    private void createTab(final TabModel model, boolean isIncognito) {
        final int launchType = TabLaunchType.FROM_CHROME_UI;
        MockTab tab =
                MockTab.createAndInitialize(
                        mNextTabId++, isIncognito ? mIncognitoProfile : mProfile, launchType);
        tab.setIsInitialized(true);
        model.addTab(tab, -1, launchType, TabCreationState.LIVE_IN_FOREGROUND);
    }

    private void selectTab(final TabModel model, final Tab tab) {
        model.setIndex(model.indexOf(tab), TabSelectionType.FROM_USER);
    }

    private void closeTab(final TabModel model, final Tab tab, final boolean undoable)
            throws TimeoutException {
        // Check preconditions.
        assertFalse(tab.isClosing());
        assertTrue(tab.isInitialized());
        assertFalse(model.isClosurePending(tab.getId()));
        assertNotNull(model.getTabById(tab.getId()));

        final CallbackHelper didReceivePendingClosureHelper = new CallbackHelper();
        model.addObserver(
                new TabModelObserver() {
                    @Override
                    public void tabPendingClosure(Tab tab) {
                        didReceivePendingClosureHelper.notifyCalled();
                    }
                });

        // Take action.
        model.closeTabs(TabClosureParams.closeTab(tab).allowUndo(undoable).build());

        boolean didMakePending = undoable && model.supportsPendingClosures();

        // Make sure the TabModel throws a tabPendingClosure callback if necessary.
        if (didMakePending) didReceivePendingClosureHelper.waitForCallback(0);

        // Check post conditions
        assertEquals(didMakePending, model.isClosurePending(tab.getId()));
        assertNull(model.getTabById(tab.getId()));
        assertTrue(tab.isClosing());
        assertEquals(didMakePending, tab.isInitialized());
    }

    private void closeMultipleTabsInternal(
            final TabModel model, final Runnable closeRunnable, final boolean undoable)
            throws TimeoutException {
        final CallbackHelper didReceivePendingClosureHelper = new CallbackHelper();
        model.addObserver(
                new TabModelObserver() {
                    @Override
                    public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                        didReceivePendingClosureHelper.notifyCalled();
                    }
                });
        closeRunnable.run();

        boolean didMakePending = undoable && model.supportsPendingClosures();

        // Make sure the TabModel throws a tabPendingClosure callback if necessary.
        if (didMakePending) didReceivePendingClosureHelper.waitForCallback(0);
    }

    private void closeMultipleTabs(
            final TabModel model, final List<Tab> tabs, final boolean undoable)
            throws TimeoutException {
        closeMultipleTabsInternal(
                model,
                () -> model.closeTabs(TabClosureParams.closeTabs(tabs).allowUndo(undoable).build()),
                undoable);
    }

    private void closeAllTabs(final TabModel model) throws TimeoutException {
        closeMultipleTabsInternal(
                model, () -> model.closeTabs(TabClosureParams.closeAllTabs().build()), true);
    }

    private void cancelTabClosure(final TabModel model, final Tab tab) throws TimeoutException {
        // Check preconditions.
        assertTrue(tab.isClosing());
        assertTrue(tab.isInitialized());
        assertTrue(model.isClosurePending(tab.getId()));
        assertNull(model.getTabById(tab.getId()));

        final CallbackHelper didReceiveClosureCancelledHelper = new CallbackHelper();
        model.addObserver(
                new TabModelObserver() {
                    @Override
                    public void tabClosureUndone(Tab tab) {
                        didReceiveClosureCancelledHelper.notifyCalled();
                    }
                });

        // Take action.
        model.cancelTabClosure(tab.getId());

        // Make sure the TabModel throws a tabClosureUndone.
        didReceiveClosureCancelledHelper.waitForCallback(0);

        // Check post conditions.
        assertFalse(model.isClosurePending(tab.getId()));
        assertNotNull(model.getTabById(tab.getId()));
        assertFalse(tab.isClosing());
        assertTrue(tab.isInitialized());
    }

    private void cancelAllTabClosures(final TabModel model, final Tab[] expectedToClose)
            throws TimeoutException {
        final CallbackHelper tabClosureUndoneHelper = new CallbackHelper();
        final CallbackHelper allTabClosureCancellationCompletedHelper = new CallbackHelper();

        for (int i = 0; i < expectedToClose.length; i++) {
            Tab tab = expectedToClose[i];
            assertTrue(tab.isClosing());
            assertTrue(tab.isInitialized());
            assertTrue(model.isClosurePending(tab.getId()));
            assertNull(model.getTabById(tab.getId()));

            // Make sure that this TabModel throws the right events.
            model.addObserver(
                    new TabModelObserver() {
                        @Override
                        public void tabClosureUndone(Tab currentTab) {
                            tabClosureUndoneHelper.notifyCalled();
                        }

                        @Override
                        public void allTabsClosureUndone() {
                            allTabClosureCancellationCompletedHelper.notifyCalled();
                        }
                    });
        }

        for (int i = 0; i < expectedToClose.length; i++) {
            Tab tab = expectedToClose[i];
            model.cancelTabClosure(tab.getId());
        }
        model.notifyAllTabsClosureUndone();

        tabClosureUndoneHelper.waitForCallback(0, expectedToClose.length);
        allTabClosureCancellationCompletedHelper.waitForCallback(0, 1);

        for (int i = 0; i < expectedToClose.length; i++) {
            final Tab tab = expectedToClose[i];
            assertFalse(model.isClosurePending(tab.getId()));
            assertNotNull(model.getTabById(tab.getId()));
            assertFalse(tab.isClosing());
            assertTrue(tab.isInitialized());
        }
    }

    private void commitTabClosure(final TabModel model, final Tab tab) throws TimeoutException {
        // Check preconditions.
        assertTrue(tab.isClosing());
        assertTrue(tab.isInitialized());
        assertTrue(model.isClosurePending(tab.getId()));
        assertNull(model.getTabById(tab.getId()));

        final CallbackHelper didReceiveClosureCommittedHelper = new CallbackHelper();
        model.addObserver(
                new TabModelObserver() {
                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        didReceiveClosureCommittedHelper.notifyCalled();
                    }
                });

        // Take action.
        model.commitTabClosure(tab.getId());

        // Make sure the TabModel throws a tabClosureCommitted.
        didReceiveClosureCommittedHelper.waitForCallback(0);

        // Check post conditions
        assertFalse(model.isClosurePending(tab.getId()));
        assertNull(model.getTabById(tab.getId()));
        assertTrue(tab.isClosing());
        assertFalse(tab.isInitialized());
    }

    private void commitAllTabClosures(final TabModel model, Tab[] expectedToClose)
            throws TimeoutException {
        final CallbackHelper tabClosureCommittedHelper = new CallbackHelper();

        for (int i = 0; i < expectedToClose.length; i++) {
            Tab tab = expectedToClose[i];
            assertTrue(tab.isClosing());
            assertTrue(tab.isInitialized());
            assertTrue(model.isClosurePending(tab.getId()));

            // Make sure that this TabModel throws the right events.
            model.addObserver(
                    new TabModelObserver() {
                        @Override
                        public void tabClosureCommitted(Tab currentTab) {
                            tabClosureCommittedHelper.notifyCalled();
                        }
                    });
        }

        model.commitAllTabClosures();

        tabClosureCommittedHelper.waitForCallback(0, expectedToClose.length);
        for (int i = 0; i < expectedToClose.length; i++) {
            final Tab tab = expectedToClose[i];
            assertTrue(tab.isClosing());
            assertFalse(tab.isInitialized());
            assertFalse(model.isClosurePending(tab.getId()));
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
    @SmallTest
    public void testSingleTab() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);

        Tab[] fullList = new Tab[] {tab0};

        // 1.
        checkState(model, new Tab[] {tab0}, tab0, sEmptyList, fullList, tab0);

        // 2.
        closeTab(model, tab0, true);
        checkState(model, sEmptyList, null, new Tab[] {tab0}, fullList, tab0);

        // 3.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, sEmptyList, fullList, tab0);

        // 4.
        closeTab(model, tab0, true);
        checkState(model, sEmptyList, null, new Tab[] {tab0}, fullList, tab0);

        // 5.
        commitTabClosure(model, tab0);
        fullList = sEmptyList;
        checkState(model, sEmptyList, null, sEmptyList, sEmptyList, null);

        // 6.
        createTab(model, isIncognito);
        tab0 = model.getTabAt(0);
        fullList = new Tab[] {tab0};
        checkState(model, new Tab[] {tab0}, tab0, sEmptyList, fullList, tab0);

        // 7.
        closeTab(model, tab0, true);
        checkState(model, sEmptyList, null, new Tab[] {tab0}, fullList, tab0);

        // 8.
        commitAllTabClosures(model, new Tab[] {tab0});
        fullList = sEmptyList;
        checkState(model, sEmptyList, null, sEmptyList, sEmptyList, null);

        // 9.
        createTab(model, isIncognito);
        tab0 = model.getTabAt(0);
        fullList = new Tab[] {tab0};
        checkState(model, new Tab[] {tab0}, tab0, sEmptyList, fullList, tab0);

        // 10.
        closeTab(model, tab0, false);
        fullList = sEmptyList;
        checkState(model, sEmptyList, null, sEmptyList, fullList, null);
        assertTrue(tab0.isClosing());
        assertFalse(tab0.isInitialized());
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
    @SmallTest
    public void testTwoTabs() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);

        Tab[] fullList = new Tab[] {tab0, tab1};

        // 1.
        checkState(model, new Tab[] {tab0, tab1}, tab1, sEmptyList, fullList, tab1);

        // 2.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 3.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1}, tab1, sEmptyList, fullList, tab1);

        // 4.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 5.
        closeTab(model, tab1, true);
        checkState(model, sEmptyList, null, new Tab[] {tab0, tab1}, fullList, tab0);

        // 6.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 7.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1}, tab1, sEmptyList, fullList, tab1);

        // 8.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1}, fullList, tab0);

        // 9.
        closeTab(model, tab0, true);
        checkState(model, sEmptyList, null, new Tab[] {tab0, tab1}, fullList, tab0);

        // 10.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 11.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1}, tab1, sEmptyList, fullList, tab1);

        // 12.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1}, fullList, tab0);

        // 13.
        closeTab(model, tab0, true);
        checkState(model, sEmptyList, null, new Tab[] {tab0, tab1}, fullList, tab0);

        // 14.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1}, fullList, tab0);

        // 15.
        closeTab(model, tab0, true);
        checkState(model, sEmptyList, null, new Tab[] {tab0, tab1}, fullList, tab0);

        // 16.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1}, fullList, tab0);

        // 17.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab0, tab1}, tab0, sEmptyList, fullList, tab0);

        // 18.
        closeTab(model, tab0, false);
        fullList = new Tab[] {tab1};
        checkState(model, new Tab[] {tab1}, tab1, sEmptyList, fullList, tab1);

        // 19.
        createTab(model, isIncognito);
        tab0 = model.getTabAt(1);
        fullList = new Tab[] {tab1, tab0};
        checkState(model, new Tab[] {tab1, tab0}, tab0, sEmptyList, fullList, tab0);

        // 20.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 21.
        commitTabClosure(model, tab0);
        fullList = new Tab[] {tab1};
        checkState(model, new Tab[] {tab1}, tab1, sEmptyList, fullList, tab1);

        // 22.
        createTab(model, isIncognito);
        tab0 = model.getTabAt(1);
        fullList = new Tab[] {tab1, tab0};
        checkState(model, new Tab[] {tab1, tab0}, tab0, sEmptyList, fullList, tab0);

        // 23.
        closeTab(model, tab0, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab0}, fullList, tab1);

        // 24.
        closeTab(model, tab1, true);
        checkState(model, sEmptyList, null, new Tab[] {tab1, tab0}, fullList, tab1);

        // 25.
        commitAllTabClosures(model, new Tab[] {tab1, tab0});
        checkState(model, sEmptyList, null, sEmptyList, sEmptyList, null);
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
    @SmallTest
    public void testInOrderRestore() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        final Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

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
        checkState(model, sEmptyList, null, new Tab[] {tab3, tab2, tab1, tab0}, fullList, tab0);

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
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

        // 10.
        selectTab(model, tab3);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

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
        checkState(model, sEmptyList, null, new Tab[] {tab0, tab1, tab2, tab3}, fullList, tab0);

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
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab0, sEmptyList, fullList, tab0);

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
        checkState(model, new Tab[] {tab0, tab1, tab3}, tab1, new Tab[] {tab2}, fullList, tab1);

        // 24.
        cancelTabClosure(model, tab2);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab1, sEmptyList, fullList, tab1);
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
    @SmallTest
    public void testReverseOrderRestore() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

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
        checkState(model, sEmptyList, null, new Tab[] {tab3, tab2, tab1, tab0}, fullList, tab0);

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
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab0, sEmptyList, fullList, tab0);

        // 10.
        closeTab(model, tab3, true);
        checkState(model, new Tab[] {tab0, tab1, tab2}, tab0, new Tab[] {tab3}, fullList, tab0);

        // 11.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab1}, tab0, new Tab[] {tab2, tab3}, fullList, tab0);

        // 12.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1, tab2, tab3}, fullList, tab0);

        // 13.
        closeTab(model, tab0, true);
        checkState(model, sEmptyList, null, new Tab[] {tab0, tab1, tab2, tab3}, fullList, tab0);

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
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

        // 18.
        selectTab(model, tab3);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

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
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab1, sEmptyList, fullList, tab1);
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
    @SmallTest
    public void testOutOfOrder1() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

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
        checkState(model, sEmptyList, null, new Tab[] {tab3, tab2, tab1, tab0}, fullList, tab0);

        // 6.
        cancelTabClosure(model, tab2);
        checkState(model, new Tab[] {tab2}, tab2, new Tab[] {tab3, tab1, tab0}, fullList, tab2);

        // 7.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab1, tab2}, tab2, new Tab[] {tab3, tab0}, fullList, tab2);

        // 8.
        cancelTabClosure(model, tab3);
        checkState(model, new Tab[] {tab1, tab2, tab3}, tab2, new Tab[] {tab0}, fullList, tab2);

        // 9.
        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab2, sEmptyList, fullList, tab2);

        // 10.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab2, new Tab[] {tab1}, fullList, tab2);

        // 11.
        cancelTabClosure(model, tab1);
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab2, sEmptyList, fullList, tab2);

        // 12.
        closeTab(model, tab3, false);
        fullList = new Tab[] {tab0, tab1, tab2};
        checkState(model, new Tab[] {tab0, tab1, tab2}, tab2, sEmptyList, fullList, tab2);

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
        checkState(model, new Tab[] {tab1, tab2}, tab2, sEmptyList, fullList, tab2);

        // 17.
        closeTab(model, tab2, false);
        fullList = new Tab[] {tab1};
        checkState(model, new Tab[] {tab1}, tab1, sEmptyList, fullList, tab1);
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
    @SmallTest
    public void testOutOfOrder2() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

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
        checkState(model, new Tab[] {tab1, tab3}, tab3, sEmptyList, fullList, tab3);

        // 12.
        closeTab(model, tab3, true);
        checkState(model, new Tab[] {tab1}, tab1, new Tab[] {tab3}, fullList, tab1);

        // 13.
        closeTab(model, tab1, true);
        checkState(model, sEmptyList, null, new Tab[] {tab1, tab3}, fullList, tab1);

        // 14.
        commitAllTabClosures(model, new Tab[] {tab1, tab3});
        checkState(model, sEmptyList, null, sEmptyList, sEmptyList, null);
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
    @SmallTest
    public void testCloseAll() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, fullList, tab3, sEmptyList, fullList, tab3);

        // 2.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, new Tab[] {tab1}, fullList, tab3);

        // 3.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab3}, tab3, new Tab[] {tab1, tab2}, fullList, tab3);

        // 4.
        closeAllTabs(model);
        checkState(model, sEmptyList, null, fullList, fullList, tab0);

        // 5.
        cancelAllTabClosures(model, fullList);
        checkState(model, fullList, tab0, sEmptyList, fullList, tab0);

        // 6.
        closeAllTabs(model);
        checkState(model, sEmptyList, null, fullList, fullList, tab0);

        // 7.
        commitAllTabClosures(model, fullList);
        checkState(model, sEmptyList, null, sEmptyList, sEmptyList, null);
        assertTrue(tab0.isClosing());
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertTrue(tab3.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab1.isInitialized());
        assertFalse(tab2.isInitialized());
        assertFalse(tab3.isInitialized());

        // 8.
        createTab(model, isIncognito);
        tab0 = model.getTabAt(0);
        fullList = new Tab[] {tab0};
        checkState(model, new Tab[] {tab0}, tab0, sEmptyList, fullList, tab0);

        // 9.
        closeAllTabs(model);
        checkState(model, sEmptyList, null, fullList, fullList, tab0);
        assertTrue(tab0.isClosing());
        assertTrue(tab0.isInitialized());
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
     */
    @Test
    @SmallTest
    public void testCloseTab() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

        // 2.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

        // 3.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab3}, tab3, sEmptyList, fullList, tab3);

        // 4.
        closeTab(model, tab3, false);
        fullList = new Tab[] {tab0};
        checkState(model, new Tab[] {tab0}, tab0, sEmptyList, fullList, tab0);
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertFalse(tab1.isInitialized());
        assertFalse(tab2.isInitialized());
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
    @SmallTest
    public void testMoveTab() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

        // 2.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

        // 3.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab3}, tab3, sEmptyList, fullList, tab3);

        // 4.
        model.moveTab(tab0.getId(), 2);
        fullList = new Tab[] {tab3, tab0};
        checkState(model, new Tab[] {tab3, tab0}, tab3, sEmptyList, fullList, tab3);
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertFalse(tab1.isInitialized());
        assertFalse(tab1.isInitialized());
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
    @Test
    @SmallTest
    public void testAddTab() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

        // 2.
        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, sEmptyList, fullList, tab3);

        // 3.
        closeTab(model, tab2, true);
        checkState(model, new Tab[] {tab0, tab3}, tab3, sEmptyList, fullList, tab3);

        // 4.
        createTab(model, isIncognito);
        Tab tab4 = model.getTabAt(2);
        fullList = new Tab[] {tab0, tab3, tab4};
        checkState(model, new Tab[] {tab0, tab3, tab4}, tab4, sEmptyList, fullList, tab4);
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
        checkState(model, sEmptyList, null, new Tab[] {tab4, tab3, tab0}, fullList, tab0);

        // 8.
        createTab(model, isIncognito);
        Tab tab5 = model.getTabAt(0);
        fullList = new Tab[] {tab5};
        checkState(model, new Tab[] {tab5}, tab5, sEmptyList, fullList, tab5);
        assertTrue(tab0.isClosing());
        assertTrue(tab3.isClosing());
        assertTrue(tab4.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab3.isInitialized());
        assertFalse(tab4.isInitialized());
    }

    /**
     * Test a {@link TabModel} where undo is not supported:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         -                 [ 0 2 3s ]
     * 3.  CloseAll                   -                  -                 -
     */
    @Test
    @SmallTest
    public void testUndoNotSupported() throws TimeoutException {
        final boolean isIncognito = true;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3};

        // 1.
        checkState(model, new Tab[] {tab0, tab1, tab2, tab3}, tab3, sEmptyList, fullList, tab3);
        assertFalse(model.supportsPendingClosures());

        // 2.
        closeTab(model, tab1, true);
        fullList = new Tab[] {tab0, tab2, tab3};
        checkState(model, new Tab[] {tab0, tab2, tab3}, tab3, sEmptyList, fullList, tab3);
        assertTrue(tab1.isClosing());
        assertFalse(tab1.isInitialized());

        // 3.
        closeAllTabs(model);
        checkState(model, sEmptyList, null, sEmptyList, sEmptyList, null);
        assertTrue(tab0.isClosing());
        assertTrue(tab2.isClosing());
        assertTrue(tab3.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab2.isInitialized());
        assertFalse(tab3.isInitialized());
    }

    /**
     * Test a {@link TabModel} where undo is not supported and
     * {@link TabModelObserver#onFinishingMultipleTabClosure()} is called.
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3 4s ]     -                 [ 0 1 2 3 4s ]
     * 2.  CloseTab(1)                [ 0 2 3 4s ]       -                 [ 0 2 3 4s ]
     * 3.  CloseMultipleTabs(2, 4)    [ 0 3s ]           -                 [ 0 3s ]
     * 4.  CloseAll                   -                  -                 -
     */
    @Test
    @SmallTest
    public void testUndoNotSupportedOnFinishingMultipleTabClosure() throws TimeoutException {
        final boolean isIncognito = true;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);
        Tab tab4 = model.getTabAt(4);

        Tab[] fullList = new Tab[] {tab0, tab1, tab2, tab3, tab4};

        // 1.
        checkState(model, fullList, tab4, sEmptyList, fullList, tab4);
        assertFalse(model.supportsPendingClosures());

        final ArrayList<Tab> lastClosedTabs = new ArrayList<Tab>();
        model.addObserver(
                new TabModelObserver() {
                    @Override
                    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                        lastClosedTabs.clear();
                        lastClosedTabs.addAll(tabs);
                    }
                });

        // 2.
        closeTab(model, tab1, true);
        fullList = new Tab[] {tab0, tab2, tab3, tab4};
        checkState(model, fullList, tab4, sEmptyList, fullList, tab4);
        assertTrue(tab1.isClosing());
        assertFalse(tab1.isInitialized());
        assertArrayEquals(new Tab[] {tab1}, lastClosedTabs.toArray(new Tab[0]));

        // 3.
        closeMultipleTabs(model, Arrays.asList(new Tab[] {tab2, tab4}), true);
        fullList = new Tab[] {tab0, tab3};
        checkState(model, fullList, tab0, sEmptyList, fullList, tab0);
        assertTrue(tab2.isClosing());
        assertTrue(tab4.isClosing());
        assertFalse(tab2.isInitialized());
        assertFalse(tab4.isInitialized());
        assertArrayEquals(new Tab[] {tab2, tab4}, lastClosedTabs.toArray(new Tab[0]));

        // 4.
        closeAllTabs(model);
        checkState(model, sEmptyList, null, sEmptyList, sEmptyList, null);
        assertTrue(tab0.isClosing());
        assertTrue(tab3.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab3.isInitialized());
        assertArrayEquals(new Tab[] {tab0, tab3}, lastClosedTabs.toArray(new Tab[0]));
    }

    /** Test opening recently closed tabs using the rewound list in Java. */
    @Test
    @SmallTest
    public void testOpenRecentlyClosedTab() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        createTab(model, isIncognito);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab[] allTabs = new Tab[] {tab0, tab1};

        closeTab(model, tab1, true);
        checkState(model, new Tab[] {tab0}, tab0, new Tab[] {tab1}, allTabs, tab0);

        // Ensure tab recovery, and reuse of {@link Tab} objects in Java.
        model.openMostRecentlyClosedEntry();
        checkState(model, allTabs, tab0, sEmptyList, allTabs, tab0);
    }

    @Test
    @SmallTest
    public void testActiveModelCloseAndUndoForTabSupplier() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModel model = createTabModel(isIncognito);
        assertEquals(0, model.getTabCountSupplier().get().intValue());
        createTab(model, isIncognito);
        model.getCurrentTabSupplier().addObserver(mTabSupplierObserver);
        ShadowLooper.runUiThreadTasks();

        Tab tab0 = model.getTabAt(0);
        Tab[] fullList = new Tab[] {tab0};

        assertEquals(tab0, model.getCurrentTabSupplier().get());
        verify(mTabSupplierObserver).onResult(eq(tab0));
        checkState(model, new Tab[] {tab0}, tab0, sEmptyList, fullList, tab0);
        assertEquals(1, model.getTabCountSupplier().get().intValue());

        closeTab(model, tab0, true);
        checkState(model, sEmptyList, null, new Tab[] {tab0}, fullList, tab0);
        assertNull(model.getCurrentTabSupplier().get());
        verify(mTabSupplierObserver).onResult(isNull());
        assertEquals(0, model.getTabCountSupplier().get().intValue());

        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, sEmptyList, fullList, tab0);
        assertEquals(tab0, model.getCurrentTabSupplier().get());
        verify(mTabSupplierObserver, times(2)).onResult(eq(tab0));
        assertEquals(1, model.getTabCountSupplier().get().intValue());
    }

    @Test
    @SmallTest
    public void testInactiveModelCloseAndUndoForTabSupplier() throws TimeoutException {
        final boolean isIncognito = false;
        final TabModelImpl model = createTabModel(isIncognito);
        assertEquals(0, model.getTabCountSupplier().get().intValue());
        model.getCurrentTabSupplier().addObserver(mTabSupplierObserver);
        model.setActive(false);
        createTab(model, isIncognito);

        Tab tab0 = model.getTabAt(0);
        Tab[] fullList = new Tab[] {tab0};

        assertEquals(tab0, model.getCurrentTabSupplier().get());
        verify(mTabSupplierObserver).onResult(eq(tab0));
        checkState(model, new Tab[] {tab0}, tab0, sEmptyList, fullList, tab0);
        assertEquals(1, model.getTabCountSupplier().get().intValue());

        closeTab(model, tab0, true);
        checkState(model, sEmptyList, null, new Tab[] {tab0}, fullList, tab0);
        assertNull(model.getCurrentTabSupplier().get());
        verify(mTabSupplierObserver).onResult(isNull());
        assertEquals(0, model.getTabCountSupplier().get().intValue());

        cancelTabClosure(model, tab0);
        checkState(model, new Tab[] {tab0}, tab0, sEmptyList, fullList, tab0);
        assertEquals(tab0, model.getCurrentTabSupplier().get());
        verify(mTabSupplierObserver, times(2)).onResult(eq(tab0));
        assertEquals(1, model.getTabCountSupplier().get().intValue());
    }
}
