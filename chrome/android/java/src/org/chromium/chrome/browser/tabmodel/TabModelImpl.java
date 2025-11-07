// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.process_launcher.ScopedServiceBindingBatch;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.PendingTabClosureManager.PendingTabClosureDelegate;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.tasks.tab_management.MoveTabUtils;
import org.chromium.components.tabs.TabStripCollection;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This is the implementation of the synchronous {@link TabModel} for the {@link
 * ChromeTabbedActivity}.
 *
 * @deprecated This class is replaced by {@link TabCollectionTabModelImpl}. This class will be
 *     deleted in the coming weeks. If you make a change to this class it MUST be mirrored to {@link
 *     TabCollectionTabModelImpl}.
 */
@Deprecated
@NullMarked
public class TabModelImpl extends TabModelJniBridge {
    /** The name of the UKM event used for tab state changes. */
    private static final String UKM_METRICS_TAB_STATE_CHANGED = "Tab.StateChange";

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
    private final TabRemover mTabRemover;
    private final ObserverList<TabModelObserver> mObservers = new ObserverList<>();
    private final NextTabPolicySupplier mNextTabPolicySupplier;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final ObservableSupplierImpl<@Nullable Tab> mCurrentTabSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>();
    private final boolean mIsArchivedTabModel;
    private final Set<Integer> mMultiSelectedTabs = new HashSet<>();

    /** This specifies the current {@link Tab} in {@link #mTabs}. */
    private int mIndex = INVALID_TAB_INDEX;

    private boolean mActive;
    private boolean mInitializationComplete;

    // Undo State Tracking -------------------------------------------------------------------------

    private @Nullable PendingTabClosureManager mPendingTabClosureManager;

    /**
     * Implementation of {@link PendingTabClosureDelegate} that has access to the internal state of
     * TabModelImpl.
     */
    private class PendingTabClosureDelegateImpl implements PendingTabClosureDelegate {
        @Override
        public void insertUndoneTabClosureAt(Tab tab, int insertIndex) {
            if (mIndex >= insertIndex) mIndex++;
            assert !tab.isDestroyed() : "Attempting to undo tab that is destroyed.";

            // Alert observers that the tab closure will be undone. Intentionally notifies before
            // the tabs have been re-inserted into the model.
            for (TabModelObserver obs : mObservers) {
                obs.willUndoTabClosure(Collections.singletonList(tab), /* isAllTabs= */ false);
            }

            mTabs.add(insertIndex, tab);
            tab.onAddedToTabModel(mCurrentTabSupplier, TabModelImpl.this::isTabMultiSelected);
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
                if (ChromeFeatureList.sTabClosureMethodRefactor.isEnabled()) {
                    obs.onTabCloseUndone(Collections.singletonList(tab), /* isAllTabs= */ false);
                } else {
                    obs.tabClosureUndone(tab);
                }
            }

            // If the mIndex we set earlier is still in use then trigger a proper index update and
            // notify any observers.
            if (wasInvalidIndex && isActiveModel() && mIndex == insertIndex) {
                // Reset the index first so the event is raised properly as a index change and not
                // re-using the current index.
                mIndex = INVALID_TAB_INDEX;
                setIndex(insertIndex, TabSelectionType.FROM_UNDO);
            } else if (wasInvalidIndex && !isActiveModel()) {
                mCurrentTabSupplier.set(TabModelUtils.getCurrentTab(TabModelImpl.this));
            }
        }

        @Override
        public void finalizeClosure(Tab tab) {
            finalizeTabClosure(tab, true, TabClosingSource.UNKNOWN);
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

        @Override
        public List<Tab> getAllTabs() {
            return TabModelImpl.this.getAllTabs();
        }
    }

    public TabModelImpl(
            Profile profile,
            @ActivityType int activityType,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            TabModelDelegate modelDelegate,
            TabRemover tabRemover,
            boolean supportUndo,
            boolean isArchivedTabModel) {
        super(profile);
        mRegularTabCreator = regularTabCreator;
        mIncognitoTabCreator = incognitoTabCreator;
        mOrderController = orderController;
        mTabContentManager = tabContentManager;
        assert mTabContentManager != null;
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mModelDelegate = modelDelegate;
        mTabRemover = tabRemover;
        mTabCountSupplier.set(0);
        if (supportUndo && !isIncognito()) {
            mPendingTabClosureManager =
                    new PendingTabClosureManager(this, new PendingTabClosureDelegateImpl());
        }
        mIsArchivedTabModel = isArchivedTabModel;
        // The call to initializeNative() should be as late as possible, as it results in calling
        // observers on the native side, which may in turn call |addObserver()| on this object.
        initializeNative(activityType, isArchivedTabModel);
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
        for (TabModelObserver obs : mObservers) obs.onDestroy();
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
    public void completeInitialization() {
        mInitializationComplete = true;

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
    public ObservableSupplier<Integer> getTabCountSupplier() {
        return mTabCountSupplier;
    }

    @Override
    public TabCreator getTabCreator() {
        return getTabCreator(isIncognitoBranded());
    }

    @Override
    public void moveTabToWindow(Tab tab, Activity activity, int newIndex) {
        mModelDelegate.moveTabToWindow(tab, activity, newIndex);
    }

    @Override
    public void moveTabGroupToWindow(Token tabGroupId, Activity activity, int newIndex) {
        mModelDelegate.moveTabGroupToWindow(tabGroupId, activity, newIndex, isIncognito());
    }

    /**
     * Initializes the newly created tab, adds it to controller, and dispatches creation step
     * notifications.
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
            // Clear the multi-selection set before adding the tab.
            clearMultiSelection(/* notifyObservers= */ false);
            boolean selectTab =
                    mOrderController.willOpenInForeground(type, isIncognitoBranded())
                            || (mTabs.size() == 0
                                    && type == TabLaunchType.FROM_LONGPRESS_BACKGROUND);

            index = mOrderController.determineInsertionIndex(type, index, tab);
            if (tab.getIsPinned()) {
                int firstNonPinnedTabIndex = findFirstNonPinnedTabIndex();
                if (firstNonPinnedTabIndex == INVALID_TAB_INDEX) {
                    // All tabs are pinned or the model is empty, next valid non-pinned index is at
                    // the end of the list.
                    firstNonPinnedTabIndex = mTabs.size();
                }

                // Insert in next non-pinned index if index wasn't handled in
                // TabModelOrderController.
                if (index == INVALID_TAB_INDEX) index = firstNonPinnedTabIndex;
                assert index <= firstNonPinnedTabIndex;
            }

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
            tab.onAddedToTabModel(mCurrentTabSupplier, this::isTabMultiSelected);
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
        newIndex = MathUtils.clamp(newIndex, 0, mTabs.size() - 1);

        int curIndex = TabModelUtils.getTabIndexById(this, id);

        if (curIndex == INVALID_TAB_INDEX || curIndex == newIndex) {
            return;
        }

        // TODO(dtrainor): Update the list of undoable tabs instead of committing it.
        commitAllTabClosures();

        Tab tab = mTabs.remove(curIndex);

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

    @Override
    public void pinTab(
            int tabId,
            boolean showUngroupDialog,
            @Nullable TabModelActionListener tabModelActionListener) {
        Tab eligibleTabToPin = getEligibleTabToPin(tabId);
        if (eligibleTabToPin == null) return;

        TabPinnerActionListener listener =
                new TabPinnerActionListener(() -> doPin(tabId), tabModelActionListener);
        getTabUngrouper()
                .ungroupTabs(
                        Collections.singletonList(eligibleTabToPin),
                        /* trailing= */ true,
                        showUngroupDialog,
                        listener);
        listener.pinIfCollaborationDialogShown();
    }

    private @Nullable Tab getEligibleTabToPin(int tabId) {
        Tab tab = getTabById(tabId);
        if (tab == null) return null;

        if (tab.getIsPinned()) return null;

        return tab;
    }

    private void doPin(int tabId) {
        Tab tab = getEligibleTabToPin(tabId);
        if (tab == null) return;

        int availableIndex = findFirstNonPinnedTabIndex();
        if (availableIndex == mTabs.size()) return;

        WebContents webContents = tab.getWebContents();
        if (webContents != null) {
            new UkmRecorder(webContents, UKM_METRICS_TAB_STATE_CHANGED)
                    .addBooleanMetric("IsPinned")
                    .record();
        }

        notifyWillChangeInPinState(tab);
        tab.setIsPinned(true);
        recordPinTimestamp(tab);
        moveTab(tab.getId(), availableIndex);
        notifyDidChangeInPinState(tab);
    }

    @Override
    public void unpinTab(int tabId) {
        int nextAvailableIndex = findFirstNonPinnedTabIndex();

        Tab tab = getTabById(tabId);
        if (tab == null) return;

        if (!tab.getIsPinned()) return;

        // The index before the first non-pinned tab is the last pinned tab. Hence move the tab to
        // the last pinned tab.
        moveTab(tab.getId(), nextAvailableIndex - 1);

        notifyWillChangeInPinState(tab);
        tab.setIsPinned(false);
        recordPinnedDuration(tab);
        notifyDidChangeInPinState(tab);
    }

    @Override
    public @Nullable Tab getNextTabIfClosed(int id, boolean uponExit) {
        if (mIsArchivedTabModel) return null;
        Tab tab = getTabById(id);
        if (tab == null) return mCurrentTabSupplier.get();
        return TabModelImplUtil.getNextTabIfClosed(
                this,
                mModelDelegate,
                mCurrentTabSupplier,
                mNextTabPolicySupplier,
                Collections.singletonList(tab),
                uponExit,
                TabCloseType.SINGLE);
    }

    @Override
    public boolean isClosurePending(int tabId) {
        if (!supportsPendingClosures()) return false;

        return mPendingTabClosureManager.isClosurePending(tabId);
    }

    @Override
    @EnsuresNonNullIf("mPendingTabClosureManager")
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
     * @param tabClosingSource Used to identify where the close action originated, e.g. the tablet
     *     tab strip.
     * @param tabCloseType Used to notify observers that this tab is closing by itself for {@link
     *     TabModelObserver#onFinishingMultipleTabClosure} if the closure cannot be undone and for
     *     {@link TabModelObserver#willCloseTab}. This should be {@code TabCloseType.SINGLE} if
     *     closing the tab by itself, {@code TabCloseType.MULTIPLE} if closing multiple tabs or
     *     {@code TabCloseType.ALL} all tabs (which also does additional optimization).
     * @param undoRunnable The runnable to invoke if the operation is undone. Only applicable if
     *     {@code allowUndo} and {@code notifyPending} are both true.
     * @return true if the closure succeeds and false otherwise.
     */
    private boolean closeTab(
            Tab tabToClose,
            @Nullable Tab recommendedNextTab,
            boolean uponExit,
            boolean allowUndo,
            boolean notifyPending,
            @TabClosingSource int tabClosingSource,
            @TabCloseType int tabCloseType,
            @Nullable Runnable undoRunnable) {
        if (tabToClose == null) {
            assert false : "Tab is null!";
            return false;
        }

        if (!containsTab(tabToClose)) {
            assert false : "Tried to close a tab from another model!";
            return false;
        }

        allowUndo &= supportsPendingClosures();
        if (isTabMultiSelected(tabToClose.getId())) {
            setTabsMultiSelected(Collections.singleton(tabToClose.getId()), /* isSelected= */ false);
        }
        startTabClosure(tabToClose, recommendedNextTab, uponExit, allowUndo, tabCloseType);
        List<Tab> tabsToClose = Collections.singletonList(tabToClose);
        if (notifyPending && allowUndo) {
            assumeNonNull(mPendingTabClosureManager);
            mPendingTabClosureManager.addTabClosureEvent(tabsToClose, undoRunnable);
            for (TabModelObserver obs : mObservers) {
                obs.onTabClosePending(tabsToClose, /* isAllTabs= */ false, tabClosingSource);
            }
        }
        if (!allowUndo) {
            if (tabCloseType == TabCloseType.SINGLE) {
                notifyOnFinishingMultipleTabClosure(
                        tabsToClose, /* saveToTabRestoreService= */ true);
            }
            finalizeTabClosure(
                    tabToClose, /* notifyTabClosureCommitted= */ false, tabClosingSource);
        }
        return true;
    }

    private void closeMultipleTabs(
            List<Tab> tabs,
            boolean allowUndo,
            boolean saveToTabRestoreService,
            @TabClosingSource int tabClosingSource,
            @Nullable Runnable undoRunnable) {
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
        for (TabModelObserver obs : mObservers) obs.willCloseMultipleTabs(allowUndo, tabs);
        if (!allowUndo) {
            notifyOnFinishingMultipleTabClosure(tabs, saveToTabRestoreService);
        }
        for (Tab tab : tabs) {
            // Pass a null undoRunnable here as we want to attach it to the latter tab closure
            // event.
            closeTab(
                    tab,
                    /* recommendedNextTab= */ null,
                    /* uponExit= */ false,
                    allowUndo,
                    /* notifyPending= */ false,
                    tabClosingSource,
                    TabCloseType.MULTIPLE,
                    /* undoRunnable= */ null);
        }
        if (allowUndo) {
            assumeNonNull(mPendingTabClosureManager);
            mPendingTabClosureManager.addTabClosureEvent(tabs, undoRunnable);
            for (TabModelObserver obs : mObservers) {
                obs.onTabClosePending(tabs, false, tabClosingSource);
            }
        }
    }

    private void closeAllTabs(
            boolean uponExit,
            boolean allowUndo,
            @TabClosingSource int tabClosingSource,
            @Nullable Runnable undoRunnable) {
        for (TabModelObserver obs : mObservers) obs.willCloseAllTabs(isIncognito());

        // Force close immediately if:
        // 1. the tabs are to be closed upon app exit,
        // 2. the operation doesn't allow undo,
        // 3. the operation is triggered from tablet tab strip, or
        // 4. Chrome needs to close with a zero-state.
        if (uponExit
                || !allowUndo
                || tabClosingSource == TabClosingSource.TABLET_TAB_STRIP
                || HomepageManager.getInstance().shouldCloseAppWithZeroTabs()) {
            commitAllTabClosures();

            for (int i = 0; i < getCount(); i++) getTabAtChecked(i).setClosing(true);
            notifyOnFinishingMultipleTabClosure(mTabs, /* saveToTabRestoreService= */ true);
            while (getCount() > 0) {
                Tab tab = getTabAtChecked(0);
                // Pass a null undoRunnable here as we want to attach it to the latter tab closure
                // event.
                closeTab(
                        tab,
                        /* recommendedNextTab= */ null,
                        uponExit,
                        /* allowUndo= */ false,
                        /* notifyPending= */ false,
                        tabClosingSource,
                        TabCloseType.ALL,
                        /* undoRunnable= */ null);
            }
            return;
        }

        // Close with the opportunity to undo if this TabModel supports pending closures.
        for (int i = 0; i < getCount(); i++) getTabAtChecked(i).setClosing(true);
        List<Tab> closedTabs = new ArrayList<>(mTabs);
        if (!supportsPendingClosures()) {
            notifyOnFinishingMultipleTabClosure(closedTabs, /* saveToTabRestoreService= */ true);
        }
        while (getCount() > 0) {
            Tab tab = getTabAtChecked(0);
            // Pass a null undoRunnable here as we want to attach it to the latter tab closure
            // event.
            closeTab(
                    tab,
                    /* recommendedNextTab= */ null,
                    /* uponExit= */ false,
                    /* allowUndo= */ true,
                    /* notifyPending= */ false,
                    tabClosingSource,
                    TabCloseType.ALL,
                    /* undoRunnable= */ null);
        }

        if (supportsPendingClosures()) {
            mPendingTabClosureManager.addTabClosureEvent(closedTabs, undoRunnable);
            for (TabModelObserver obs : mObservers) {
                obs.onTabClosePending(closedTabs, true, tabClosingSource);
            }
        }
    }

    @Override
    public @Nullable Tab getTabAt(int index) {
        // This will catch INVALID_TAB_INDEX and return null
        if (index < 0 || index >= mTabs.size()) return null;
        return mTabs.get(index);
    }

    @Override
    public @Nullable Tab getTabById(int tabId) {
        return mTabIdToTabs.get(tabId);
    }

    @Override
    public TabRemover getTabRemover() {
        assert mTabRemover != null;
        return mTabRemover;
    }

    @Override
    public boolean closeTabs(TabClosureParams tabClosureParams) {
        if (!tabClosureParams.allowUndo) {
            // The undo stacks assumes that previous actions in the stack are undoable. If an entry
            // is not undoable then the reversal of the operations may fail or yield an invalid
            // state. Commit the rest of the closures now to ensure that doesn't occur.
            commitAllTabClosures();
        }
        // TODO(crbug.com/356445932): Respect the provided params more broadly.
        switch (tabClosureParams.tabCloseType) {
            case TabCloseType.SINGLE:
                assert assumeNonNull(tabClosureParams.tabs).size() == 1;
                boolean notifyPending = tabClosureParams.allowUndo;
                return closeTab(
                        tabClosureParams.tabs.get(0),
                        tabClosureParams.recommendedNextTab,
                        tabClosureParams.uponExit,
                        tabClosureParams.allowUndo,
                        notifyPending,
                        tabClosureParams.tabClosingSource,
                        tabClosureParams.tabCloseType,
                        tabClosureParams.undoRunnable);
            case TabCloseType.MULTIPLE:
                closeMultipleTabs(
                        assumeNonNull(tabClosureParams.tabs),
                        tabClosureParams.allowUndo,
                        tabClosureParams.saveToTabRestoreService,
                        tabClosureParams.tabClosingSource,
                        tabClosureParams.undoRunnable);
                return true;
            case TabCloseType.ALL:
                closeAllTabs(
                        tabClosureParams.uponExit,
                        tabClosureParams.allowUndo,
                        tabClosureParams.tabClosingSource,
                        tabClosureParams.undoRunnable);
                mTabCountSupplier.set(mTabs.size());
                return true;
            default:
                assert false : "Not reached.";
                return false;
        }
    }

    // Index of the given tab in the order of the tab stack.
    @Override
    public int indexOf(@Nullable Tab tab) {
        if (tab == null) return INVALID_TAB_INDEX;
        int retVal = mTabs.indexOf(tab);
        return retVal == -1 ? INVALID_TAB_INDEX : retVal;
    }

    @Override
    public Iterator<Tab> iterator() {
        return ReadOnlyIterator.maybeCreate(mTabs.iterator());
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
    public ObservableSupplier<@Nullable Tab> getCurrentTabSupplier() {
        return mCurrentTabSupplier;
    }

    // This function is complex and its behavior depends on persisted state, including mIndex.
    @Override
    public void setIndex(int i, final @TabSelectionType int type) {
        if (mIsArchivedTabModel) return;
        // Batch service binding updates for the tabs becoming active and inactive. The activeness
        // change usually causes visibility changes, which updates service bindings of subframes at
        // the same time.
        try (ScopedServiceBindingBatch scope = ScopedServiceBindingBatch.scoped()) {
            TraceEvent.begin("TabModelImpl.setIndex");
            int lastId =
                    mCurrentTabSupplier.get() != null
                            ? mCurrentTabSupplier.get().getId()
                            : Tab.INVALID_TAB_ID;

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
                for (TabModelObserver obs : mObservers) {
                    obs.didSelectTab(tab, type, lastId);
                    // Required, otherwise the previously active tab will have MULTISELECTED as its
                    // VisualState.
                    obs.onTabsSelectionChanged();
                }
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

    @Override
    public boolean isInitializationComplete() {
        return mInitializationComplete;
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
            @Nullable Tab recommendedNextTab,
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
            @Nullable Tab recommendedNextTab,
            @TabSelectionType int selectionType,
            boolean pauseMedia,
            boolean updatePendingTabClosureManager,
            @TabCloseType int tabCloseType) {
        assert selectionType == TabSelectionType.FROM_CLOSE
                || selectionType == TabSelectionType.FROM_EXIT;

        final int closingTabIndex = indexOf(tab);

        Tab currentTabInModel = TabModelUtils.getCurrentTab(this);
        Tab adjacentTabInModel = getTabAt(closingTabIndex == 0 ? 1 : closingTabIndex - 1);
        Tab nextTab =
                recommendedNextTab == null
                        ? TabModelImplUtil.getNextTabIfClosed(
                                this,
                                mModelDelegate,
                                mCurrentTabSupplier,
                                mNextTabPolicySupplier,
                                Collections.singletonList(tab),
                                /* uponExit= */ false,
                                tabCloseType)
                        : recommendedNextTab;

        // TODO(dtrainor): Update the list of undoable tabs instead of committing it.
        if (updatePendingTabClosureManager) commitAllTabClosures();

        // Cancel or mute any media currently playing.
        if (pauseMedia) {
            TabUtils.pauseMedia(tab);
        }

        mTabs.remove(tab);
        tab.onRemovedFromTabModel(mCurrentTabSupplier);
        mTabIdToTabs.remove(tab.getId());
        // Close all tabs should update the mTabCountSupplier after all tabs are closed.
        if (tabCloseType != TabCloseType.ALL) {
            mTabCountSupplier.set(mTabs.size());
        }

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
                mCurrentTabSupplier.set(adjacentTabInModel);
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

        // Deferred until another tab is selected. Otherwise the compositor may try to re-navigate
        // the tab.
        if (pauseMedia && TabUtils.isCapturingForMedia(tab)) {
            // If media is being captured freeze the tab to disconnect it.
            tab.freeze();
        }
    }

    /**
     * Actually closes and cleans up {@code tab}.
     *
     * @param tab The {@link Tab} to close.
     * @param notifyTabClosureCommitted If true then observers will receive a tabClosureCommitted
     *     notification.
     * @param closingSource The source that triggered the tab close (e.g. the tablet tab strip).
     */
    private void finalizeTabClosure(
            Tab tab, boolean notifyTabClosureCommitted, @TabClosingSource int closingSource) {
        mTabContentManager.removeTabThumbnail(tab.getId());

        for (TabModelObserver obs : mObservers) {
            obs.onFinishingTabClosure(tab, closingSource);
        }
        if (notifyTabClosureCommitted) {
            for (TabModelObserver obs : mObservers) obs.tabClosureCommitted(tab);
        }

        // Destroy the native tab after the observer notifications have fired, otherwise they risk a
        // use after free or null dereference.
        tab.destroy();
    }

    @Override
    protected TabCreator getTabCreator(boolean incognito) {
        return incognito ? mIncognitoTabCreator : mRegularTabCreator;
    }

    @Override
    public int getCount() {
        return mTabs.size();
    }

    @Override
    public int index() {
        if (mIsArchivedTabModel) return INVALID_TAB_INDEX;
        return mIndex;
    }

    @Override
    protected boolean isSessionRestoreInProgress() {
        return !mModelDelegate.isTabModelRestored();
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
    public void moveTabToIndex(Tab tab, int newIndex) {
        TabGroupModelFilter filter = mModelDelegate.getFilter(isIncognitoBranded());
        MoveTabUtils.moveSingleTab(this, filter, tab, newIndex);
    }

    @Override
    public void moveGroupToIndex(Token tabGroupId, int newIndex) {
        TabGroupModelFilter filter = mModelDelegate.getFilter(isIncognitoBranded());
        if (!filter.tabGroupExists(tabGroupId)) return;
        MoveTabUtils.moveTabGroup(this, filter, tabGroupId, newIndex);
    }

    @Override
    public List<Tab> getAllTabs() {
        return mTabs;
    }

    // This is only for TabListInterface, prefer calling TabGroupModelFilter methods.
    @Override
    protected @Nullable Token addTabsToGroup(@Nullable Token tabGroupId, List<Tab> tabs) {
        if (tabs.isEmpty()) return null;

        TabGroupModelFilter filter = mModelDelegate.getFilter(isIncognitoBranded());
        if (tabGroupId == null) {
            // TODO(crbug.com/427929717): Ensure any pinned tabs get unpinned.
            ungroup(tabs);
            Tab destinationTab = tabs.get(0);
            filter.mergeListOfTabsToGroup(tabs, destinationTab, MergeNotificationType.DONT_NOTIFY);
            return destinationTab.getTabGroupId();
        }
        List<Tab> tabsInGroup = filter.getTabsInGroup(tabGroupId);
        if (tabsInGroup.isEmpty()) return null;

        // TODO(crbug.com/427929717): Ensure any pinned tabs get unpinned.
        List<Tab> tabsToGroup = new ArrayList<>();
        for (Tab tab : tabs) {
            if (!tabsInGroup.contains(tab)) {
                tabsToGroup.add(tab);
            }
        }
        // Ungroup the tabs first to ensure they are not in any groups.
        ungroup(tabsToGroup);
        filter.mergeListOfTabsToGroup(
                tabsToGroup, tabsInGroup.get(0), MergeNotificationType.DONT_NOTIFY);
        return tabsInGroup.get(0).getTabGroupId();
    }

    @Override
    protected TabUngrouper getTabUngrouper() {
        return mModelDelegate.getFilter(isIncognitoBranded()).getTabUngrouper();
    }

    private void notifyOnFinishingMultipleTabClosure(
            List<Tab> tabs, boolean saveToTabRestoreService) {
        for (TabModelObserver obs : mObservers) {
            obs.onFinishingMultipleTabClosure(tabs, saveToTabRestoreService);
        }
    }

    /**
     * Notifies observers that the pin state of the given tab will change.
     *
     * @param tab The tab whose pin state will change.
     */
    private void notifyWillChangeInPinState(Tab tab) {
        for (TabModelObserver obs : mObservers) {
            obs.willChangePinState(tab);
        }
    }

    /**
     * Notifies observers that the pin state of the given tab has changed.
     *
     * @param tab The tab whose pin state has changed.
     */
    private void notifyDidChangeInPinState(Tab tab) {
        for (TabModelObserver obs : mObservers) {
            obs.didChangePinState(tab);
        }
    }

    @Override
    public void setTabsMultiSelected(Set<Integer> tabIds, boolean isSelected) {
        TabModelImplUtil.setTabsMultiSelected(tabIds, isSelected, mMultiSelectedTabs, mObservers);
        assert mMultiSelectedTabs.isEmpty()
                        || mMultiSelectedTabs.contains(TabModelUtils.getCurrentTabId(this))
                : "If the selection is not empty, the current tab must always be present within the"
                        + " set.";
    }

    @Override
    public void clearMultiSelection(boolean notifyObservers) {
        TabModelImplUtil.clearMultiSelection(notifyObservers, mMultiSelectedTabs, mObservers);
    }

    @Override
    public boolean isTabMultiSelected(int tabId) {
        return TabModelImplUtil.isTabMultiSelected(tabId, mMultiSelectedTabs, this);
    }

    @Override
    public int getMultiSelectedTabsCount() {
        if (mTabs.isEmpty()) return 0;
        // If no other tabs are in multi-selection, this returns 1, as the active tab is always
        // considered selected.
        return mMultiSelectedTabs.isEmpty() ? 1 : mMultiSelectedTabs.size();
    }

    @Override
    public int findFirstNonPinnedTabIndex() {
        int low = 0;
        int high = getCount() - 1;
        int firstNonPinnedIndex = getCount();

        while (low <= high) {
            int mid = low + (high - low) / 2;
            Tab tab = getTabAt(mid);
            assumeNonNull(tab);
            if (tab.getIsPinned()) {
                // The first non-pinned tab must be after this index.
                low = mid + 1;
            } else {
                // This might be the first non-pinned tab, but there might be an earlier one.
                firstNonPinnedIndex = mid;
                high = mid - 1;
            }
        }

        return firstNonPinnedIndex;
    }

    @Override
    public @Nullable TabStripCollection getTabStripCollection() {
        return null;
    }
}
