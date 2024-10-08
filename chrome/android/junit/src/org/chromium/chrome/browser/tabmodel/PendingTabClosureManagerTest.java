// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;

import androidx.annotation.NonNull;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

/** Unit tests for {@link PendingTabClosureManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PendingTabClosureManagerTest {
    private PendingTabClosureManager mPendingTabClosureManager;

    private static class FakeTabModel extends EmptyTabModel {
        private LinkedList<Tab> mTabs = new LinkedList<Tab>();
        private int mIndex = TabModel.INVALID_TAB_INDEX;

        public FakeTabModel() {}

        public void setTabs(Tab[] tabs) {
            mTabs = new LinkedList<Tab>(Arrays.asList(tabs));
        }

        public void clear() {
            mTabs.clear();
        }

        public void insertUndoneTabClosureAt(Tab tab, int insertIndex) {
            if (mIndex >= insertIndex) mIndex++;
            mTabs.add(insertIndex, tab);

            if (mIndex == INVALID_TAB_INDEX) {
                mIndex = insertIndex;
            }
        }

        @Override
        public int getCount() {
            return mTabs.size();
        }

        @Override
        public Tab getTabAt(int position) {
            return mTabs.get(position);
        }

        @Override
        public boolean isIncognito() {
            return false;
        }

        @Override
        public int indexOf(Tab tab) {
            return mTabs.indexOf(tab);
        }

        @Override
        public int index() {
            return mIndex;
        }

        @Override
        public boolean supportsPendingClosures() {
            return true;
        }
    }

    private class PendingClosureDelegate
            implements PendingTabClosureManager.PendingTabClosureDelegate {
        @Override
        public void insertUndoneTabClosureAt(Tab tab, int index) {
            mTabModel.insertUndoneTabClosureAt(tab, index);
        }

        @Override
        public void finalizeClosure(Tab tab) {}

        @Override
        public void notifyAllTabsClosureUndone() {}

        @Override
        public void notifyOnFinishingMultipleTabClosure(List<Tab> tabs) {}

        @Override
        public void notifyOnCancelingTabClosure(@NonNull Runnable undoRunnable) {}
    }

    FakeTabModel mTabModel;
    @Mock PendingClosureDelegate mDelegate;
    @Mock Profile mProfile;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTabModel = new FakeTabModel();
        mDelegate = spy(new PendingClosureDelegate());
        mPendingTabClosureManager = new PendingTabClosureManager(mTabModel, mDelegate);
    }

    @After
    public void tearDown() {
        mPendingTabClosureManager.destroy();
    }

    private void setupRewoundState(PendingTabClosureManager manager, Tab[] tabs) {
        manager.commitAllTabClosures();
        mTabModel.setTabs(tabs);
        manager.resetState();

        // Simulate initiating closing all tabs in the tab model.
        mTabModel.clear();
    }

    private void checkRewoundState(
            PendingTabClosureManager manager, Tab[] tabs, boolean ignoreClosing) {
        TabList rewoundList = manager.getRewoundList();
        Assert.assertEquals("Tab count not matching", tabs.length, rewoundList.getCount());
        for (int i = 0; i < tabs.length; i++) {
            if (!ignoreClosing) {
                Assert.assertTrue(manager.isClosurePending(tabs[i].getId()));
            }
            Assert.assertEquals(
                    "Tab at index " + Integer.toString(i) + " doesn't match.",
                    tabs[i],
                    rewoundList.getTabAt(i));
        }
    }

    /** Test that committing a single pending tab closure works. */
    @Test
    public void testCommitSingleTabEvent() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab[] tabList = new Tab[] {tab0};
        setupRewoundState(mPendingTabClosureManager, tabList);

        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(tabList), /* undoRunnable= */ null);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.commitTabClosure(tab0.getId());
        delegateInOrder
                .verify(mDelegate)
                .notifyOnFinishingMultipleTabClosure(eq(Arrays.asList(tabList)));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab0));
        checkRewoundState(mPendingTabClosureManager, new Tab[] {}, false);
    }

    /** Test that cancelling a single pending tab closure works. */
    @Test
    public void testCancelSingleTabEvent() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab[] tabList = new Tab[] {tab0};
        setupRewoundState(mPendingTabClosureManager, tabList);

        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(tabList), /* undoRunnable= */ null);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.cancelTabClosure(tab0.getId());
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab0), eq(0));
        // Still in rewound state as the tab continues to exist.
        checkRewoundState(mPendingTabClosureManager, tabList, true);
    }

    @Test
    public void testCancelSingleTabEvent_WithUndoRunnable() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab[] tabList = new Tab[] {tab0};
        setupRewoundState(mPendingTabClosureManager, tabList);

        Runnable undoRunnable = () -> {};
        mPendingTabClosureManager.addTabClosureEvent(Arrays.asList(tabList), undoRunnable);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.cancelTabClosure(tab0.getId());
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab0), eq(0));
        // Still in rewound state as the tab continues to exist.
        checkRewoundState(mPendingTabClosureManager, tabList, true);
        delegateInOrder.verify(mDelegate).notifyOnCancelingTabClosure(undoRunnable);
    }

    /**
     * Test that committing a pending multiple tab closure works and is deferred until all tabs
     * commit.
     */
    @Test
    public void testCommitMultipleTabEvent() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mProfile);
        Tab[] tabList = new Tab[] {tab1, tab0};
        setupRewoundState(mPendingTabClosureManager, tabList);

        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(tabList), /* undoRunnable= */ null);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.commitTabClosure(tab0.getId());
        // No commits actually occur until later.
        checkRewoundState(mPendingTabClosureManager, tabList, false);
        mPendingTabClosureManager.commitTabClosure(tab1.getId());
        delegateInOrder
                .verify(mDelegate)
                .notifyOnFinishingMultipleTabClosure(eq(Arrays.asList(tabList)));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab1));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab0));
        checkRewoundState(mPendingTabClosureManager, new Tab[] {}, false);
    }

    /** Test that cancelling a pending multiple tab closure works and happens immediately. */
    @Test
    public void testCancelMultipleTabEvent() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mProfile);
        Tab[] tabList = new Tab[] {tab1, tab0};
        setupRewoundState(mPendingTabClosureManager, tabList);

        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(tabList), /* undoRunnable= */ null);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.cancelTabClosure(tab0.getId());
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab0), eq(0));
        checkRewoundState(mPendingTabClosureManager, tabList, true);

        mPendingTabClosureManager.cancelTabClosure(tab1.getId());
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab1), eq(0));
        checkRewoundState(mPendingTabClosureManager, tabList, true);
    }

    @Test
    public void testPartialCommitThenCancel() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mProfile);
        Tab[] tabList = new Tab[] {tab1, tab0};
        setupRewoundState(mPendingTabClosureManager, tabList);

        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(tabList), /* undoRunnable= */ null);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.commitTabClosure(tab0.getId());
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.cancelTabClosure(tab1.getId());
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab1), eq(0));
        delegateInOrder.verify(mDelegate).notifyOnFinishingMultipleTabClosure(eq(List.of(tab0)));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab0));
        checkRewoundState(mPendingTabClosureManager, new Tab[] {tab1}, false);
    }

    @Test
    public void testPartialCancelThenCommit() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mProfile);
        Tab[] tabList = new Tab[] {tab1, tab0};
        setupRewoundState(mPendingTabClosureManager, tabList);

        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(tabList), /* undoRunnable= */ null);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.cancelTabClosure(tab0.getId());
        checkRewoundState(mPendingTabClosureManager, tabList, true);
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab0), eq(0));

        mPendingTabClosureManager.commitTabClosure(tab1.getId());
        delegateInOrder.verify(mDelegate).notifyOnFinishingMultipleTabClosure(eq(List.of(tab1)));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab1));
        checkRewoundState(mPendingTabClosureManager, new Tab[] {tab0}, false);
    }

    @Test
    public void testCommitAndCancelMultipleEventsOutOfOrder() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mProfile);
        Tab tab2 = new MockTab(2, mProfile);
        Tab tab3 = new MockTab(3, mProfile);
        Tab tab4 = new MockTab(4, mProfile);
        Tab[] tabList = new Tab[] {tab0, tab1, tab2, tab3, tab4};
        setupRewoundState(mPendingTabClosureManager, tabList);

        mPendingTabClosureManager.addTabClosureEvent(
                Collections.singletonList(tab0), /* undoRunnable= */ null);
        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(new Tab[] {tab2, tab4}), /* undoRunnable= */ null);
        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(new Tab[] {tab1, tab3}), /* undoRunnable= */ null);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.cancelTabClosure(tab3.getId());
        checkRewoundState(mPendingTabClosureManager, tabList, true);
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab3), eq(0));

        mPendingTabClosureManager.commitTabClosure(tab2.getId());
        // No commits actually occur until all tabs are committed.
        checkRewoundState(mPendingTabClosureManager, tabList, true);

        tabList = new Tab[] {tab0, tab1, tab3};
        mPendingTabClosureManager.commitTabClosure(tab4.getId());
        checkRewoundState(mPendingTabClosureManager, tabList, true);
        delegateInOrder
                .verify(mDelegate)
                .notifyOnFinishingMultipleTabClosure(eq(Arrays.asList(new Tab[] {tab2, tab4})));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab2));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab4));

        mPendingTabClosureManager.cancelTabClosure(tab1.getId());
        checkRewoundState(mPendingTabClosureManager, tabList, true);
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab1), eq(0));

        mPendingTabClosureManager.commitTabClosure(tab0.getId());
        delegateInOrder
                .verify(mDelegate)
                .notifyOnFinishingMultipleTabClosure(eq(Collections.singletonList(tab0)));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab0));
        checkRewoundState(mPendingTabClosureManager, new Tab[] {tab1, tab3}, true);
    }

    /**
     * Test that {@link PendingTabClosureManager#commitAllTabClosures()} commits all tabs queued
     * except for tabs that have been undone already.
     */
    @Test
    public void testCommitAllClosures() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mProfile);
        Tab tab2 = new MockTab(2, mProfile);
        Tab tab3 = new MockTab(3, mProfile);
        Tab tab4 = new MockTab(4, mProfile);
        Tab tab5 = new MockTab(5, mProfile);
        Tab[] tabList = new Tab[] {tab0, tab1, tab2, tab3, tab4, tab5};
        setupRewoundState(mPendingTabClosureManager, tabList);

        mPendingTabClosureManager.addTabClosureEvent(
                Collections.singletonList(tab0), /* undoRunnable= */ null);
        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(new Tab[] {tab1, tab4}), /* undoRunnable= */ null);
        mPendingTabClosureManager.addTabClosureEvent(
                Collections.singletonList(tab2), /* undoRunnable= */ null);
        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(new Tab[] {tab3, tab5}), /* undoRunnable= */ null);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.commitTabClosure(tab1.getId());
        // No commits actually occur until all tabs are committed.
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        tabList = new Tab[] {tab0, tab1, tab3, tab4, tab5};
        // Fully close tab 2.
        mPendingTabClosureManager.commitTabClosure(tab2.getId());
        checkRewoundState(mPendingTabClosureManager, tabList, false);
        delegateInOrder
                .verify(mDelegate)
                .notifyOnFinishingMultipleTabClosure(eq(Collections.singletonList(tab2)));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab2));

        // Restore tab 5.
        mPendingTabClosureManager.cancelTabClosure(tab5.getId());
        checkRewoundState(mPendingTabClosureManager, tabList, true);
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab5), eq(0));

        mPendingTabClosureManager.commitAllTabClosures();
        delegateInOrder
                .verify(mDelegate)
                .notifyOnFinishingMultipleTabClosure(eq(Collections.singletonList(tab0)));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab0));
        delegateInOrder
                .verify(mDelegate)
                .notifyOnFinishingMultipleTabClosure(eq(Arrays.asList(new Tab[] {tab1, tab4})));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab1));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab4));
        delegateInOrder
                .verify(mDelegate)
                .notifyOnFinishingMultipleTabClosure(eq(Collections.singletonList(tab3)));
        delegateInOrder.verify(mDelegate).finalizeClosure(eq(tab3));
        checkRewoundState(mPendingTabClosureManager, new Tab[] {tab5}, true);
    }

    /**
     * Test that {@link PendingTabClosureManager#openMostRecentlyClosedEntry()} undoes all tabs in
     * the most recent event even if some are ready to commit.
     */
    @Test
    public void testOpenMostRecentlyClosedWithCommit() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mProfile);
        Tab tab2 = new MockTab(2, mProfile);
        Tab tab3 = new MockTab(3, mProfile);
        Tab[] tabList = new Tab[] {tab0, tab1, tab2, tab3};
        setupRewoundState(mPendingTabClosureManager, tabList);

        mPendingTabClosureManager.addTabClosureEvent(
                Collections.singletonList(tab0), /* undoRunnable= */ null);
        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(new Tab[] {tab1, tab2, tab3}), /* undoRunnable= */ null);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        mPendingTabClosureManager.commitTabClosure(tab1.getId());
        // No commits actually occur until all tabs are committed.
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        // Restore entry as a whole.
        mPendingTabClosureManager.openMostRecentlyClosedEntry();
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab1), eq(0));
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab2), eq(1));
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab3), eq(2));
        checkRewoundState(mPendingTabClosureManager, tabList, true);
    }

    /**
     * Test that {@link PendingTabClosureManager#openMostRecentlyClosedEntry()} undoes all tabs in
     * the most recent event except those already undone..
     */
    @Test
    public void testOpenMostRecentlyClosedWithClose() {
        InOrder delegateInOrder = inOrder(mDelegate);
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mProfile);
        Tab tab2 = new MockTab(2, mProfile);
        Tab tab3 = new MockTab(3, mProfile);
        Tab[] tabList = new Tab[] {tab0, tab1, tab2, tab3};
        setupRewoundState(mPendingTabClosureManager, tabList);

        mPendingTabClosureManager.addTabClosureEvent(
                Collections.singletonList(tab0), /* undoRunnable= */ null);
        mPendingTabClosureManager.addTabClosureEvent(
                Arrays.asList(new Tab[] {tab1, tab2, tab3}), /* undoRunnable= */ null);
        checkRewoundState(mPendingTabClosureManager, tabList, false);

        // Restore tab 2.
        mPendingTabClosureManager.cancelTabClosure(tab2.getId());
        checkRewoundState(mPendingTabClosureManager, tabList, true);
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab2), eq(0));

        // Restore entry as a whole.
        mPendingTabClosureManager.openMostRecentlyClosedEntry();
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab1), eq(0));
        delegateInOrder.verify(mDelegate).insertUndoneTabClosureAt(eq(tab3), eq(2));
        checkRewoundState(mPendingTabClosureManager, tabList, true);
    }
}
