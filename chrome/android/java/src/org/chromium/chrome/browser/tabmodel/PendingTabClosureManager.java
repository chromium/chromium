// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;

/**
 * Manages the logic pertaining to tracking pending tab closures for a {@link TabModelImpl}.
 * This class does not directly perform any tab related actions and delegates that work
 * to the {@link PendingTabClosureDelegate} it is provided.
 */
public class PendingTabClosureManager {
    /**
     * Delegate for applying changes to a {@link TabList} based on the decision logic in
     * {@link PendingTabClosureManager}.
     */
    public interface PendingTabClosureDelegate {
        /**
         * Return {@code tab} to the {@link TabList} at {@code index}.
         * @param tab The tab to insert.
         * @param index The location to insert the tab at.
         */
        public void insertUndoneTabClosureAt(Tab tab, int index);

        /**
         * Finalize the closure of a Tab.
         * @param tab The tab to finalize the closure of.
         */
        public void finalizeClosure(Tab tab);

        /**
         * Notify observers about completion of undo action to restore all tabs.
         */
        public void notifyAllTabsClosureUndone();

        /**
         * Request to notify observers that {@code tabs} will be closed.
         * @param tabs The list of tabs to close together.
         */
        public void notifyOnFinishingMultipleTabClosure(List<Tab> tabs);
    }

    /**
     * Represents a set of tabs closed together.
     */
    private class TabClosureEvent {
        private final LinkedList<Tab> mClosingTabs;
        private final HashSet<Tab> mUnhandledTabs;
        private boolean mReadyToCommitCalled;
        private boolean mCancelCalled;

        /**
         * @param tabs The list of closing tabs.
         */
        public TabClosureEvent(List<Tab> tabs) {
            mClosingTabs = new LinkedList<Tab>(tabs);
            mUnhandledTabs = new HashSet<Tab>(mClosingTabs);
        }

        /**
         * @param tab The tab to mark as having closed.
         */
        public boolean markReadyToCommit(Tab tab) {
            final boolean removed = mUnhandledTabs.remove(tab);
            if (removed) {
                assert !mCancelCalled
                    : "Committing a tab closure from an event that was partly cancelled.";
                mReadyToCommitCalled = true;
            }
            return removed;
        }

        /**
         * @param tab The tab to mark as having been cancelled.
         */
        public boolean markCancelled(Tab tab) {
            final boolean removed = mUnhandledTabs.remove(tab);
            if (removed) {
                mClosingTabs.remove(tab);
                assert !mReadyToCommitCalled
                    : "Cancelling a tab closure from an event that was partly ready to commit.";
                mCancelCalled = true;
            }
            return removed;
        }

        /**
         * @return true once all tabs have been marked as ready to commit or were cancelled.
         */
        public boolean allTabsHandled() {
            return mUnhandledTabs.isEmpty();
        }

        /**
         * @return the list of tabs marked as closing in this event.
         */
        public LinkedList<Tab> getList() {
            return mClosingTabs;
        }
    }

    private class RewoundList implements TabList {
        /**
         * A list of {@link Tab}s that represents the completely rewound list (if all
         * rewindable closes were undone). If there are no possible rewindable closes this list
         * should match {@link #mTabs}.
         */
        private final List<Tab> mRewoundTabs = new ArrayList<Tab>();

        @Override
        public boolean isIncognito() {
            return mTabList.isIncognito();
        }

        /**
         * If {@link TabList} has a valid selected tab, this will return that same tab in the
         * context of the rewound list of tabs.  If {@link TabList} has no tabs but the rewound
         * list is not empty, it will return 0, the first tab.  Otherwise it will return
         * {@link TabList#INVALID_TAB_INDEX}.
         * @return The selected index of the rewound list of tabs (includes all pending closures).
         */
        @Override
        public int index() {
            if (mTabList.index() != INVALID_TAB_INDEX) {
                return mRewoundTabs.indexOf(TabModelUtils.getCurrentTab(mTabList));
            }
            if (!mRewoundTabs.isEmpty()) return 0;
            return INVALID_TAB_INDEX;
        }

        @Override
        public int getCount() {
            return mRewoundTabs.size();
        }

        @Override
        public Tab getTabAt(int index) {
            if (index < 0 || index >= mRewoundTabs.size()) return null;
            return mRewoundTabs.get(index);
        }

        @Override
        public int indexOf(Tab tab) {
            return mRewoundTabs.indexOf(tab);
        }

        /**
         * Resets this list to match the original {@link TabList}.  Note that if the
         * {@link TabList} doesn't support pending closures this model will be empty.  This should
         * be called whenever {@link TabList}'s list of tabs changes.
         */
        public void resetRewoundState() {
            mRewoundTabs.clear();

            for (int i = 0; i < mTabList.getCount(); i++) {
                mRewoundTabs.add(mTabList.getTabAt(i));
            }
        }

        /**
         * Finds the {@link Tab} specified by {@code tabId} and only returns it if it is
         * actually a {@link Tab} that is in the middle of being closed (which means that it
         * is present in this model but not in {@code mTabList}.
         *
         * @param tabId The id of the {@link Tab} to search for.
         * @return The {@link Tab} specified by {@code tabId} as long as that tab only exists
         *         in this model and not in {@code mTabList}. {@code null} otherwise.
         */
        public Tab getPendingRewindTab(int tabId) {
            if (TabModelUtils.getTabById(mTabList, tabId) != null) return null;
            return TabModelUtils.getTabById(this, tabId);
        }

        /**
         * Removes a {@link Tab} from this internal list.
         * @param tab The {@link Tab} to remove.
         * @return whether the tab was removed.
         */
        public boolean removeTab(Tab tab) {
            return mRewoundTabs.remove(tab);
        }

        /**
         * Destroy all tabs in this model.  This will check to see if the tab is already destroyed
         * before destroying it.
         */
        public void destroy() {
            // All tabs pending closure are committed in TabModelImpl#destroy.
            for (Tab tab : mRewoundTabs) {
                if (tab.isInitialized()) tab.destroy();
            }
            mRewoundTabs.clear();
        }

        public boolean hasPendingClosures() {
            return mRewoundTabs.size() > mTabList.getCount();
        }
    }

    /**
     * The {@link TabList} that this {@link PendingTabClosureManager} operates on.
     */
    private TabList mTabList;
    private PendingTabClosureDelegate mDelegate;

    /**
     * Representation of a set of tabs that were closed together.
     */
    private LinkedList<TabClosureEvent> mTabClosureEvents = new LinkedList<>();

    /**
     * A {@link TabList} that represents the complete list of {@link Tab}s. This is so that
     * certain UI elements can call {@link TabModel#getComprehensiveModel()} to get a full list of
     * {@link Tab}s that includes rewindable entries, as the typical {@link TabModel} does not
     * return rewindable entries.
     */
    private final RewoundList mRewoundList = new RewoundList();

    /**
     * @param tabList The {@link TabList} that this manages closing for.
     * @param delegate A {@link PendingTabClosureDelegate} to use to apply cancelled and committed
     *                 tab closures.
     */
    public PendingTabClosureManager(
            @NonNull TabList tabList, @NonNull PendingTabClosureDelegate delegate) {
        assert tabList != null;
        assert delegate != null;

        mTabList = tabList;
        mDelegate = delegate;
    }

    public void destroy() {
        mRewoundList.destroy();
        mTabClosureEvents.clear();
    }

    public void destroyWhileReparentingInProgress() {
        mTabClosureEvents.clear();
    }

    /**
     * Resets the state of the rewound list based on {@code mTabList}.
     */
    public void resetState() {
        assert mTabClosureEvents.isEmpty();
        mRewoundList.resetRewoundState();
    }

    /**
     * Creates a new closure event when pending tabs are closed.
     * @param tabs The list of {@link Tab} that are closing.
     */
    public void addTabClosureEvent(List<Tab> tabs) {
        mTabClosureEvents.add(new TabClosureEvent(tabs));
    }

    /**
     * @return the list of rewindable tabs.
     */
    public TabList getRewoundList() {
        return mRewoundList;
    }

    /**
     * @param tabId The ID of the {@link Tab} to search for.
     * @return whether the list of rewindable tabs contains {@code tabId}.
     */
    public boolean isClosurePending(int tabId) {
        return mRewoundList.getPendingRewindTab(tabId) != null;
    }

    /**
     * Marks a {@link Tab} as ready to commit. If it is the last tab of a {@link TabClosureEvent}
     * to be "ready to commit" then the {@link TabClosureEvent} will commit all tabs as closed.
     * @param tabId The ID of the {@link Tab} to mark as ready to commit.
     */
    public void commitTabClosure(int tabId) {
        Tab tab = mRewoundList.getPendingRewindTab(tabId);
        if (tab == null) return;

        ListIterator<TabClosureEvent> events = mTabClosureEvents.listIterator();
        while (events.hasNext()) {
            TabClosureEvent event = events.next();
            if (!event.markReadyToCommit(tab)) continue;

            if (event.allTabsHandled()) {
                events.remove();
                commitClosuresInternal(event.getList());
            }
            break;
        }
    }

    /**
     * Marks a {@link Tab} as cancelled and restores it to the {@code mTabList}.
     * @param tabId The ID of the {@link Tab} to cancel the closure of.
     */
    public void cancelTabClosure(int tabId) {
        Tab tab = mRewoundList.getPendingRewindTab(tabId);
        if (tab == null) return;

        ListIterator<TabClosureEvent> events = mTabClosureEvents.listIterator();
        while (events.hasNext()) {
            TabClosureEvent event = events.next();
            if (!event.markCancelled(tab)) continue;

            // Bulk undoing closures is messy so just do it one by one.
            cancelClosureInternal(tab);

            // Remove the event once all tabs in it are gone.
            if (event.allTabsHandled()) {
                events.remove();
            }
            break;
        }
    }

    /**
     * Notify observers about completion of undo action to restore all tabs.
     */
    public void notifyAllTabsClosureUndone() {
        mDelegate.notifyAllTabsClosureUndone();
    }

    /**
     * Commits all tab closures in the order in which {@link #addTabClosureEvent(List<Tab>)} was
     * called.
     */
    public void commitAllTabClosures() {
        ListIterator<TabClosureEvent> events = mTabClosureEvents.listIterator();
        while (events.hasNext()) {
            TabClosureEvent event = events.next();
            events.remove();
            // This calls notifyOnFinishingMultipleTabClosure once per TabClosureEvent. This is
            // intended so that tabs closed as distinct events are recorded as such.
            commitClosuresInternal(event.getList());
        }
        assert mTabClosureEvents.isEmpty();
        assert !mRewoundList.hasPendingClosures();
    }

    /**
     * Reverses the most recent {@link #addTabClosureEvent(List<Tab>)} call that hasn't been fully
     * committed or fully cancelled.
     * Caveats:
     * - If any tab closures were cancelled and are in the most recent {@link TabClosureEvent}
     *   those tabs will not be opened again.
     * - If any tab closure were marked as ready to commit but the associated
     *   {@link TabClosureEvent} has not committed then all tab closures for that event will be
     *   opened as the assumption is the most recent close event was desired to be undone.
     */
    boolean openMostRecentlyClosedEntry() {
        if (mTabClosureEvents.isEmpty()) return false;

        TabClosureEvent event = mTabClosureEvents.removeLast();
        for (Tab tab : event.getList()) {
            cancelClosureInternal(tab);
        }
        return true;
    }

    private void commitClosuresInternal(List<Tab> tabs) {
        // Remove tabs first to prevent additional commit attempts in response to closing e.g.
        // UndoBarController when dismissing snackbars. This avoids re-entrancy issues when
        // closing all due to checks at commitTabClosure. This requires all accesses are on the UI
        // thread, but this is already a requirement of TabModelImpl.
        for (Tab tab : tabs) {
            boolean removed = mRewoundList.removeTab(tab);
            // Tabs shouldn't be removed more than once.
            assert removed;
        }
        mDelegate.notifyOnFinishingMultipleTabClosure(tabs);
        for (Tab tab : tabs) {
            mDelegate.finalizeClosure(tab);
        }
    }

    private void cancelClosureInternal(Tab tab) {
        tab.setClosing(false);

        // Find a valid previous tab entry so we know what tab to insert after.  With the following
        // example, calling cancelTabClosure(4) would need to know to insert after 2.  So we have to
        // track across mRewoundTabs and mTabList and see what the last valid mTabList entry was
        // (2) when we hit the 4 in the rewound list.  An insertIndex of -1 represents the beginning
        // of the list, as this is the index of tab to insert after.
        // mTabList:   0   2     5
        // mRewoundTabs 0 1 2 3 4 5
        int prevIndex = -1;
        final int stopIndex = mRewoundList.indexOf(tab);
        for (int rewoundIndex = 0; rewoundIndex < stopIndex; rewoundIndex++) {
            Tab rewoundTab = mRewoundList.getTabAt(rewoundIndex);
            if (prevIndex == mTabList.getCount() - 1) break;
            if (rewoundTab == mTabList.getTabAt(prevIndex + 1)) prevIndex++;
        }

        // Figure out where to insert the tab.  Just add one to prevIndex, as -1 represents the
        // beginning of the list, so we'll insert at 0.
        int insertIndex = prevIndex + 1;
        mDelegate.insertUndoneTabClosureAt(tab, insertIndex);
    }
}
