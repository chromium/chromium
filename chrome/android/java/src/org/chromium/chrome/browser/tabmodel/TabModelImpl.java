// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.List;

/**
 * This is the implementation of the synchronous {@link TabModel} for the
 * {@link ChromeTabbedActivity}.
 */
public class TabModelImpl extends TabModelJniBridge {
    /**
     * The application ID used for tabs opened from an application that does not specify an app ID
     * in its VIEW intent extras.
     */
    public static final String UNKNOWN_APP_ID = "com.google.android.apps.chrome.unknown_app";

    /**
     * The main list of tabs.  Note that when this changes, all pending closures must be committed
     * via {@link #commitAllTabClosures()} as the indices are no longer valid. Also
     * {@link RewoundList#resetRewoundState()} must be called so that the full model will be up to
     * date.
     */
    private final List<Tab> mTabs = new ArrayList<Tab>();

    private final TabCreator mRegularTabCreator;
    private final TabCreator mIncognitoTabCreator;
    private final TabModelOrderController mOrderController;
    private final TabContentManager mTabContentManager;
    private final TabModelDelegate mModelDelegate;
    private final ObserverList<TabModelObserver> mObservers;
    private final NextTabPolicySupplier mNextTabPolicySupplier;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private RecentlyClosedBridge mRecentlyClosedBridge;

    // Undo State Tracking -------------------------------------------------------------------------

    /**
     * A {@link TabList} that represents the complete list of {@link Tab}s. This is so that
     * certain UI elements can call {@link TabModel#getComprehensiveModel()} to get a full list of
     * {@link Tab}s that includes rewindable entries, as the typical {@link TabModel} does not
     * return rewindable entries.
     */
    private final RewoundList mRewoundList = new RewoundList();

    /**
     * This specifies the current {@link Tab} in {@link #mTabs}.
     */
    private int mIndex = INVALID_TAB_INDEX;

    /**
     * Whether this tab model supports undoing.
     */
    private boolean mIsUndoSupported = true;
    private boolean mActive;

    public TabModelImpl(@NonNull Profile profile, @ActivityType int activityType,
            TabCreator regularTabCreator, TabCreator incognitoTabCreator,
            TabModelOrderController orderController, TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager, TabModelDelegate modelDelegate,
            boolean supportUndo) {
        super(profile, activityType);
        mRegularTabCreator = regularTabCreator;
        mIncognitoTabCreator = incognitoTabCreator;
        mOrderController = orderController;
        mTabContentManager = tabContentManager;
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mModelDelegate = modelDelegate;
        mIsUndoSupported = supportUndo;
        mObservers = new ObserverList<TabModelObserver>();
        // The call to initializeNative() should be as late as possible, as it results in calling
        // observers on the native side, which may in turn call |addObserver()| on this object.
        initializeNative(profile);
        mRecentlyClosedBridge = new RecentlyClosedBridge(profile);
    }

    @Override
    public void removeTab(Tab tab) {
        removeTabAndSelectNext(tab, null, TabSelectionType.FROM_CLOSE, false, true);

        for (TabModelObserver obs : mObservers) obs.tabRemoved(tab);
    }

    @Override
    public void destroy() {
        for (Tab tab : mTabs) {
            // When reparenting tabs, we skip destoying tabs that we're intentionally keeping in
            // memory.
            if (mModelDelegate.isReparentingInProgress()
                    && mAsyncTabParamsManager.hasParamsForTabId(tab.getId())) {
                continue;
            }

            if (tab.isInitialized()) tab.destroy();
        }

        mRewoundList.destroy();
        mTabs.clear();
        mObservers.clear();
        mRecentlyClosedBridge.destroy();
        super.destroy();
    }

    @Override
    public void broadcastSessionRestoreComplete() {
        super.broadcastSessionRestoreComplete();

        // This is to make sure TabModel has a valid index when it has at least one valid Tab after
        // TabState is initialized. Otherwise, TabModel can have an invalid index even though it has
        // valid tabs, if the TabModel becomes active before any Tab is restored to that model.
        if (hasValidTab() && mIndex == INVALID_TAB_INDEX) {
            // Actually select the first tab if it is the active model, otherwise just set mIndex.
            if (isActiveModel()) {
                TabModelUtils.setIndex(this, 0);
            } else {
                mIndex = 0;
            }
        }

        for (TabModelObserver observer : mObservers) observer.restoreCompleted();
    }

    @Override
    public void addObserver(TabModelObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Initializes the newly created tab, adds it to controller, and dispatches creation
     * step notifications.
     */
    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        try {
            TraceEvent.begin("TabModelImpl.addTab");

            for (TabModelObserver obs : mObservers) obs.willAddTab(tab, type);

            boolean selectTab = mOrderController.willOpenInForeground(type, isIncognito());

            index = mOrderController.determineInsertionIndex(type, index, tab);
            assert index <= mTabs.size();

            if (tab.isIncognito() != isIncognito()) {
                throw new IllegalStateException("Attempting to open tab in wrong model");
            }

            // TODO(dtrainor): Update the list of undoable tabs instead of committing it.
            commitAllTabClosures();

            if (index < 0 || index > mTabs.size()) {
                mTabs.add(tab);
            } else {
                mTabs.add(index, tab);
                if (index <= mIndex) {
                    mIndex++;
                }
            }

            if (!isActiveModel()) {
                // When adding new tabs in the background, make sure we set a valid index when the
                // first one is added.  When in the foreground, calls to setIndex will take care of
                // this.
                mIndex = Math.max(mIndex, 0);
            }

            mRewoundList.resetRewoundState();

            int newIndex = indexOf(tab);
            tabAddedToModel(tab);

            for (TabModelObserver obs : mObservers) obs.didAddTab(tab, type, creationState);

            // setIndex takes care of making sure the appropriate model is active.
            if (selectTab) setIndex(newIndex, TabSelectionType.FROM_NEW);
        } finally {
            TraceEvent.end("TabModelImpl.addTab");
        }
    }

    @Override
    public void moveTab(int id, int newIndex) {
        newIndex = MathUtils.clamp(newIndex, 0, mTabs.size());

        int curIndex = TabModelUtils.getTabIndexById(this, id);

        if (curIndex == INVALID_TAB_INDEX || curIndex == newIndex || curIndex + 1 == newIndex) {
            return;
        }

        // TODO(dtrainor): Update the list of undoable tabs instead of committing it.
        commitAllTabClosures();

        Tab tab = mTabs.remove(curIndex);
        if (curIndex < newIndex) --newIndex;

        mTabs.add(newIndex, tab);

        if (curIndex == mIndex) {
            mIndex = newIndex;
        } else if (curIndex < mIndex && newIndex >= mIndex) {
            --mIndex;
        } else if (curIndex > mIndex && newIndex <= mIndex) {
            ++mIndex;
        }

        mRewoundList.resetRewoundState();

        for (TabModelObserver obs : mObservers) obs.didMoveTab(tab, newIndex, curIndex);
    }

    @Override
    public boolean closeTab(Tab tab) {
        return closeTab(tab, true, false, false);
    }

    private Tab findTabInAllTabModels(int tabId) {
        Tab tab = TabModelUtils.getTabById(mModelDelegate.getModel(isIncognito()), tabId);
        if (tab != null) return tab;
        return TabModelUtils.getTabById(mModelDelegate.getModel(!isIncognito()), tabId);
    }

    @Override
    public Tab getNextTabIfClosed(int id) {
        Tab tabToClose = TabModelUtils.getTabById(this, id);
        Tab currentTab = TabModelUtils.getCurrentTab(this);
        if (tabToClose == null) return currentTab;

        int closingTabIndex = indexOf(tabToClose);
        Tab adjacentTab = getTabAt((closingTabIndex == 0) ? 1 : closingTabIndex - 1);
        Tab parentTab =
                findTabInAllTabModels(CriticalPersistedTabData.from(tabToClose).getParentId());

        // Determine which tab to select next according to these rules:
        //   * If closing a background tab, keep the current tab selected.
        //   * Otherwise, if not in overview mode, select the parent tab if it exists.
        //   * Otherwise, select an adjacent tab if one exists.
        //   * Otherwise, if closing the last incognito tab, select the current normal tab.
        //   * Otherwise, select nothing.
        Tab nextTab = null;
        if (!isActiveModel()) {
            nextTab = TabModelUtils.getCurrentTab(mModelDelegate.getCurrentModel());
        } else if (tabToClose != currentTab && currentTab != null && !currentTab.isClosing()) {
            nextTab = currentTab;
        } else if (parentTab != null && !parentTab.isClosing()
                && mNextTabPolicySupplier.get() == NextTabPolicy.HIERARCHICAL) {
            nextTab = parentTab;
        } else if (adjacentTab != null && !adjacentTab.isClosing()) {
            nextTab = adjacentTab;
        } else if (isIncognito()) {
            nextTab = TabModelUtils.getCurrentTab(mModelDelegate.getModel(false));
        }

        return nextTab != null && nextTab.isClosing() ? null : nextTab;
    }

    @Override
    public boolean isClosurePending(int tabId) {
        return mRewoundList.getPendingRewindTab(tabId) != null;
    }

    @Override
    public boolean supportsPendingClosures() {
        return !isIncognito() && mIsUndoSupported;
    }

    @Override
    public TabList getComprehensiveModel() {
        if (!supportsPendingClosures()) return this;
        return mRewoundList;
    }

    @Override
    public void cancelTabClosure(int tabId) {
        Tab tab = mRewoundList.getPendingRewindTab(tabId);
        if (tab == null) return;

        tab.setClosing(false);

        // Find a valid previous tab entry so we know what tab to insert after.  With the following
        // example, calling cancelTabClosure(4) would need to know to insert after 2.  So we have to
        // track across mRewoundTabs and mTabs and see what the last valid mTabs entry was (2) when
        // we hit the 4 in the rewound list.  An insertIndex of -1 represents the beginning of the
        // list, as this is the index of tab to insert after.
        // mTabs:       0   2     5
        // mRewoundTabs 0 1 2 3 4 5
        int prevIndex = -1;
        final int stopIndex = mRewoundList.indexOf(tab);
        for (int rewoundIndex = 0; rewoundIndex < stopIndex; rewoundIndex++) {
            Tab rewoundTab = mRewoundList.getTabAt(rewoundIndex);
            if (prevIndex == mTabs.size() - 1) break;
            if (rewoundTab == mTabs.get(prevIndex + 1)) prevIndex++;
        }

        // Figure out where to insert the tab.  Just add one to prevIndex, as -1 represents the
        // beginning of the list, so we'll insert at 0.
        int insertIndex = prevIndex + 1;
        if (mIndex >= insertIndex) mIndex++;
        mTabs.add(insertIndex, tab);

        WebContents webContents = tab.getWebContents();
        if (webContents != null) webContents.setAudioMuted(false);

        boolean activeModel = isActiveModel();

        if (mIndex == INVALID_TAB_INDEX) {
            // If we're the active model call setIndex to actually select this tab, otherwise just
            // set mIndex but don't kick off everything that happens when calling setIndex().
            if (activeModel) {
                TabModelUtils.setIndex(this, insertIndex);
            } else {
                mIndex = insertIndex;
            }
        }

        for (TabModelObserver obs : mObservers) obs.tabClosureUndone(tab);
    }

    @Override
    public void commitTabClosure(int tabId) {
        Tab tab = mRewoundList.getPendingRewindTab(tabId);
        if (tab == null) return;

        // We're committing the close, actually remove it from the lists and finalize the closing
        // operation.
        mRewoundList.removeTab(tab);
        finalizeTabClosure(tab, true);
    }

    @Override
    public void commitAllTabClosures() {
        while (mRewoundList.getCount() > mTabs.size()) {
            commitTabClosure(mRewoundList.getNextRewindableTab().getId());
        }

        assert !mRewoundList.hasPendingClosures();

        if (supportsPendingClosures()) {
            for (TabModelObserver obs : mObservers) obs.allTabsClosureCommitted();
        }
    }

    @Override
    public boolean closeTab(Tab tabToClose, boolean animate, boolean uponExit, boolean canUndo) {
        return closeTab(tabToClose, null, animate, uponExit, canUndo, canUndo);
    }

    @Override
    public boolean closeTab(
            Tab tab, Tab recommendedNextTab, boolean animate, boolean uponExit, boolean canUndo) {
        return closeTab(tab, recommendedNextTab, animate, uponExit, canUndo, canUndo);
    }

    /**
     * See TabModel.java documentation for description of other parameters.
     * @param notify Whether or not to notify observers about the pending closure. If this is
     *               {@code true}, {@link #supportsPendingClosures()} is {@code true},
     *               and canUndo is {@code true}, observers will be notified of the pending
     *               closure. Observers will still be notified of a committed/cancelled closure
     *               even if they are not notified of a pending closure to start with.
     */
    private boolean closeTab(Tab tabToClose, Tab recommendedNextTab, boolean animate,
            boolean uponExit, boolean canUndo, boolean notify) {
        if (tabToClose == null) {
            assert false : "Tab is null!";
            return false;
        }

        if (!mTabs.contains(tabToClose)) {
            assert false : "Tried to close a tab from another model!";
            return false;
        }

        canUndo &= supportsPendingClosures();

        startTabClosure(tabToClose, recommendedNextTab, animate, uponExit, canUndo);
        if (notify && canUndo) {
            for (TabModelObserver obs : mObservers) obs.tabPendingClosure(tabToClose);
        }
        if (!canUndo) finalizeTabClosure(tabToClose, false);

        return true;
    }

    @Override
    public void closeMultipleTabs(List<Tab> tabs, boolean canUndo) {
        for (Tab tab : tabs) {
            if (!mTabs.contains(tab)) {
                assert false : "Tried to close a tab from another model!";
                continue;
            }
            tab.setClosing(true);
            closeTab(tab, null, false, false, canUndo, false);
        }
        if (canUndo && supportsPendingClosures()) {
            for (TabModelObserver obs : mObservers) obs.multipleTabsPendingClosure(tabs, false);
        }
    }

    @Override
    public void closeAllTabs() {
        closeAllTabs(true, false);
    }

    @Override
    public void closeAllTabs(boolean allowDelegation, boolean uponExit) {
        for (TabModelObserver obs : mObservers) obs.willCloseAllTabs(isIncognito());

        if (uponExit) {
            commitAllTabClosures();

            for (int i = 0; i < getCount(); i++) getTabAt(i).setClosing(true);
            while (getCount() > 0) TabModelUtils.closeTabByIndex(this, 0);
            return;
        }

        if (allowDelegation && mModelDelegate.closeAllTabsRequest(isIncognito())) return;

        if (HomepageManager.shouldCloseAppWithZeroTabs()) {
            commitAllTabClosures();

            for (int i = 0; i < getCount(); i++) getTabAt(i).setClosing(true);
            while (getCount() > 0) TabModelUtils.closeTabByIndex(this, 0);
            return;
        }

        // TODO(meiliang): This is a band-aid fix, should remove after LayoutManager is able to
        // manage the Grid Tab Switcher.
        // Disable animation if GridTabSwitcher or TabGroup is enabled.
        boolean animate = !TabUiFeatureUtilities.isGridTabSwitcherEnabled();

        closeAllTabs(animate, false, true);
    }

    /**
     * Close all tabs on this model without notifying observers about pending tab closures.
     *
     * @param animate true iff the closing animation should be displayed
     * @param uponExit true iff the tabs are being closed upon application exit (after user presses
     *                 the system back button)
     * @param canUndo Whether or not this action can be undone. If this is {@code true} and
     *                {@link #supportsPendingClosures()} is {@code true}, these {@link Tab}s
     *                will not actually be closed until {@link #commitTabClosure(int)} or
     *                {@link #commitAllTabClosures()} is called, but they will be effectively
     *                removed from this list.
     */
    public void closeAllTabs(boolean animate, boolean uponExit, boolean canUndo) {
        for (int i = 0; i < getCount(); i++) getTabAt(i).setClosing(true);

        List<Tab> closedTabs = new ArrayList<>();
        while (getCount() > 0) {
            Tab tab = getTabAt(0);
            closedTabs.add(tab);
            closeTab(tab, null, animate, uponExit, canUndo, false);
        }

        if (!uponExit && canUndo && supportsPendingClosures()) {
            for (TabModelObserver obs : mObservers) {
                obs.multipleTabsPendingClosure(closedTabs, true);
            }
        }
    }

    @Override
    public Tab getTabAt(int index) {
        // This will catch INVALID_TAB_INDEX and return null
        if (index < 0 || index >= mTabs.size()) return null;
        return mTabs.get(index);
    }

    // Index of the given tab in the order of the tab stack.
    @Override
    public int indexOf(Tab tab) {
        if (tab == null) return INVALID_TAB_INDEX;
        int retVal = mTabs.indexOf(tab);
        return retVal == -1 ? INVALID_TAB_INDEX : retVal;
    }

    // TODO(aurimas): Move this method to TabModelSelector when notifications move there.
    private int getLastId(@TabSelectionType int type) {
        if (type == TabSelectionType.FROM_CLOSE || type == TabSelectionType.FROM_EXIT) {
            return Tab.INVALID_TAB_ID;
        }

        // Get the current tab in the current tab model.
        Tab currentTab = TabModelUtils.getCurrentTab(mModelDelegate.getCurrentModel());
        return currentTab != null ? currentTab.getId() : Tab.INVALID_TAB_ID;
    }

    private boolean hasValidTab() {
        if (mTabs.size() <= 0) return false;
        for (int i = 0; i < mTabs.size(); i++) {
            if (!mTabs.get(i).isClosing()) return true;
        }
        return false;
    }

    // This function is complex and its behavior depends on persisted state, including mIndex.
    @Override
    public void setIndex(int i, final @TabSelectionType int type) {
        try {
            TraceEvent.begin("TabModelImpl.setIndex");
            int lastId = getLastId(type);

            // This can cause recursive entries into setIndex, which causes duplicate notifications
            // and UMA records.
            if (!isActiveModel()) mModelDelegate.selectModel(isIncognito());

            if (!hasValidTab()) {
                mIndex = INVALID_TAB_INDEX;
            } else {
                mIndex = MathUtils.clamp(i, 0, mTabs.size() - 1);
            }

            Tab tab = TabModelUtils.getCurrentTab(this);

            mModelDelegate.requestToShowTab(tab, type);

            if (tab != null) {
                for (TabModelObserver obs : mObservers) obs.didSelectTab(tab, type, lastId);

                boolean wasAlreadySelected = tab.getId() == lastId;
                if (!wasAlreadySelected && type == TabSelectionType.FROM_USER) {
                    // We only want to record when the user actively switches to a different tab.
                    RecordUserAction.record("MobileTabSwitched");
                }
            }
        } finally {
            TraceEvent.end("TabModelImpl.setIndex");
        }
    }

    @Override
    public boolean isActiveModel() {
        return mActive;
    }

    /**
     * Performs the necessary actions to remove this {@link Tab} from this {@link TabModel}.
     * This does not actually destroy the {@link Tab} (see
     * {@link #finalizeTabClosure(Tab)}.
     *
     * @param tab The {@link Tab} to remove from this {@link TabModel}.
     * @param animate Whether or not to animate the closing.
     * @param uponExit Whether or not this is closing while the Activity is exiting.
     * @param canUndo Whether or not this operation can be undone. Note that if this is {@code true}
     *                and {@link #supportsPendingClosures()} is {@code true},
     *                {@link #commitTabClosure(int)} or {@link #commitAllTabClosures()} needs to be
     *                called to actually delete and clean up {@code tab}.
     */
    private void startTabClosure(
            Tab tab, Tab recommendedNextTab, boolean animate, boolean uponExit, boolean canUndo) {
        tab.setClosing(true);

        for (TabModelObserver obs : mObservers) obs.willCloseTab(tab, animate);

        @TabSelectionType
        int selectionType = uponExit ? TabSelectionType.FROM_EXIT : TabSelectionType.FROM_CLOSE;
        boolean pauseMedia = canUndo;
        boolean updateRewoundList = !canUndo;
        removeTabAndSelectNext(
                tab, recommendedNextTab, selectionType, pauseMedia, updateRewoundList);
    }

    /**
     * Removes the given tab from the tab model and selects a new tab.
     */
    private void removeTabAndSelectNext(Tab tab, Tab recommendedNextTab,
            @TabSelectionType int selectionType, boolean pauseMedia, boolean updateRewoundList) {
        assert selectionType == TabSelectionType.FROM_CLOSE
                || selectionType == TabSelectionType.FROM_EXIT;

        final int closingTabId = tab.getId();
        final int closingTabIndex = indexOf(tab);

        Tab currentTabInModel = TabModelUtils.getCurrentTab(this);
        Tab adjacentTabInModel = getTabAt(closingTabIndex == 0 ? 1 : closingTabIndex - 1);
        Tab nextTab =
                recommendedNextTab == null ? getNextTabIfClosed(closingTabId) : recommendedNextTab;

        // TODO(dtrainor): Update the list of undoable tabs instead of committing it.
        if (updateRewoundList) commitAllTabClosures();

        // Cancel or mute any media currently playing.
        if (pauseMedia) {
            WebContents webContents = tab.getWebContents();
            if (webContents != null) {
                webContents.suspendAllMediaPlayers();
                webContents.setAudioMuted(true);
            }
        }

        mTabs.remove(tab);

        boolean nextIsIncognito = nextTab == null ? false : nextTab.isIncognito();
        int nextTabId = nextTab == null ? Tab.INVALID_TAB_ID : nextTab.getId();
        int nextTabIndex = nextTab == null ? INVALID_TAB_INDEX : TabModelUtils.getTabIndexById(
                mModelDelegate.getModel(nextIsIncognito), nextTabId);

        if (nextTab != currentTabInModel) {
            if (nextIsIncognito != isIncognito()) mIndex = indexOf(adjacentTabInModel);

            TabModel nextModel = mModelDelegate.getModel(nextIsIncognito);
            nextModel.setIndex(nextTabIndex, selectionType);
        } else {
            mIndex = nextTabIndex;
        }

        if (updateRewoundList) mRewoundList.resetRewoundState();
    }

    /**
     * Actually closes and cleans up {@code tab}.
     * @param tab The {@link Tab} to close.
     * @param notifyTabClosureCommitted If true then observers will receive a tabClosureCommitted
     *     notification.
     */
    private void finalizeTabClosure(Tab tab, boolean notifyTabClosureCommitted) {
        if (mTabContentManager != null) mTabContentManager.removeTabThumbnail(tab.getId());

        for (TabModelObserver obs : mObservers) obs.didCloseTab(tab);
        for (TabModelObserver obs : mObservers) obs.didCloseTab(tab.getId(), tab.isIncognito());
        if (notifyTabClosureCommitted) {
            for (TabModelObserver obs : mObservers) obs.tabClosureCommitted(tab);
        }

        // Destroy the native tab after the observer notifications have fired, otherwise they risk a
        // use after free or null dereference.
        tab.destroy();
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
            return TabModelImpl.this.isIncognito();
        }

        /**
         * If {@link TabModel} has a valid selected tab, this will return that same tab in the
         * context of the rewound list of tabs.  If {@link TabModel} has no tabs but the rewound
         * list is not empty, it will return 0, the first tab.  Otherwise it will return
         * {@link TabModel#INVALID_TAB_INDEX}.
         * @return The selected index of the rewound list of tabs (includes all pending closures).
         */
        @Override
        public int index() {
            if (TabModelImpl.this.index() != INVALID_TAB_INDEX) {
                return mRewoundTabs.indexOf(TabModelUtils.getCurrentTab(TabModelImpl.this));
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

        @Override
        public boolean isClosurePending(int tabId) {
            return TabModelImpl.this.isClosurePending(tabId);
        }

        /**
         * Resets this list to match the original {@link TabModel}.  Note that if the
         * {@link TabModel} doesn't support pending closures this model will be empty.  This should
         * be called whenever {@link #mTabs} changes.
         */
        public void resetRewoundState() {
            mRewoundTabs.clear();

            if (TabModelImpl.this.supportsPendingClosures()) {
                for (int i = 0; i < TabModelImpl.this.getCount(); i++) {
                    mRewoundTabs.add(TabModelImpl.this.getTabAt(i));
                }
            }
        }

        /**
         * Finds the {@link Tab} specified by {@code tabId} and only returns it if it is
         * actually a {@link Tab} that is in the middle of being closed (which means that it
         * is present in this model but not in {@link #mTabs}.
         *
         * @param tabId The id of the {@link Tab} to search for.
         * @return The {@link Tab} specified by {@code tabId} as long as that tab only exists
         *         in this model and not in {@link #mTabs}. {@code null} otherwise.
         */
        public Tab getPendingRewindTab(int tabId) {
            if (!TabModelImpl.this.supportsPendingClosures()) return null;
            if (TabModelUtils.getTabById(TabModelImpl.this, tabId) != null) return null;
            return TabModelUtils.getTabById(this, tabId);
        }

        /**
         * A utility method for easily finding a {@link Tab} that can be closed.
         * @return The next tab that is in the middle of being closed.
         */
        public Tab getNextRewindableTab() {
            if (!hasPendingClosures()) return null;

            for (int i = 0; i < mRewoundTabs.size(); i++) {
                Tab tab = i < TabModelImpl.this.getCount() ? TabModelImpl.this.getTabAt(i) : null;
                Tab rewoundTab = mRewoundTabs.get(i);

                if (tab == null || rewoundTab.getId() != tab.getId()) return rewoundTab;
            }

            return null;
        }

        /**
         * Removes a {@link Tab} from this internal list.
         * @param tab The {@link Tab} to remove.
         */
        public void removeTab(Tab tab) {
            mRewoundTabs.remove(tab);
        }

        /**
         * Destroy all tabs in this model.  This will check to see if the tab is already destroyed
         * before destroying it.
         */
        public void destroy() {
            // All tabs pending closure are committed in TabModelImpl#destroy.
            if (TabModelImpl.this.mModelDelegate.isReparentingInProgress()) return;
            for (Tab tab : mRewoundTabs) {
                if (tab.isInitialized()) tab.destroy();
            }
        }

        public boolean hasPendingClosures() {
            return TabModelImpl.this.supportsPendingClosures()
                    && mRewoundTabs.size() > TabModelImpl.this.getCount();
        }
    }

    @Override
    protected boolean closeTabAt(int index) {
        return closeTab(getTabAt(index));
    }

    @Override
    protected TabCreator getTabCreator(boolean incognito) {
        return incognito ? mIncognitoTabCreator : mRegularTabCreator;
    }

    @Override
    protected boolean createTabWithWebContents(
            Tab parent, Profile profile, WebContents webContents) {
        return getTabCreator(profile.isOffTheRecord())
                .createTabWithWebContents(
                        parent, webContents, TabLaunchType.FROM_LONGPRESS_BACKGROUND);
    }

    @Override
    public void openNewTab(Tab parent, GURL url, @Nullable Origin initiatorOrigin,
            String extraHeaders, ResourceRequestBody postData, int disposition,
            boolean persistParentage, boolean isRendererInitiated) {
        if (parent.isClosing()) return;

        boolean incognito = parent.isIncognito();
        @TabLaunchType
        int tabLaunchType = TabLaunchType.FROM_LONGPRESS_FOREGROUND;

        switch (disposition) {
            case WindowOpenDisposition.NEW_WINDOW: // fall through
            case WindowOpenDisposition.NEW_FOREGROUND_TAB:
                break;
            case WindowOpenDisposition.NEW_POPUP: // fall through
            case WindowOpenDisposition.NEW_BACKGROUND_TAB:
                tabLaunchType = TabLaunchType.FROM_LONGPRESS_BACKGROUND;
                break;
            case WindowOpenDisposition.OFF_THE_RECORD:
                assert incognito;
                incognito = true;
                break;
            default:
                assert false;
        }

        // If shouldIgnoreNewTab returns true, the intent is handled by another
        // activity. As a result, don't launch a new tab to open the URL.
        InterceptNavigationDelegateImpl delegate = InterceptNavigationDelegateTabHelper.get(parent);
        if (delegate != null && delegate.shouldIgnoreNewTab(url, incognito)) return;

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setInitiatorOrigin(initiatorOrigin);
        loadUrlParams.setVerbatimHeaders(extraHeaders);
        loadUrlParams.setPostData(postData);
        loadUrlParams.setIsRendererInitiated(isRendererInitiated);
        getTabCreator(incognito).createNewTab(
                loadUrlParams, tabLaunchType, persistParentage ? parent : null);
    }

    @Override
    public int getCount() {
        return mTabs.size();
    }

    @Override
    public int index() {
        return mIndex;
    }

    @Override
    protected boolean isSessionRestoreInProgress() {
        return mModelDelegate.isSessionRestoreInProgress();
    }

    @Override
    public void openMostRecentlyClosedTab() {
        // First try to recover tab from rewound list, same as {@link UndoBarController}.
        if (mRewoundList.hasPendingClosures()) {
            Tab tab = mRewoundList.getNextRewindableTab();
            if (tab != null) cancelTabClosure(tab.getId());
            return;
        }

        // If there are no pending closures in the rewound list,
        // then try to restore the tab from the native tab restore service.
        mRecentlyClosedBridge.openRecentlyClosedTab();
        // If there is only one tab, select it.
        if (getCount() == 1) setIndex(0, TabSelectionType.FROM_NEW);
    }

    @Override
    public void setActive(boolean active) {
        mActive = active;
    }
}
