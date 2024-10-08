// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.PendingTabClosureManager.PendingTabClosureDelegate;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This is the implementation of the synchronous {@link TabModel} for the {@link
 * ChromeTabbedActivity}.
 */
public class TabModelImpl extends TabModelJniBridge {
    /**
     * The application ID used for tabs opened from an application that does not specify an app ID
     * in its VIEW intent extras.
     */
    public static final String UNKNOWN_APP_ID = "com.google.android.apps.chrome.unknown_app";

    /**
     * The main list of tabs. Note that when this changes, all pending closures must be committed
     * via {@link #commitAllTabClosures()} as the indices are no longer valid. Also {@link
     * PendingTabClosureManager#resetState()} must be called so that the full model will be up to
     * date.
     */
    private final List<Tab> mTabs = new ArrayList<>();

    // Allows efficient lookup by tab id instead of index.
    private final Map<Integer, Tab> mTabIdToTabs = new HashMap<>();

    private final TabCreator mRegularTabCreator;
    private final TabCreator mIncognitoTabCreator;
    private final TabModelOrderController mOrderController;
    private final TabContentManager mTabContentManager;
    private final TabModelDelegate mModelDelegate;
    private final ObserverList<TabModelObserver> mObservers;
    private final NextTabPolicySupplier mNextTabPolicySupplier;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>();

    /** This specifies the current {@link Tab} in {@link #mTabs}. */
    private int mIndex = INVALID_TAB_INDEX;

    private boolean mActive;

    // Undo State Tracking -------------------------------------------------------------------------

    private PendingTabClosureManager mPendingTabClosureManager;

    /**
     * Implementation of {@link PendingTabClosureDelegate} that has access to the internal state of
     * TabModelImpl.
     */
    private class PendingTabClosureDelegateImpl implements PendingTabClosureDelegate {
        @Override
        public void insertUndoneTabClosureAt(Tab tab, int insertIndex) {
            if (mIndex >= insertIndex) mIndex++;
            assert !tab.isDestroyed() : "Attempting to undo tab that is destroyed.";
            mTabs.add(insertIndex, tab);
            mTabIdToTabs.put(tab.getId(), tab);
            mTabCountSupplier.set(mTabs.size());

            WebContents webContents = tab.getWebContents();
            if (webContents != null) webContents.setAudioMuted(false);

            // Start by setting a valid index to the restored tab if not already valid. This ensures
            // getting the current index is valid for any observers.
            boolean wasInvalidIndex = mIndex == INVALID_TAB_INDEX;
            if (wasInvalidIndex) {
                mIndex = insertIndex;
            }

            // Alert observers the tab closure was undone before calling setIndex if necessary as
            // * Observers may rely on this signal to re-introduce the tab to their visibility if it
            //   is selected before this it may not exist for those observers.
            // * UndoRefocusHelper may update the index out-of-band.
            for (TabModelObserver obs : mObservers) {
                obs.tabClosureUndone(tab);
            }

            // If the mIndex we set earlier is still in use then trigger a proper index update and
            // notify any observers.
            if (wasInvalidIndex && isActiveModel() && mIndex == insertIndex) {
                // Reset the index first so the event is raised properly as a index change and not
                // re-using the current index.
                mIndex = INVALID_TAB_INDEX;
                TabModelUtils.setIndex(TabModelImpl.this, insertIndex, TabSelectionType.FROM_UNDO);
            } else if (wasInvalidIndex && !isActiveModel()) {
                mCurrentTabSupplier.set(TabModelUtils.getCurrentTab(TabModelImpl.this));
            }
        }

        @Override
        public void finalizeClosure(Tab tab) {
            finalizeTabClosure(tab, true);
        }

        @Override
        public void notifyAllTabsClosureUndone() {
            for (TabModelObserver obs : mObservers) {
                obs.allTabsClosureUndone();
            }
        }

        @Override
        public void notifyOnFinishingMultipleTabClosure(List<Tab> tabs) {
            TabModelImpl.this.notifyOnFinishingMultipleTabClosure(
                    tabs, /* saveToTabRestoreService= */ true);
        }

        @Override
        public void notifyOnCancelingTabClosure(@Nullable Runnable undoRunnable) {
            if (undoRunnable != null) {
                undoRunnable.run();
            }
        }
    }

    public TabModelImpl(
            @NonNull Profile profile,
            @ActivityType int activityType,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            @NonNull TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            TabModelDelegate modelDelegate,
            boolean supportUndo,
            boolean isArchivedTabModel) {
        super(profile, activityType, isArchivedTabModel);
        mRegularTabCreator = regularTabCreator;
        mIncognitoTabCreator = incognitoTabCreator;
        mOrderController = orderController;
        mTabContentManager = tabContentManager;
        assert mTabContentManager != null;
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mModelDelegate = modelDelegate;
        mTabCountSupplier.set(0);
        if (supportUndo && !isIncognito()) {
            mPendingTabClosureManager =
                    new PendingTabClosureManager(this, new PendingTabClosureDelegateImpl());
        }
        mObservers = new ObserverList<TabModelObserver>();
        // The call to initializeNative() should be as late as possible, as it results in calling
        // observers on the native side, which may in turn call |addObserver()| on this object.
        initializeNative(profile);
    }

    @Override
    public void removeTab(Tab tab) {
        removeTabAndSelectNext(
                tab, null, TabSelectionType.FROM_CLOSE, false, true, TabCloseType.SINGLE);

        for (TabModelObserver obs : mObservers) obs.tabRemoved(tab);
    }

    @Override
    public void destroy() {
        commitAllTabClosures();
        for (Tab tab : mTabs) {
            // When reparenting tabs, we skip destroying tabs that we're intentionally keeping in
            // memory.
            if (mModelDelegate.isReparentingInProgress()
                    && mAsyncTabParamsManager.hasParamsForTabId(tab.getId())) {
                continue;
            }

            if (tab.isInitialized()) tab.destroy();
        }

        if (mPendingTabClosureManager != null) {
            if (mModelDelegate.isReparentingInProgress()) {
                mPendingTabClosureManager.destroyWhileReparentingInProgress();
            } else {
                mPendingTabClosureManager.destroy();
            }
        }
        mTabs.clear();
        mTabIdToTabs.clear();
        mTabCountSupplier.set(0);
        mObservers.clear();
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
                mCurrentTabSupplier.set(TabModelUtils.getCurrentTab(this));
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

    @Override
    public @NonNull ObservableSupplier<Integer> getTabCountSupplier() {
        return mTabCountSupplier;
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
            // TODO(crbug.com/40923859): Technically this should trigger NPEs downstream. Adding out
            // of an abundance of caution.
            assert tab != null : "Attempting to add a tab that is null to TabModel.";
            assert !mTabIdToTabs.containsKey(tab.getId())
                    : "Attempting to add a tab with a duplicate id=" + tab.getId();

            for (TabModelObserver obs : mObservers) obs.willAddTab(tab, type);

            boolean selectTab =
                    mOrderController.willOpenInForeground(type, isIncognitoBranded())
                            || (mTabs.size() == 0
                                    && type == TabLaunchType.FROM_LONGPRESS_BACKGROUND);

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
            mTabIdToTabs.put(tab.getId(), tab);
            mTabCountSupplier.set(mTabs.size());

            if (!isActiveModel()) {
                // When adding new tabs in the background, make sure we set a valid index when the
                // first one is added. When in the foreground, calls to setIndex will take care of
                // this.
                mIndex = Math.max(mIndex, 0);
                if (!selectTab) {
                    mCurrentTabSupplier.set(TabModelUtils.getCurrentTab(this));
                }
            }

            if (supportsPendingClosures()) {
                mPendingTabClosureManager.resetState();
            }

            int newIndex = indexOf(tab);
            tabAddedToModel(tab);

            for (TabModelObserver obs : mObservers) {
                obs.didAddTab(tab, type, creationState, selectTab);
            }

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

        assert tab != null : "Attempting to move a tab that is null.";
        mTabs.add(newIndex, tab);

        if (curIndex == mIndex) {
            mIndex = newIndex;
        } else if (curIndex < mIndex && newIndex >= mIndex) {
            --mIndex;
        } else if (curIndex > mIndex && newIndex <= mIndex) {
            ++mIndex;
        }

        if (supportsPendingClosures()) {
            mPendingTabClosureManager.resetState();
        }

        for (TabModelObserver obs : mObservers) obs.didMoveTab(tab, newIndex, curIndex);
    }

    private Tab findTabInAllTabModels(int tabId) {
        Tab tab = mModelDelegate.getModel(isIncognito()).getTabById(tabId);
        if (tab != null) return tab;
        return mModelDelegate.getModel(!isIncognito()).getTabById(tabId);
    }

    private Tab findNearbyNotClosingTab(int closingIndex) {
        if (closingIndex > 0) {
            // Search for the first tab before the closing tab.
            for (int i = closingIndex - 1; i >= 0; i--) {
                Tab tab = getTabAt(i);
                if (!tab.isClosing()) {
                    return tab;
                }
            }
        }
        // If this is the first tab or all tabs before the closing tab are closed then search the
        // other direction.
        for (int i = closingIndex + 1; i < mTabs.size(); i++) {
            Tab tab = getTabAt(i);
            if (!tab.isClosing()) {
                return tab;
            }
        }
        return null;
    }

    @Override
    public Tab getNextTabIfClosed(int id, boolean uponExit) {
        return getNextTabIfClosed(id, uponExit, TabCloseType.SINGLE);
    }

    /**
     * See public getNextTabIfClosed documentation
     *
     * @param tabCloseType the type of tab closure occurring. This is used to avoid searching for a
     *     nearby tab when closing all tabs.
     */
    private Tab getNextTabIfClosed(int id, boolean uponExit, @TabCloseType int tabCloseType) {
        Tab tabToClose = getTabById(id);
        Tab currentTab = TabModelUtils.getCurrentTab(this);
        if (tabToClose == null) return currentTab;

        final boolean useCurrentTab =
                tabToClose != currentTab && currentTab != null && !currentTab.isClosing();

        int closingTabIndex = indexOf(tabToClose);
        Tab nearbyTab = null;
        if (tabCloseType != TabCloseType.ALL && !useCurrentTab) {
            nearbyTab = findNearbyNotClosingTab(closingTabIndex);
        }
        Tab parentTab = findTabInAllTabModels(tabToClose.getParentId());
        Tab nextMostRecentTab = null;
        if (uponExit) {
            nextMostRecentTab = TabModelUtils.getMostRecentTab(this, id);
        }

        // Determine which tab to select next according to these rules:
        //   * If closing a background tab, keep the current tab selected.
        //   * Otherwise, if closing the tab upon exit select the next most recent tab.
        //   * Otherwise, if not in overview mode, select the parent tab if it exists.
        //   * Otherwise, select a nearby tab if one exists.
        //   * Otherwise, if closing the last incognito tab, select the current normal tab.
        //   * Otherwise, select nothing.
        Tab nextTab = null;
        if (!isActiveModel()) {
            nextTab = TabModelUtils.getCurrentTab(mModelDelegate.getCurrentModel());
        } else if (useCurrentTab) {
            nextTab = currentTab;
        } else if (nextMostRecentTab != null && !nextMostRecentTab.isClosing()) {
            nextTab = nextMostRecentTab;
        } else if (parentTab != null
                && !parentTab.isClosing()
                && mNextTabPolicySupplier.get() == NextTabPolicy.HIERARCHICAL) {
            nextTab = parentTab;
        } else if (nearbyTab != null && !nearbyTab.isClosing()) {
            nextTab = nearbyTab;
        } else if (isIncognito()) {
            nextTab = TabModelUtils.getCurrentTab(mModelDelegate.getModel(false));
        }

        return nextTab != null && nextTab.isClosing() ? null : nextTab;
    }

    @Override
    public boolean isClosurePending(int tabId) {
        if (!supportsPendingClosures()) return false;

        return mPendingTabClosureManager.isClosurePending(tabId);
    }

    @Override
    public boolean supportsPendingClosures() {
        assert mPendingTabClosureManager == null || !isIncognito();
        return mPendingTabClosureManager != null;
    }

    @Override
    public TabList getComprehensiveModel() {
        if (!supportsPendingClosures()) return this;
        return mPendingTabClosureManager.getRewoundList();
    }

    @Override
    public void cancelTabClosure(int tabId) {
        if (!supportsPendingClosures()) return;

        mPendingTabClosureManager.cancelTabClosure(tabId);
    }

    @Override
    public void commitTabClosure(int tabId) {
        if (!supportsPendingClosures()) return;

        mPendingTabClosureManager.commitTabClosure(tabId);
    }

    @Override
    public void commitAllTabClosures() {
        if (!supportsPendingClosures()) return;

        mPendingTabClosureManager.commitAllTabClosures();

        for (TabModelObserver obs : mObservers) obs.allTabsClosureCommitted(isIncognito());
    }

    @Override
    public void notifyAllTabsClosureUndone() {
        if (!supportsPendingClosures()) return;

        mPendingTabClosureManager.notifyAllTabsClosureUndone();
    }

    /**
     * @param tabToClose The tab being closed.
     * @param recommendedNextTab The tab to select next unless some condition overrides it.
     * @param uponExit Whether the app is closing as a result of this tab closing.
     * @param allowUndo Whether undo of the closure is allowed.
     * @param notifyPending Whether or not to notify observers about the pending closure. If this is
     *     {@code true}, {@link #supportsPendingClosures()} is {@code true}, and allowUndo is {@code
     *     true}, observers will be notified of the pending closure. Observers will still be
     *     notified of a committed/cancelled closure even if they are not notified of a pending
     *     closure to start with.
     * @param tabCloseType Used to notify observers that this tab is closing by itself for {@link
     *     TabModelObserver#onFinishingMultipleTabClosure} if the closure cannot be undone and for
     *     {@link TabModelObserver#willCloseTab}. This should be {@code TabCloseType.SINGLE} if
     *     closing the tab by itself, {@code TabCloseType.MULTIPLE} if closing multiple tabs or
     *     {@code TabCloseType.ALL} all tabs (which also does additional optimization).
     * @return true if the closure succeeds and false otherwise.
     */
    private boolean closeTab(
            Tab tabToClose,
            Tab recommendedNextTab,
            boolean uponExit,
            boolean allowUndo,
            boolean notifyPending,
            @TabCloseType int tabCloseType) {
        if (tabToClose == null) {
            assert false : "Tab is null!";
            return false;
        }

        if (!containsTab(tabToClose)) {
            assert false : "Tried to close a tab from another model!";
            return false;
        }

        allowUndo &= supportsPendingClosures();

        startTabClosure(tabToClose, recommendedNextTab, uponExit, allowUndo, tabCloseType);
        if (notifyPending && allowUndo) {
            mPendingTabClosureManager.addTabClosureEvent(
                    Collections.singletonList(tabToClose), /* undoRunnable= */ null);
            for (TabModelObserver obs : mObservers) obs.tabPendingClosure(tabToClose);
        }
        if (!allowUndo) {
            if (tabCloseType == TabCloseType.SINGLE) {
                notifyOnFinishingMultipleTabClosure(
                        Collections.singletonList(tabToClose), /* saveToTabRestoreService= */ true);
            }
            finalizeTabClosure(tabToClose, false);
        }
        return true;
    }

    private void closeMultipleTabs(
            List<Tab> tabs, boolean allowUndo, boolean saveToTabRestoreService) {
        assert (!allowUndo || saveToTabRestoreService)
                : "saveToTabRestoreService == false is ignored if allowUndo == true.";

        for (Tab tab : tabs) {
            if (!containsTab(tab)) {
                assert false : "Tried to close a tab from another model!";
                continue;
            }
            tab.setClosing(true);
        }
        allowUndo &= supportsPendingClosures();
        if (!allowUndo) {
            notifyOnFinishingMultipleTabClosure(tabs, saveToTabRestoreService);
        }
        for (TabModelObserver obs : mObservers) obs.willCloseMultipleTabs(allowUndo, tabs);
        for (Tab tab : tabs) {
            closeTab(tab, null, false, allowUndo, false, TabCloseType.MULTIPLE);
        }
        if (allowUndo) {
            mPendingTabClosureManager.addTabClosureEvent(tabs, /* undoRunnable= */ null);
            for (TabModelObserver obs : mObservers) obs.multipleTabsPendingClosure(tabs, false);
        }
    }

    private void closeAllTabs(boolean uponExit, @Nullable Runnable undoRunnable) {
        for (TabModelObserver obs : mObservers) obs.willCloseAllTabs(isIncognito());

        // Force close immediately upon exit or if Chrome needs to close with a zero-state.
        if (uponExit || HomepageManager.getInstance().shouldCloseAppWithZeroTabs()) {
            commitAllTabClosures();

            for (int i = 0; i < getCount(); i++) getTabAt(i).setClosing(true);
            notifyOnFinishingMultipleTabClosure(mTabs, /* saveToTabRestoreService= */ true);
            while (getCount() > 0) {
                Tab tab = getTabAt(0);
                closeTab(tab, null, uponExit, false, false, TabCloseType.ALL);
            }
            return;
        }

        // Close with the opportunity to undo if this TabModel supports pending closures.
        for (int i = 0; i < getCount(); i++) getTabAt(i).setClosing(true);
        List<Tab> closedTabs = new ArrayList<>(mTabs);
        if (!supportsPendingClosures()) {
            notifyOnFinishingMultipleTabClosure(closedTabs, /* saveToTabRestoreService= */ true);
        }
        while (getCount() > 0) {
            Tab tab = getTabAt(0);
            closeTab(tab, null, false, true, false, TabCloseType.ALL);
        }

        if (supportsPendingClosures()) {
            mPendingTabClosureManager.addTabClosureEvent(closedTabs, undoRunnable);
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

    @Override
    public @Nullable Tab getTabById(int tabId) {
        return mTabIdToTabs.get(tabId);
    }

    @Override
    public boolean closeTabs(TabClosureParams tabClosureParams) {
        // TODO(crbug.com/356445932): Respect the provided params more broadly.
        switch (tabClosureParams.tabCloseType) {
            case TabCloseType.SINGLE:
                assert tabClosureParams.tabs.size() == 1;
                boolean notifyPending = tabClosureParams.allowUndo;
                return closeTab(
                        tabClosureParams.tabs.get(0),
                        tabClosureParams.recommendedNextTab,
                        tabClosureParams.uponExit,
                        tabClosureParams.allowUndo,
                        notifyPending,
                        tabClosureParams.tabCloseType);
            case TabCloseType.MULTIPLE:
                closeMultipleTabs(
                        tabClosureParams.tabs,
                        tabClosureParams.allowUndo,
                        tabClosureParams.saveToTabRestoreService);
                return true;
            case TabCloseType.ALL:
                closeAllTabs(tabClosureParams.uponExit, tabClosureParams.undoRunnable);
                return true;
            default:
                assert false : "Not reached.";
                return false;
        }
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

    private boolean containsTab(Tab tab) {
        return mTabIdToTabs.containsKey(tab.getId());
    }

    @Override
    public ObservableSupplier<Tab> getCurrentTabSupplier() {
        return mCurrentTabSupplier;
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

            mCurrentTabSupplier.set(tab);
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
     * Performs the necessary actions to remove this {@link Tab} from this {@link TabModel}. This
     * does not actually destroy the {@link Tab} (see {@link #finalizeTabClosure(Tab)}.
     *
     * @param tab The {@link Tab} to remove from this {@link TabModel}.
     * @param uponExit Whether or not this is closing while the Activity is exiting.
     * @param allowUndo Whether or not this operation can be undone. Note that if this is {@code
     *     true} and {@link #supportsPendingClosures()} is {@code true}, {@link
     *     #commitTabClosure(int)} or {@link #commitAllTabClosures()} needs to be called to actually
     *     delete and clean up {@code tab}.
     * @param tabCloseType Used to notify observers that this tab is closing by itself for {@link
     *     TabModelObserver#onFinishingMultipleTabClosure} if the closure cannot be undone and for
     *     {@link TabModelObserver#willCloseTab}. This should be {@code TabCloseType.SINGLE} if
     *     closing the tab by itself, {@code TabCloseType.MULTIPLE} if closing multiple tabs or
     *     {@code TabCloseType.ALL} all tabs (which also does additional optimization).
     */
    private void startTabClosure(
            Tab tab,
            Tab recommendedNextTab,
            boolean uponExit,
            boolean allowUndo,
            @TabCloseType int tabCloseType) {
        tab.setClosing(true);

        for (TabModelObserver obs : mObservers) {
            obs.willCloseTab(tab, tabCloseType == TabCloseType.SINGLE);
        }

        @TabSelectionType
        int selectionType = uponExit ? TabSelectionType.FROM_EXIT : TabSelectionType.FROM_CLOSE;
        boolean pauseMedia = allowUndo;
        boolean updatePendingTabClosureManager = !allowUndo;
        removeTabAndSelectNext(
                tab,
                recommendedNextTab,
                selectionType,
                pauseMedia,
                updatePendingTabClosureManager,
                tabCloseType);
    }

    /** Removes the given tab from the tab model and selects a new tab. */
    private void removeTabAndSelectNext(
            Tab tab,
            Tab recommendedNextTab,
            @TabSelectionType int selectionType,
            boolean pauseMedia,
            boolean updatePendingTabClosureManager,
            @TabCloseType int tabCloseType) {
        assert selectionType == TabSelectionType.FROM_CLOSE
                || selectionType == TabSelectionType.FROM_EXIT;

        final int closingTabId = tab.getId();
        final int closingTabIndex = indexOf(tab);

        Tab currentTabInModel = TabModelUtils.getCurrentTab(this);
        Tab adjacentTabInModel = getTabAt(closingTabIndex == 0 ? 1 : closingTabIndex - 1);
        Tab nextTab =
                recommendedNextTab == null
                        ? getNextTabIfClosed(closingTabId, /* uponExit= */ false, tabCloseType)
                        : recommendedNextTab;

        // TODO(dtrainor): Update the list of undoable tabs instead of committing it.
        if (updatePendingTabClosureManager) commitAllTabClosures();

        // Cancel or mute any media currently playing.
        if (pauseMedia) {
            WebContents webContents = tab.getWebContents();
            if (webContents != null) {
                webContents.suspendAllMediaPlayers();
                webContents.setAudioMuted(true);
            }
        }

        mTabs.remove(tab);
        mTabIdToTabs.remove(tab.getId());
        mTabCountSupplier.set(mTabs.size());

        boolean nextIsIncognito = nextTab == null ? false : nextTab.isIncognito();
        int nextTabId = nextTab == null ? Tab.INVALID_TAB_ID : nextTab.getId();
        int nextTabIndex =
                nextTab == null
                        ? INVALID_TAB_INDEX
                        : TabModelUtils.getTabIndexById(
                                mModelDelegate.getModel(nextIsIncognito), nextTabId);

        if (nextTab != currentTabInModel) {
            if (nextIsIncognito != isIncognito()) {
                mIndex = indexOf(adjacentTabInModel);
            }

            TabModel nextModel = mModelDelegate.getModel(nextIsIncognito);
            nextModel.setIndex(nextTabIndex, selectionType);
        } else {
            mIndex = nextTabIndex;
            mCurrentTabSupplier.set(TabModelUtils.getCurrentTab(this));
        }

        if (updatePendingTabClosureManager && supportsPendingClosures()) {
            mPendingTabClosureManager.resetState();
        }
    }

    /**
     * Actually closes and cleans up {@code tab}.
     * @param tab The {@link Tab} to close.
     * @param notifyTabClosureCommitted If true then observers will receive a tabClosureCommitted
     *     notification.
     */
    private void finalizeTabClosure(Tab tab, boolean notifyTabClosureCommitted) {
        mTabContentManager.removeTabThumbnail(tab.getId());

        for (TabModelObserver obs : mObservers) obs.onFinishingTabClosure(tab);
        if (notifyTabClosureCommitted) {
            for (TabModelObserver obs : mObservers) obs.tabClosureCommitted(tab);
        }

        // Destroy the native tab after the observer notifications have fired, otherwise they risk a
        // use after free or null dereference.
        tab.destroy();
    }

    @Override
    protected boolean closeTabAt(int index) {
        return closeTabs(TabClosureParams.closeTab(getTabAt(index)).allowUndo(false).build());
    }

    @Override
    protected TabCreator getTabCreator(boolean incognito) {
        return incognito ? mIncognitoTabCreator : mRegularTabCreator;
    }

    /** Used to restore tabs from native. */
    @Override
    protected boolean createTabWithWebContents(
            Tab parent, Profile profile, WebContents webContents, boolean select) {
        return getTabCreator(profile.isOffTheRecord())
                .createTabWithWebContents(
                        parent,
                        webContents,
                        select
                                ? TabLaunchType.FROM_RECENT_TABS_FOREGROUND
                                : TabLaunchType.FROM_RECENT_TABS);
    }

    @Override
    public void openNewTab(
            Tab parent,
            GURL url,
            @Nullable Origin initiatorOrigin,
            String extraHeaders,
            ResourceRequestBody postData,
            int disposition,
            boolean persistParentage,
            boolean isRendererInitiated) {
        if (parent.isClosing()) return;

        boolean incognito = parent.isIncognito();
        @TabLaunchType int tabLaunchType = TabLaunchType.FROM_LONGPRESS_FOREGROUND;

        switch (disposition) {
            case WindowOpenDisposition.NEW_WINDOW: // fall through
            case WindowOpenDisposition.NEW_FOREGROUND_TAB:
                break;
            case WindowOpenDisposition.NEW_POPUP: // fall through
            case WindowOpenDisposition.NEW_BACKGROUND_TAB:
                tabLaunchType = TabLaunchType.FROM_LONGPRESS_BACKGROUND;
                break;
            case WindowOpenDisposition.OFF_THE_RECORD:
                incognito = true;
                break;
            default:
                assert false;
        }

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setInitiatorOrigin(initiatorOrigin);
        loadUrlParams.setVerbatimHeaders(extraHeaders);
        loadUrlParams.setPostData(postData);
        loadUrlParams.setIsRendererInitiated(isRendererInitiated);
        getTabCreator(incognito)
                .createNewTab(loadUrlParams, tabLaunchType, persistParentage ? parent : null);
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
    public void openMostRecentlyClosedEntry() {
        // First try to recover tab from rewound list.
        if (supportsPendingClosures() && mPendingTabClosureManager.openMostRecentlyClosedEntry()) {
            return;
        }

        // If there are no pending closures in the rewound list, then try to restore from the native
        // tab restore service.
        mModelDelegate.openMostRecentlyClosedEntry(this);
        // If there is only one tab, select it.
        if (getCount() == 1) setIndex(0, TabSelectionType.FROM_NEW);
    }

    @Override
    public void setActive(boolean active) {
        mActive = active;
    }

    @Override
    public int getTabCountNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {
        return getTabsNavigatedInTimeWindow(beginTimeMs, endTimeMs).size();
    }

    @Override
    public void closeTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {
        List<Tab> tabsToClose = getTabsNavigatedInTimeWindow(beginTimeMs, endTimeMs);
        if (tabsToClose.isEmpty()) return;

        final TabModelFilter filter = TabModelUtils.getTabModelFilterByTab(tabsToClose.get(0));

        assert filter instanceof TabGroupModelFilter;
        final TabGroupModelFilter groupingFilter = (TabGroupModelFilter) filter;

        var params =
                TabClosureParams.closeTabs(tabsToClose)
                        .allowUndo(false)
                        .saveToTabRestoreService(false)
                        .build();
        groupingFilter.closeTabs(params);
    }

    @VisibleForTesting
    List<Tab> getTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {
        List<Tab> tabList = new ArrayList<>();
        for (Tab tab : mTabs) {
            if (tab.isCustomTab()) continue;

            final long recentNavigationTime = tab.getLastNavigationCommittedTimestampMillis();
            if (recentNavigationTime >= beginTimeMs && recentNavigationTime < endTimeMs) {
                tabList.add(tab);
            }
        }

        return tabList;
    }

    private void notifyOnFinishingMultipleTabClosure(
            List<Tab> tabs, boolean saveToTabRestoreService) {
        for (TabModelObserver obs : mObservers) {
            obs.onFinishingMultipleTabClosure(tabs, saveToTabRestoreService);
        }
    }
}
