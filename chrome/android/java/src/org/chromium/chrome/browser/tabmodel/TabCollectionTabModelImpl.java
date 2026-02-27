// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType.DONT_NOTIFY;
import static org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType.NOTIFY_ALWAYS;
import static org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP;
import static org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils.UNSET_TAB_GROUP_TITLE;
import static org.chromium.chrome.browser.tabmodel.TabGroupUtils.areAnyTabsPartOfSharedGroup;

import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.process_launcher.ScopedServiceBindingBatch;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge;
import org.chromium.chrome.browser.ntp.RecentlyClosedBulkEvent;
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedGroup;
import org.chromium.chrome.browser.ntp.RecentlyClosedTab;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.PendingTabClosureManager.PendingTabClosureDelegate;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabModel.RecentlyClosedEntryType;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.DetachReason;
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
import java.util.Objects;
import java.util.Set;
import java.util.function.Supplier;

/**
 * This class implements {@link TabModelInternal} and {@link TabGroupModelFilterInternal}.
 *
 * <p>The class uses the tab collection tree-like structure available in components/tabs/ to
 * organize tabs. The tabs in C++ tab collections are only cached with weak ptr references to the
 * C++ TabAndroid objects for memory safety; however, a strong reference is kept in {@code
 * mTabIdToTabs} to ensure the Java Tab objects are not GC'd. As a result of the weak ptrs, most of
 * this class's public methods can only be used on the UI thread.
 *
 * <p>Ideally, more of the observers and logic should be moved to be in C++ or shared with desktop's
 * tab strip model.
 */
@NullMarked
@JNINamespace("tabs")
public class TabCollectionTabModelImpl extends TabModelJniBridge
        implements TabGroupModelFilterInternal {
    /** The name of the UKM event used for tab state changes. */
    private static final String UKM_METRICS_TAB_STATE_CHANGED = "Tab.StateChange";

    /**
     * Holds data for an individual tab that was part of a group merge operation that may be undone.
     */
    private static class UndoGroupTabData {
        public final Tab tab;
        public final int originalIndex;
        public final boolean originalIsPinned;
        public final @Nullable Token originalTabGroupId;

        UndoGroupTabData(
                Tab tab,
                int originalIndex,
                boolean originalIsPinned,
                @Nullable Token originalTabGroupId) {
            this.tab = tab;
            this.originalIndex = originalIndex;
            this.originalIsPinned = originalIsPinned;
            this.originalTabGroupId = originalTabGroupId;
        }
    }

    /**
     * Holds the metadata and individual tab data for a group merge operation that may be undone.
     */
    private static class UndoGroupMetadataImpl implements UndoGroupMetadata {
        private final Token mDestinationGroupId;
        private final boolean mIsIncognito;

        public final List<UndoGroupTabData> mergedTabsData = new ArrayList<>();
        public final int adoptedTabGroupOriginalIndex;
        public final List<Token> removedTabGroupIds;
        public final boolean didCreateNewGroup;
        public final boolean adoptedTabGroupTitle;
        public final boolean wasDestinationTabGroupCollapsed;

        UndoGroupMetadataImpl(
                Token destinationGroupId,
                boolean isIncognito,
                @Nullable UndoGroupTabData destinationTabData,
                int adoptedTabGroupOriginalIndex,
                List<Token> removedTabGroupIds,
                boolean didCreateNewGroup,
                boolean adoptedTabGroupTitle,
                boolean wasDestinationTabGroupCollapsed) {
            mDestinationGroupId = destinationGroupId;
            mIsIncognito = isIncognito;
            if (destinationTabData != null) {
                this.mergedTabsData.add(destinationTabData);
            }
            this.adoptedTabGroupOriginalIndex = adoptedTabGroupOriginalIndex;
            this.removedTabGroupIds = removedTabGroupIds;
            this.didCreateNewGroup = didCreateNewGroup;
            this.adoptedTabGroupTitle = adoptedTabGroupTitle;
            this.wasDestinationTabGroupCollapsed = wasDestinationTabGroupCollapsed;
        }

        void addMergedTab(
                Tab tab,
                int originalIndex,
                boolean originalIsPinned,
                @Nullable Token originalTabGroupId) {
            this.mergedTabsData.add(
                    new UndoGroupTabData(tab, originalIndex, originalIsPinned, originalTabGroupId));
        }

        @Override
        public Token getTabGroupId() {
            return mDestinationGroupId;
        }

        @Override
        public boolean isIncognito() {
            return mIsIncognito;
        }
    }

    /**
     * Implementation of {@link PendingTabClosureDelegate} that has access to the internal state of
     * {@link TabCollectionTabModelImpl}.
     */
    private class PendingTabClosureDelegateImpl implements PendingTabClosureDelegate {
        @Override
        public void insertUndoneTabClosureAt(Tab tab, int insertIndex) {
            try (ScopedStorageBatch ignored = mBatchFactory.get()) {
                insertUndoneTabClosureAtInternal(tab, insertIndex);
            }
        }

        private void insertUndoneTabClosureAtInternal(Tab tab, int insertIndex) {
            assert !tab.isDestroyed() : "Attempting to undo tab that is destroyed.";
            maybeAssertTabHasWebContents(tab);

            // Alert observers that the tab closure will be undone. Intentionally notifies before
            // the tabs have been re-inserted into the model.
            for (TabModelObserver obs : mTabModelObservers) {
                obs.willUndoTabClosure(Collections.singletonList(tab), /* isAllTabs= */ false);
            }

            Token tabGroupId = tab.getTabGroupId();
            boolean restoredTabGroup = tabGroupId != null && !tabGroupExists(tabGroupId);
            int finalIndex =
                    TabCollectionTabModelImplJni.get()
                            .addTabRecursive(
                                    mNativeTabCollectionTabModelImplPtr,
                                    tab,
                                    insertIndex,
                                    tabGroupId,
                                    restoredTabGroup,
                                    tab.getIsPinned());

            decrementClosingTabsCount();

            if (restoredTabGroup) {
                assumeNonNull(tabGroupId);
                setLastShownTabForGroup(tabGroupId, tab);
            }

            tab.onAddedToTabModel(
                    mCurrentTabSupplier, TabCollectionTabModelImpl.this::isTabMultiSelected);
            mTabIdToTabs.put(tab.getId(), tab);
            mTabCountSupplier.set(assumeNonNull(mTabCountSupplier.get()) + 1);

            WebContents webContents = tab.getWebContents();
            if (webContents != null) webContents.setAudioMuted(false);

            boolean noTabIsActivated = !(mCurrentTabSupplier.get() != null);
            if (noTabIsActivated) {
                mCurrentTabSupplier.set(tab);
            }
            if (tabGroupId != null) {
                mHidingTabGroups.remove(tabGroupId);
            }

            // Alert observers the tab closure was undone before calling setIndex if necessary as
            // * Observers may rely on this signal to re-introduce the tab to their visibility if it
            //   is selected before this it may not exist for those observers.
            // * UndoRefocusHelper may update the index out-of-band.
            for (TabModelObserver obs : mTabModelObservers) {
                if (ChromeFeatureList.sTabClosureMethodRefactor.isEnabled()) {
                    obs.onTabCloseUndone(Collections.singletonList(tab), /* isAllTabs= */ false);
                } else {
                    obs.tabClosureUndone(tab);
                }
            }

            // If there is no selected tab, then trigger a proper selected tab update and
            // notify any observers.
            if (noTabIsActivated && isActiveModel()) {
                mCurrentTabSupplier.set(null);
                setIndex(finalIndex, TabSelectionType.FROM_UNDO);
            } else if (noTabIsActivated && !isActiveModel()) {
                mCurrentTabSupplier.set(tab);
            }

            if (restoredTabGroup) {
                assumeNonNull(tabGroupId);
                restoreTabGroupVisualData(tabGroupId);
            }
        }

        @Override
        public void finalizeClosure(Tab tab) {
            finalizeTabClosure(
                    tab, /* notifyTabClosureCommitted= */ true, TabClosingSource.UNKNOWN);
        }

        @Override
        public void notifyOnFinishingMultipleTabClosure(List<Tab> tabs) {
            TabCollectionTabModelImpl.this.notifyOnFinishingMultipleTabClosure(
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
            return TabCollectionTabModelImpl.this.getAllTabs();
        }
    }

    /** Holds a tab and its index in the tab collection. */
    private static class IndexAndTab {
        /** The index may be {@link TabList#INVALID_TAB_INDEX} if the tab is not in the model. */
        public final int index;

        /** The tab may be {@code null} if the tab is not in the model. */
        public final @Nullable Tab tab;

        IndexAndTab(int index, @Nullable Tab tab) {
            this.index = index;
            this.tab = tab;
        }
    }

    private final ObserverList<TabModelObserver> mTabModelObservers = new ObserverList<>();
    private final ObserverList<TabGroupModelFilterObserver> mTabGroupObservers =
            new ObserverList<>();
    private final SettableLookAheadObservableSupplier<Tab> mCurrentTabSupplier =
            new SettableLookAheadObservableSupplier<>();
    private final SettableNonNullObservableSupplier<Integer> mTabCountSupplier =
            ObservableSuppliers.createNonNull(0);
    private final Set<Integer> mMultiSelectedTabs = new HashSet<>();
    private final Set<Token> mHidingTabGroups = new HashSet<>();

    // Efficient lookup of tabs by id rather than index (stored in C++). Also ensures the Java Tab
    // objects are not GC'd as the C++ TabAndroid objects only hold weak references to their Java
    // counterparts.
    private final Map<Integer, Tab> mTabIdToTabs = new HashMap<>();

    private final @TabModelType int mTabModelType;
    private final TabCreator mRegularTabCreator;
    private final TabCreator mIncognitoTabCreator;
    private final TabModelOrderController mOrderController;
    private final TabContentManager mTabContentManager;
    private final NextTabPolicySupplier mNextTabPolicySupplier;
    private final TabModelDelegate mModelDelegate;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final TabRemover mTabRemover;
    private final TabUngrouper mTabUngrouper;
    private final Supplier<ScopedStorageBatch> mBatchFactory;
    private @Nullable PendingTabClosureManager mPendingTabClosureManager;

    private long mNativeTabCollectionTabModelImplPtr;
    // Only ever true for the regular tab model. Called after tab state is initialized, before
    // broadcastSessionRestoreComplete().
    private boolean mInitializationComplete;
    private boolean mActive;

    // This is null until the first closing tab is encountered. This prevents #isClosingAllTabs()
    // from returning true for an uninitialized TabModel.
    private @Nullable Integer mClosingTabsCount;

    /**
     * @param profile The {@link Profile} tabs in the tab collection tab model belongs to.
     * @param activityType The type of activity this tab collection tab model is for.
     * @param regularTabCreator The tab creator for regular tabs.
     * @param incognitoTabCreator The tab creator for incognito tabs.
     * @param orderController Controls logic for selecting and positioning tabs.
     * @param tabContentManager Manages tab content.
     * @param nextTabPolicySupplier Supplies the next tab policy.
     * @param modelDelegate The {@link TabModelDelegate} for interacting outside the tab model.
     * @param asyncTabParamsManager To detect if an async tab operation is in progress.
     * @param tabRemover For removing tabs.
     * @param tabUngrouper For ungrouping tabs.
     * @param batchFactory A factory for creating {@link ScopedStorageBatch} objects.
     * @param supportUndo Whether the tab model supports undo functionality.
     */
    public TabCollectionTabModelImpl(
            Profile profile,
            @ActivityType int activityType,
            @TabModelType int tabModelType,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            TabModelDelegate modelDelegate,
            AsyncTabParamsManager asyncTabParamsManager,
            TabRemover tabRemover,
            TabUngrouper tabUngrouper,
            Supplier<ScopedStorageBatch> batchFactory,
            boolean supportUndo) {
        super(profile);
        assertOnUiThread();
        mTabModelType = tabModelType;
        mRegularTabCreator = regularTabCreator;
        mIncognitoTabCreator = incognitoTabCreator;
        mOrderController = orderController;
        mTabContentManager = tabContentManager;
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mModelDelegate = modelDelegate;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mTabRemover = tabRemover;
        mTabUngrouper = tabUngrouper;
        mBatchFactory = batchFactory;
        if (supportUndo && !isIncognito()) {
            mPendingTabClosureManager =
                    new PendingTabClosureManager(this, new PendingTabClosureDelegateImpl());
        }

        initializeNative(activityType, tabModelType);
    }

    @Override
    public void destroy() {
        assertOnUiThread();
        commitAllTabClosures();

        for (TabModelObserver obs : mTabModelObservers) obs.onDestroy();

        // Cache the list of tabs so we have them before native is destroyed.
        List<Tab> tabs = getAllTabs();

        // Destroy native first to avoid any weak ptrs to TabAndroid objects from outliving the
        // tab's themselves.
        if (mNativeTabCollectionTabModelImplPtr != 0) {
            TabCollectionTabModelImplJni.get().destroy(mNativeTabCollectionTabModelImplPtr);
            mNativeTabCollectionTabModelImplPtr = 0;
        }

        for (Tab tab : tabs) {
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

        mTabIdToTabs.clear();
        mTabCountSupplier.set(0);
        mTabModelObservers.clear();
        mTabGroupObservers.clear();
        mClosingTabsCount = null;

        super.destroy();
    }

    // TabList overrides except those overridden by TabModelJniBridge.

    @Override
    public int index() {
        assertOnUiThread();
        if (isArchivedTabModel()) return TabList.INVALID_TAB_INDEX;
        return indexOf(mCurrentTabSupplier.get());
    }

    @Override
    public int getCount() {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return 0;
        return TabCollectionTabModelImplJni.get()
                .getTabCountRecursive(mNativeTabCollectionTabModelImplPtr);
    }

    @Override
    public @Nullable Tab getTabAt(int index) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return null;
        return TabCollectionTabModelImplJni.get()
                .getTabAtIndexRecursive(mNativeTabCollectionTabModelImplPtr, index);
    }

    @Override
    public int indexOf(@Nullable Tab tab) {
        assertOnUiThread();
        if (tab == null || mNativeTabCollectionTabModelImplPtr == 0) {
            return TabList.INVALID_TAB_INDEX;
        }
        assert tab.isInitialized();
        return TabCollectionTabModelImplJni.get()
                .getIndexOfTabRecursive(mNativeTabCollectionTabModelImplPtr, tab);
    }

    @Override
    public Iterator<Tab> iterator() {
        return getAllTabs().iterator();
    }

    // SupportsTabModelObserver overrides.

    @Override
    public void addObserver(TabModelObserver observer) {
        assertOnUiThread();
        mTabModelObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        assertOnUiThread();
        mTabModelObservers.removeObserver(observer);
    }

    // TabModel overrides except those overridden by TabModelJniBridge.

    @Override
    public @TabModelType int getTabModelType() {
        return mTabModelType;
    }

    @Override
    public @Nullable Tab getTabById(@TabId int tabId) {
        return mTabIdToTabs.get(tabId);
    }

    @Override
    public TabRemover getTabRemover() {
        assert mTabRemover != null;
        return mTabRemover;
    }

    @Override
    public @Nullable Tab getNextTabIfClosed(@TabId int id, boolean uponExit) {
        if (isArchivedTabModel()) return null;
        assertOnUiThread();
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
    @EnsuresNonNullIf("mPendingTabClosureManager")
    public boolean supportsPendingClosures() {
        assertOnUiThread();
        assert mPendingTabClosureManager == null || !isIncognito();
        return mPendingTabClosureManager != null;
    }

    @Override
    public boolean isClosurePending(@TabId int tabId) {
        if (!supportsPendingClosures()) return false;

        return mPendingTabClosureManager.isClosurePending(tabId);
    }

    @Override
    public void commitAllTabClosures() {
        if (!supportsPendingClosures()) return;

        mPendingTabClosureManager.commitAllTabClosures();
        for (TabModelObserver obs : mTabModelObservers) obs.allTabsClosureCommitted(isIncognito());
    }

    @Override
    public void commitTabClosure(@TabId int tabId) {
        if (!supportsPendingClosures()) return;

        mPendingTabClosureManager.commitTabClosure(tabId);
    }

    @Override
    public void cancelTabClosure(@TabId int tabId) {
        if (!supportsPendingClosures()) return;

        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            mPendingTabClosureManager.cancelTabClosure(tabId);
        }
    }

    @Override
    public long getMostRecentClosureTime() {
        if (supportsPendingClosures()
                && mPendingTabClosureManager.getMostRecentClosureTime()
                        != TabModel.INVALID_TIMESTAMP
                && mPendingTabClosureManager.getMostRecentClosureTime() > 0) {
            return mPendingTabClosureManager.getMostRecentClosureTime();
        }
        return mModelDelegate.getMostRecentClosureTime();
    }

    @Override
    public void openMostRecentlyClosedEntry() {
        assertOnUiThread();
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            openMostRecentlyClosedEntryInternal();
        }
    }

    @Override
    public @RecentlyClosedEntryType int getMostRecentlyClosedEntryType() {
        if (getProfile() == null || isIncognito()) return RecentlyClosedEntryType.NONE;

        RecentlyClosedBridge bridge =
                new RecentlyClosedBridge(getProfile(), (TabModelSelector) mModelDelegate);
        try {
            List<RecentlyClosedEntry> entries = bridge.getRecentlyClosedEntries(1);
            if (entries == null || entries.isEmpty()) {
                return RecentlyClosedEntryType.NONE;
            }
            RecentlyClosedEntry entry = entries.get(0);

            if (entry instanceof RecentlyClosedTab) {
                return RecentlyClosedEntryType.TAB;
            } else if (entry instanceof RecentlyClosedBulkEvent) {
                return RecentlyClosedEntryType.TABS;
            } else if (entry instanceof RecentlyClosedGroup) {
                return RecentlyClosedEntryType.GROUP;
            } else {
                return RecentlyClosedEntryType.NONE;
            }
        } finally {
            bridge.destroy();
        }
    }

    @Override
    public TabList getComprehensiveModel() {
        if (!supportsPendingClosures()) return this;
        return mPendingTabClosureManager.getRewoundList();
    }

    @Override
    public NullableObservableSupplier<Tab> getCurrentTabSupplier() {
        return mCurrentTabSupplier;
    }

    @Override
    public void setIndex(int i, @TabSelectionType int type) {
        assertOnUiThread();
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            setIndexInternal(i, type);
        }
    }

    @Override
    public boolean isActiveModel() {
        assertOnUiThread();
        return mActive;
    }

    @Override
    public boolean isInitializationComplete() {
        assertOnUiThread();
        return mInitializationComplete;
    }

    @Override
    public void moveTab(@TabId int id, int newIndex) {
        assertOnUiThread();

        Tab tab = getTabById(id);
        if (tab == null) return;

        int currentIndex = indexOf(tab);
        if (currentIndex == TabList.INVALID_TAB_INDEX || currentIndex == newIndex) return;

        // Clamp negative values here to ensure the tab moves to index 0 if negative. The size_t
        // cast in C++ otherwise results in the tab going to the end of the list which is not
        // intended.
        newIndex = Math.max(0, newIndex);
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            moveTabInternal(
                    tab,
                    currentIndex,
                    newIndex,
                    tab.getTabGroupId(),
                    tab.getIsPinned(),
                    /* isDestinationTab= */ false);
        }
    }

    @Override
    public void pinTab(
            int tabId,
            boolean showUngroupDialog,
            @Nullable TabModelActionListener tabModelActionListener) {
        Tab tab = getTabById(tabId);
        if (tab == null) return;
        if (tab.getIsPinned()) return;

        TabPinnerActionListener listener =
                new TabPinnerActionListener(
                        () -> updatePinnedState(tabId, /* isPinned= */ true),
                        tabModelActionListener);
        getTabUngrouper()
                .ungroupTabs(
                        Collections.singletonList(tab),
                        /* trailing= */ true,
                        showUngroupDialog,
                        listener);
        listener.pinIfCollaborationDialogShown();
    }

    @Override
    public void unpinTab(int tabId) {
        updatePinnedState(tabId, /* isPinned= */ false);
    }

    @Override
    public NonNullObservableSupplier<Integer> getTabCountSupplier() {
        assertOnUiThread();
        return mTabCountSupplier;
    }

    @Override
    public TabCreator getTabCreator() {
        assertOnUiThread();
        return getTabCreator(isIncognito());
    }

    @Override
    public void moveTabToWindow(Tab tab, Activity activity, int newIndex) {
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            mModelDelegate.moveTabToWindow(tab, activity, newIndex);
        }
    }

    @Override
    public void moveTabGroupToWindow(Token tabGroupId, Activity activity, int newIndex) {
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            mModelDelegate.moveTabGroupToWindow(tabGroupId, activity, newIndex, isIncognito());
        }
    }

    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        assertOnUiThread();

        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            addTabInternal(tab, index, type, creationState);
        }
    }

    @Override
    public void setTabsMultiSelected(Set<Integer> tabIds, boolean isSelected) {
        assertOnUiThread();
        TabModelImplUtil.setTabsMultiSelected(
                tabIds, isSelected, mMultiSelectedTabs, mTabModelObservers);
        assert mMultiSelectedTabs.isEmpty()
                        || TabModelUtils.getCurrentTabId(this) == Tab.INVALID_TAB_ID
                        || mMultiSelectedTabs.contains(TabModelUtils.getCurrentTabId(this))
                : "If the selection is not empty, the current tab must always be present within the"
                        + " set.";
    }

    @Override
    public void clearMultiSelection(boolean notifyObservers) {
        assertOnUiThread();
        TabModelImplUtil.clearMultiSelection(
                notifyObservers, mMultiSelectedTabs, mTabModelObservers);
    }

    @Override
    public boolean isTabMultiSelected(int tabId) {
        assertOnUiThread();
        return TabModelImplUtil.isTabMultiSelected(tabId, mMultiSelectedTabs, this);
    }

    @Override
    public int getMultiSelectedTabsCount() {
        assertOnUiThread();
        Tab oldSelectedTab = mCurrentTabSupplier.get();
        if (oldSelectedTab == null) return 0;
        // If no other tabs are in multi-selection, this returns 1, as the active tab is always
        // considered selected.
        return mMultiSelectedTabs.isEmpty() ? 1 : mMultiSelectedTabs.size();
    }

    @Override
    public int findFirstNonPinnedTabIndex() {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return 0;
        return TabCollectionTabModelImplJni.get()
                .getIndexOfFirstNonPinnedTab(mNativeTabCollectionTabModelImplPtr);
    }

    @Override
    public @Nullable TabStripCollection getTabStripCollection() {
        if (mNativeTabCollectionTabModelImplPtr == 0) return null;
        return TabCollectionTabModelImplJni.get()
                .getTabStripCollection(mNativeTabCollectionTabModelImplPtr);
    }

    // TabCloser overrides.

    @Override
    public boolean closeTabs(TabClosureParams params) {
        assertOnUiThread();
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            return closeTabsInternal(params);
        }
    }

    // TabModelInternal overrides.

    @Override
    public void completeInitialization() {
        // NOTE: This method is only called on the regular tab model. Incognito tab models do not
        // get notified.
        assertOnUiThread();
        assert !mInitializationComplete : "TabCollectionTabModelImpl initialized multiple times.";
        mInitializationComplete = true;

        if (getCount() != 0 && mCurrentTabSupplier.get() == null) {
            if (isActiveModel()) {
                setIndex(0, TabSelectionType.FROM_USER);
            } else {
                mCurrentTabSupplier.set(getTabAt(0));
            }
        }

        for (TabModelObserver observer : mTabModelObservers) observer.restoreCompleted();
    }

    @Override
    public void removeTab(Tab tab) {
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            removeTabsAndSelectNext(
                    Collections.singletonList(tab),
                    /* recommendedNextTab= */ null,
                    TabSelectionType.FROM_CLOSE,
                    /* isUndoable= */ false,
                    TabCloseType.SINGLE,
                    DetachReason.INSERT_INTO_OTHER_WINDOW);
        }

        for (TabModelObserver obs : mTabModelObservers) obs.tabRemoved(tab);
    }

    @Override
    public void setActive(boolean active) {
        mActive = active;
    }

    // TabModelJniBridge overrides.

    @Override
    public void initializeNative(@ActivityType int activityType, @TabModelType int tabModelType) {
        super.initializeNative(activityType, tabModelType);
        assert mNativeTabCollectionTabModelImplPtr == 0;
        mNativeTabCollectionTabModelImplPtr =
                TabCollectionTabModelImplJni.get().init(this, getProfile());
    }

    @Override
    protected TabCreator getTabCreator(boolean incognito) {
        return incognito ? mIncognitoTabCreator : mRegularTabCreator;
    }

    @Override
    protected boolean isSessionRestoreInProgress() {
        assertOnUiThread();
        return !mModelDelegate.isTabModelRestored();
    }

    @Override
    protected void moveTabToIndex(Tab tab, int newIndex) {
        moveTab(tab.getId(), newIndex);
    }

    @Override
    protected void moveGroupToIndex(Token tabGroupId, int newIndex) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return;

        List<Tab> tabs = getTabsInGroup(tabGroupId);
        if (tabs.isEmpty()) return;

        Tab firstTab = tabs.get(0);
        int curIndex = indexOf(firstTab);
        int oldIndex = curIndex;

        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            observer.willMoveTabGroup(tabGroupId, curIndex);
        }

        int finalIndex =
                TabCollectionTabModelImplJni.get()
                        .moveTabGroupTo(mNativeTabCollectionTabModelImplPtr, tabGroupId, newIndex);

        if (finalIndex == curIndex) return;

        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            for (TabModelObserver observer : mTabModelObservers) {
                observer.didMoveTab(tab, finalIndex + i, curIndex + i);
            }
        }

        int offset = tabs.size() - 1;
        Tab lastTab = tabs.get(offset);
        curIndex += offset;
        finalIndex += offset;
        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            observer.didMoveTabGroup(lastTab, curIndex, finalIndex);
        }
        for (TabModelObserver observer : mTabModelObservers) {
            observer.onTabGroupMoved(tabGroupId, oldIndex);
        }
    }

    @Override
    protected List<Tab> getAllTabs() {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return Collections.emptyList();
        return TabCollectionTabModelImplJni.get().getAllTabs(mNativeTabCollectionTabModelImplPtr);
    }

    @Override
    protected boolean containsTabGroup(Token tabGroupId) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return false;
        return TabCollectionTabModelImplJni.get()
                .tabGroupExists(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    @Override
    protected List<Token> listTabGroups() {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return Collections.emptyList();
        return TabCollectionTabModelImplJni.get()
                .getAllTabGroupIds(mNativeTabCollectionTabModelImplPtr);
    }

    @Override
    protected int[] getTabGroupTabIndices(Token tabGroupId) {
        List<Tab> tabs = getTabsInGroup(tabGroupId);
        if (tabs.isEmpty()) return new int[] {};

        Tab firstTab = tabs.get(0);
        int firstIndex = indexOf(firstTab);
        assert firstIndex != INVALID_TAB_INDEX;

        Tab lastTab = tabs.get(tabs.size() - 1);
        int lastIndex = indexOf(lastTab);
        assert lastIndex != INVALID_TAB_INDEX;

        // The returned array stores 2 values representing a range. The lastIndex + 1 is odd, but we
        // use non-inclusive tab index ranges for tab groups. See native TabGroup::ListTabs().
        return new int[] {firstIndex, lastIndex + 1};
    }

    @Override
    protected @Nullable Token createTabGroup(List<Tab> tabs) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return null;
        if (tabs.isEmpty()) return null;
        Tab tab = tabs.get(0);
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            mergeListOfTabsToGroup(tabs, tab, /* notify= */ NOTIFY_IF_NOT_NEW_GROUP);
        }
        // All tabs will have the same group ID, so return the first one.
        return tab.getTabGroupId();
    }

    @Override
    protected @Nullable Token addTabsToGroup(@Nullable Token tabGroupId, List<Tab> tabs) {
        assertOnUiThread();
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            return addTabsToGroupInternal(tabGroupId, tabs);
        }
    }

    @Override
    protected void setTabGroupVisualData(
            Token tabGroupId,
            String title,
            @TabGroupColorId int colorId,
            boolean isCollapsed,
            boolean animate) {
        updateTabGroupVisualData(tabGroupId, title, colorId, isCollapsed, animate);
    }

    /**
     * Internal helper to update tab group visual data.
     *
     * <p>Inputs of {@code null} signify "unchanged", supporting partial updates. If a property is
     * {@code null}, its current value will not be modified or written to storage.
     */
    private void updateTabGroupVisualData(
            Token tabGroupId,
            @Nullable String title,
            @Nullable Integer colorId,
            @Nullable Boolean isCollapsed,
            boolean animate) {
        assertOnUiThread();

        boolean isCached = TabGroupVisualDataStore.isTabGroupCachedForRestore(tabGroupId);

        boolean titleChanged = false;
        if (title != null) {
            String currentTitle = TabGroupVisualDataStore.getTabGroupTitle(tabGroupId);
            if (isCached || !Objects.equals(currentTitle, title)) {
                TabGroupVisualDataStore.storeTabGroupTitle(tabGroupId, title);
                if (!Objects.equals(currentTitle, title)) {
                    titleChanged = true;
                }
            }
        }

        boolean colorChanged = false;
        @TabGroupColorId Integer sanitizedColorId = null;
        if (colorId != null) {
            int currentColorId = TabGroupVisualDataStore.getTabGroupColor(tabGroupId);
            if (isCached || currentColorId != colorId) {
                if (colorId == TabGroupColorUtils.INVALID_COLOR_ID) {
                    TabGroupVisualDataStore.deleteTabGroupColor(tabGroupId);
                } else {
                    TabGroupVisualDataStore.storeTabGroupColor(tabGroupId, colorId);
                }
                if (currentColorId != colorId) {
                    colorChanged = true;
                }
                sanitizedColorId =
                        (colorId == TabGroupColorUtils.INVALID_COLOR_ID)
                                ? TabGroupColorId.GREY
                                : colorId;
            }
        }

        boolean collapsedChanged = false;
        if (isCollapsed != null) {
            boolean currentCollapsed = TabGroupVisualDataStore.getTabGroupCollapsed(tabGroupId);
            if (isCached || currentCollapsed != isCollapsed) {
                TabGroupVisualDataStore.storeTabGroupCollapsed(tabGroupId, isCollapsed);
                if (currentCollapsed != isCollapsed) {
                    collapsedChanged = true;
                }
            }
        }

        if (!titleChanged && !colorChanged && !collapsedChanged) {
            return;
        }

        if (mNativeTabCollectionTabModelImplPtr != 0) {
            TabCollectionTabModelImplJni.get()
                    .updateTabGroupVisualData(
                            mNativeTabCollectionTabModelImplPtr,
                            tabGroupId,
                            titleChanged ? title : null,
                            sanitizedColorId,
                            collapsedChanged ? isCollapsed : null);
        }

        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            if (titleChanged) {
                // The title parameter is nullable to support partial updates (null indicates no
                // change), but titleChanged guarantees it is non-null here. assumeNonNull is
                // required because static analysis (NullAway) cannot track this dependency.
                observer.didChangeTabGroupTitle(tabGroupId, assumeNonNull(title));
            }
            if (colorChanged) {
                observer.didChangeTabGroupColor(tabGroupId, assumeNonNull(sanitizedColorId));
            }
            if (collapsedChanged) {
                observer.didChangeTabGroupCollapsed(
                        tabGroupId, assumeNonNull(isCollapsed), animate);
            }
        }
        for (TabModelObserver observer : mTabModelObservers) {
            observer.onTabGroupVisualsChanged(tabGroupId);
        }
    }

    @Override
    public boolean isClosingAllTabs() {
        if (mClosingTabsCount == null) return false;
        int tabCount = getCount();
        return tabCount == 0 || mClosingTabsCount == tabCount;
    }

    // TabGroupModelFilter overrides.

    @Override
    public void addTabGroupObserver(TabGroupModelFilterObserver observer) {
        assertOnUiThread();
        mTabGroupObservers.addObserver(observer);
    }

    @Override
    public void removeTabGroupObserver(TabGroupModelFilterObserver observer) {
        assertOnUiThread();
        mTabGroupObservers.removeObserver(observer);
    }

    @Override
    public TabModel getTabModel() {
        return this;
    }

    @Override
    public List<Tab> getRepresentativeTabList() {
        // TODO(crbug.com/429145597): TabGroupModelFilterImpl uses the last selected tab in a tab
        // group as the representative tab. Ideally, we'd change this to use the first tab in the
        // tab group as the representative tab or just use the tab group id instead of a tab.
        // However, the tab TabList* code still depends on this being the last selected tab. A
        // refactor of TabList* code is needed to change this and waiting for the tab collection
        // launch to do this is a better time.
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return Collections.emptyList();
        return TabCollectionTabModelImplJni.get()
                .getRepresentativeTabList(mNativeTabCollectionTabModelImplPtr);
    }

    @Override
    public int getIndividualTabAndGroupCount() {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return 0;
        return TabCollectionTabModelImplJni.get()
                .getIndividualTabAndGroupCount(mNativeTabCollectionTabModelImplPtr);
    }

    @Override
    public int getCurrentRepresentativeTabIndex() {
        IndexAndTab currentRepresentativeIndexAndTab = getCurrentRepresentativeIndexAndTab();
        return currentRepresentativeIndexAndTab.index;
    }

    @Override
    public @Nullable Tab getCurrentRepresentativeTab() {
        IndexAndTab currentRepresentativeIndexAndTab = getCurrentRepresentativeIndexAndTab();
        return currentRepresentativeIndexAndTab.tab;
    }

    @Override
    public @Nullable Tab getRepresentativeTabAt(int index) {
        // TODO(crbug.com/428692223): Revisit the performance of this method by doing it in C++.
        List<Tab> representativeTabList = getRepresentativeTabList();
        if (index < 0 || index >= representativeTabList.size()) return null;
        return representativeTabList.get(index);
    }

    @Override
    public int representativeIndexOf(@Nullable Tab tab) {
        // TODO(crbug.com/428692223): Revisit the performance of this method by doing it in C++.
        if (tab == null) return TabList.INVALID_TAB_INDEX;
        Token tabGroupId = tab.getTabGroupId();
        boolean isTabGroup = tabGroupId != null;

        List<Tab> representativeTabList = getRepresentativeTabList();
        for (int i = 0; i < representativeTabList.size(); i++) {
            Tab representativeTab = representativeTabList.get(i);
            if (representativeTab == tab
                    || (isTabGroup
                            && Objects.equals(tabGroupId, representativeTab.getTabGroupId()))) {
                return i;
            }
        }
        return TabList.INVALID_TAB_INDEX;
    }

    @Override
    public int getTabGroupCount() {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return 0;
        return TabCollectionTabModelImplJni.get()
                .getTabGroupCount(mNativeTabCollectionTabModelImplPtr);
    }

    @Override
    public int getTabCountForGroup(@Nullable Token tabGroupId) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0 || tabGroupId == null) return 0;
        return TabCollectionTabModelImplJni.get()
                .getTabCountForGroup(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    @Override
    public boolean tabGroupExists(@Nullable Token tabGroupId) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0 || tabGroupId == null) return false;
        return TabCollectionTabModelImplJni.get()
                .tabGroupExists(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    @Override
    public List<Tab> getRelatedTabList(@TabId int tabId) {
        Tab tab = getTabById(tabId);
        if (tab == null) return Collections.emptyList();
        if (tab.getTabGroupId() == null) return Collections.singletonList(tab);
        return getTabsInGroup(tab.getTabGroupId());
    }

    @Override
    public List<Tab> getTabsInGroup(@Nullable Token tabGroupId) {
        assertOnUiThread();
        if (tabGroupId == null) return Collections.emptyList();

        return TabCollectionTabModelImplJni.get()
                .getTabsInGroup(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    @Override
    public boolean isTabInTabGroup(Tab tab) {
        assertOnUiThread();
        return tab.getTabGroupId() != null;
    }

    @Override
    public int getIndexOfTabInGroup(Tab tab) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return TabList.INVALID_TAB_INDEX;
        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId == null) return TabList.INVALID_TAB_INDEX;
        return TabCollectionTabModelImplJni.get()
                .getIndexOfTabInGroup(mNativeTabCollectionTabModelImplPtr, tab, tabGroupId);
    }

    @Override
    public @TabId int getGroupLastShownTabId(@Nullable Token tabGroupId) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0
                || tabGroupId == null
                || !tabGroupExists(tabGroupId)) {
            return Tab.INVALID_TAB_ID;
        }
        Tab tab = getLastShownTabForGroup(tabGroupId);
        return tab != null ? tab.getId() : Tab.INVALID_TAB_ID;
    }

    @Override
    public void moveRelatedTabs(@TabId int id, int newIndex) {
        assertOnUiThread();
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            moveRelatedTabsInternal(id, newIndex);
        }
    }

    @Override
    public boolean willMergingCreateNewGroup(List<Tab> tabsToMerge) {
        assertOnUiThread();
        return getCandidateTabGroupIdsForMerge(tabsToMerge).isEmpty();
    }

    @Override
    public void createSingleTabGroup(Tab tab) {
        assertOnUiThread();
        assert tab.getTabGroupId() == null;

        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            mergeListOfTabsToGroup(
                    Collections.singletonList(tab), tab, /* notify= */ NOTIFY_IF_NOT_NEW_GROUP);
        }
    }

    @Override
    public void createTabGroupForTabGroupSync(List<Tab> tabs, Token tabGroupId) {
        if (tabs.isEmpty()) return;

        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            mergeListOfTabsToGroupInternal(
                    tabs,
                    tabs.get(0),
                    /* notify= */ DONT_NOTIFY,
                    /* indexInGroup= */ null,
                    tabGroupId);
        }
    }

    @Override
    public void mergeTabsToGroup(
            @TabId int sourceTabId, @TabId int destinationTabId, boolean skipUpdateTabModel) {
        Tab sourceTab = getTabById(sourceTabId);
        if (sourceTab == null) return;

        Tab destinationTab = getTabById(destinationTabId);
        if (destinationTab == null) return;

        List<Tab> tabsToMerge;
        Token sourceTabGroupId = sourceTab.getTabGroupId();
        if (sourceTabGroupId == null) {
            tabsToMerge = Collections.singletonList(sourceTab);
        } else {
            tabsToMerge = getTabsInGroup(sourceTabGroupId);
        }
        // TODO(crbug.com/441933200): skipUpdateTabModel should be renamed to "notify" to match the
        // signature of mergeListOfTabsToGroupInternal(). It is no longer used to skip updating the
        // tab model as that often left the tab model in an invalid intermediate state.
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            mergeListOfTabsToGroup(
                    tabsToMerge,
                    destinationTab,
                    skipUpdateTabModel ? DONT_NOTIFY : NOTIFY_IF_NOT_NEW_GROUP);
        }
    }

    @Override
    public void mergeListOfTabsToGroup(
            List<Tab> tabs,
            Tab destinationTab,
            @Nullable Integer indexInGroup,
            @MergeNotificationType int notify) {
        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            mergeListOfTabsToGroupInternal(
                    tabs,
                    destinationTab,
                    notify,
                    /* indexInGroup= */ indexInGroup,
                    /* tabGroupIdForNewGroup= */ null);
        }
    }

    @Override
    public TabUngrouper getTabUngrouper() {
        return mTabUngrouper;
    }

    @Override
    public void performUndoGroupOperation(UndoGroupMetadata undoGroupMetadata) {
        assertOnUiThread();

        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            performUndoGroupOperationInternal((UndoGroupMetadataImpl) undoGroupMetadata);
        }
    }

    @Override
    public void undoGroupOperationExpired(UndoGroupMetadata undoGroupMetadata) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return;

        UndoGroupMetadataImpl undoGroupMetadataImpl = (UndoGroupMetadataImpl) undoGroupMetadata;
        for (Token removedTabGroupId : undoGroupMetadataImpl.removedTabGroupIds) {
            closeDetachedTabGroup(removedTabGroupId);
        }
    }

    @Override
    public Set<Token> getAllTabGroupIds() {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return Collections.emptySet();
        return new HashSet<>(
                TabCollectionTabModelImplJni.get()
                        .getAllTabGroupIds(mNativeTabCollectionTabModelImplPtr));
    }

    @Override
    public int getValidPosition(Tab tab, int proposedPosition) {
        // Return the proposedPosition. In the TabGroupModelFilterImpl the implementation of this
        // method makes an effort to ensure tab groups remain contiguous. This behavior is now
        // enforced when operating on the TabStripCollection in C++ so this method can effectively
        // no-op.
        return proposedPosition;
    }

    @Override
    public boolean isTabModelRestored() {
        return mModelDelegate.isTabModelRestored();
    }

    @Override
    public boolean isTabGroupHiding(@Nullable Token tabGroupId) {
        assertOnUiThread();
        if (tabGroupId == null) return false;

        return mHidingTabGroups.contains(tabGroupId);
    }

    @Override
    public LazyOneshotSupplier<Set<Token>> getLazyAllTabGroupIds(
            List<Tab> tabsToExclude, boolean includePendingClosures) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) {
            return LazyOneshotSupplier.fromValue(Collections.emptySet());
        }

        if (!includePendingClosures && tabsToExclude.isEmpty()) {
            return LazyOneshotSupplier.fromSupplier(() -> getAllTabGroupIds());
        }

        // TODO(crbug.com/428692223): A dedicated JNI method for the !includePendingClosures case
        // would likely perform better as the tabs could be iterated over only a single time.
        Supplier<Set<Token>> supplier =
                () -> {
                    Set<Token> tabGroupIds = new HashSet<>();
                    TabList tabList = includePendingClosures ? getComprehensiveModel() : this;
                    for (Tab tab : tabList) {
                        if (tabsToExclude.contains(tab)) continue;
                        Token tabGroupId = tab.getTabGroupId();
                        if (tabGroupId != null) {
                            tabGroupIds.add(tabGroupId);
                        }
                    }
                    return tabGroupIds;
                };
        return LazyOneshotSupplier.fromSupplier(supplier);
    }

    @Override
    public String getTabGroupTitle(Token tabGroupId) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) {
            return UNSET_TAB_GROUP_TITLE;
        }
        return TabCollectionTabModelImplJni.get()
                .getTabGroupTitle(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    @Override
    public String getTabGroupTitle(Tab groupedTab) {
        Token tabGroupId = groupedTab.getTabGroupId();
        assert tabGroupId != null;
        return getTabGroupTitle(tabGroupId);
    }

    @Override
    public void setTabGroupTitle(Token tabGroupId, String title) {
        assertOnUiThread();
        updateTabGroupVisualData(
                tabGroupId,
                title,
                /* colorId= */ null,
                /* isCollapsed= */ null,
                /* animate= */ false);
    }

    @Override
    public void deleteTabGroupTitle(Token tabGroupId) {
        if (!tabGroupExists(tabGroupId)) return;
        setTabGroupTitle(tabGroupId, UNSET_TAB_GROUP_TITLE);
    }

    @Override
    public int getTabGroupColor(Token tabGroupId) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return TabGroupColorUtils.INVALID_COLOR_ID;
        return TabCollectionTabModelImplJni.get()
                .getTabGroupColor(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    @Override
    public @TabGroupColorId int getTabGroupColorWithFallback(Token tabGroupId) {
        int color = getTabGroupColor(tabGroupId);
        return color == TabGroupColorUtils.INVALID_COLOR_ID ? TabGroupColorId.GREY : color;
    }

    @Override
    public @TabGroupColorId int getTabGroupColorWithFallback(Tab groupedTab) {
        Token tabGroupId = groupedTab.getTabGroupId();
        assert tabGroupId != null;
        return getTabGroupColorWithFallback(tabGroupId);
    }

    @Override
    public void setTabGroupColor(Token tabGroupId, @TabGroupColorId int color) {
        assertOnUiThread();
        updateTabGroupVisualData(
                tabGroupId,
                /* title= */ null,
                color,
                /* isCollapsed= */ null,
                /* animate= */ false);
    }

    @Override
    public void deleteTabGroupColor(Token tabGroupId) {
        if (!tabGroupExists(tabGroupId)) return;
        updateTabGroupVisualData(
                tabGroupId,
                /* title= */ null,
                TabGroupColorUtils.INVALID_COLOR_ID,
                /* isCollapsed= */ null,
                /* animate= */ false);
    }

    @Override
    public boolean getTabGroupCollapsed(Token tabGroupId) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return false;
        return TabCollectionTabModelImplJni.get()
                .getTabGroupCollapsed(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    @Override
    public void setTabGroupCollapsed(Token tabGroupId, boolean isCollapsed, boolean animate) {
        assertOnUiThread();
        updateTabGroupVisualData(
                tabGroupId, /* title= */ null, /* colorId= */ null, isCollapsed, animate);
    }

    @Override
    public void deleteTabGroupCollapsed(Token tabGroupId) {
        if (!tabGroupExists(tabGroupId)) return;
        setTabGroupCollapsed(tabGroupId, false, false);
    }

    // TabGroupModelFilterInternal overrides.

    @Override
    public void markTabStateInitialized() {
        // Intentional no-op. This is handled by mModelDelegate#isTabModelRestored().
    }

    @Override
    public void moveTabOutOfGroupInDirection(@TabId int sourceTabId, boolean trailing) {
        assertOnUiThread();

        try (ScopedStorageBatch ignored = mBatchFactory.get()) {
            moveTabOutOfGroupInDirectionInternal(sourceTabId, trailing);
        }
    }

    protected void maybeAssertTabHasWebContents(Tab tab) {
        if (mTabModelType == TabModelType.STANDARD
                && ChromeFeatureList.sLoadAllTabsAtStartup.isEnabled()) {
            assert tab.getWebContents() != null
                    : "WebContents must be created before adding to a standard tab model if load"
                            + " all tabs at startup is enabled.";
        }
    }

    private void addTabInternal(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        commitAllTabClosures();
        assert !mTabIdToTabs.containsKey(tab.getId())
                : "Attempting to add a duplicate tab id=" + tab.getId();
        if (tab.isOffTheRecord() != isOffTheRecord()) {
            throw new IllegalStateException("Attempting to open a tab in the wrong model.");
        }
        if (mNativeTabCollectionTabModelImplPtr == 0) {
            assert false : "Trying to add a tab to a destroyed TabCollectionTabModelImpl.";
            return;
        }
        maybeAssertTabHasWebContents(tab);

        for (TabModelObserver obs : mTabModelObservers) obs.willAddTab(tab, type);

        // Clear the multi-selection set before adding the tab.
        clearMultiSelection(/* notifyObservers= */ false);
        boolean hasAnyTabs = mCurrentTabSupplier.get() != null;
        boolean selectTab =
                mOrderController.willOpenInForeground(type, isIncognito())
                        || (!hasAnyTabs && type == TabLaunchType.FROM_LONGPRESS_BACKGROUND);
        index = mOrderController.determineInsertionIndex(type, index, tab);

        boolean shouldSelectBackgroundTab = !isActiveModel() && !hasAnyTabs && !selectTab;
        if (shouldSelectBackgroundTab) {
            mCurrentTabSupplier.willSet(tab);
        }

        // TODO(crbug.com/437141942): Update the list of undoable tabs instead of
        // committing it.
        commitAllTabClosures();

        Tab parentTab = getTabById(tab.getParentId());
        boolean groupWithParent = shouldGroupWithParent(tab, parentTab);
        if (groupWithParent) {
            assumeNonNull(parentTab);
            if (parentTab.getTabGroupId() == null) {
                createSingleTabGroup(parentTab);
                RecordUserAction.record("TabGroup.Created.OpenInNewTab");
            }
            tab.setTabGroupId(parentTab.getTabGroupId());
        }

        Token tabGroupId = tab.getTabGroupId();
        assert !(tabGroupId != null && tab.getIsPinned())
                : "Pinned and grouped states are mutually exclusive.";

        boolean createNewGroup = tabGroupId != null && !tabGroupExists(tabGroupId);
        if (createNewGroup) {
            assumeNonNull(tabGroupId);
            TabGroupVisualDataStore.migrateToTokenKeyedStorage(tab.getRootId(), tabGroupId);
            createDetachedTabGroup(tabGroupId);
        }
        // When migrating to tab collections we cease the use of root id. After reading
        // any
        // necessary data to restore a tab group's metadata we no longer need the root
        // id and can
        // reset it to the tab's id. If tab collections is turned off
        // TabGroupModelFilterImpl has a
        // back-migration pathway that rebuilds the correct root id structure from tab
        // group id.
        tab.setRootId(tab.getId());

        int finalIndex =
                TabCollectionTabModelImplJni.get()
                        .addTabRecursive(
                                mNativeTabCollectionTabModelImplPtr,
                                tab,
                                index,
                                tabGroupId,
                                createNewGroup,
                                tab.getIsPinned());

        // When adding the first background tab make sure to select it.
        if (shouldSelectBackgroundTab) {
            mCurrentTabSupplier.set(tab);
        }

        tab.onAddedToTabModel(mCurrentTabSupplier, this::isTabMultiSelected);
        mTabIdToTabs.put(tab.getId(), tab);
        mTabCountSupplier.set(getCount());

        if (tabGroupId != null && getTabsInGroup(tabGroupId).size() == 1) {
            setLastShownTabForGroup(tabGroupId, tab);
        }

        if (supportsPendingClosures()) {
            mPendingTabClosureManager.notifyTabAdded(tab, finalIndex);
        }

        tabAddedToModel(tab);
        for (TabModelObserver obs : mTabModelObservers) {
            obs.didAddTab(tab, type, creationState, selectTab);
        }
        if (groupWithParent) {
            // TODO(crbug.com/434015906): The sequencing here is incorrect as the tab is
            // already
            // grouped at this point; however, current clients don't care and we may be
            // able to
            // remove `willMergeTabToGroup` from the observer interface entirely one tab
            // collections
            // is fully launched.

            // Wait until after didAddTab before notifying observers so the tabs are
            // present in the
            // collection.
            for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                observer.willMergeTabToGroup(tab, Tab.INVALID_TAB_ID, tabGroupId);
                observer.didMergeTabToGroup(tab, /* isDestinationTab= */ false);
            }
        }

        if (selectTab) setIndex(finalIndex, TabSelectionType.FROM_NEW);
    }

    private void performUndoGroupOperationInternal(UndoGroupMetadataImpl undoGroupMetadata) {
        if (mNativeTabCollectionTabModelImplPtr == 0) return;

        Token tabGroupId = undoGroupMetadata.getTabGroupId();

        // Move each of the merged tabs back to their original state in reverse order. If the
        // destination tab was moved it will be moved last.
        List<UndoGroupTabData> mergedTabs = undoGroupMetadata.mergedTabsData;
        for (int i = mergedTabs.size() - 1; i >= 0; i--) {
            UndoGroupTabData undoTabData = mergedTabs.get(i);
            Tab mergedTab = undoTabData.tab;
            Token originalTabGroupId = undoTabData.originalTabGroupId;
            boolean wasSingleOrRestoredGroup = !tabGroupExists(originalTabGroupId);
            moveTabInternal(
                    mergedTab,
                    indexOf(mergedTab),
                    undoTabData.originalIndex,
                    originalTabGroupId,
                    undoTabData.originalIsPinned,
                    wasSingleOrRestoredGroup);
            // Restore the tab group information in case it was deleted or otherwise lost.
            if (wasSingleOrRestoredGroup && originalTabGroupId != null) {
                restoreTabGroupVisualData(originalTabGroupId);
            }
        }

        // If the destination tab adopted the metadata of an existing tab group, move the adopted
        // tab group back to its original position.
        if (undoGroupMetadata.adoptedTabGroupOriginalIndex != INVALID_TAB_INDEX) {
            moveGroupToIndex(tabGroupId, undoGroupMetadata.adoptedTabGroupOriginalIndex);
        }

        // Reset or delete the state of the undone group.
        if (undoGroupMetadata.adoptedTabGroupTitle) {
            deleteTabGroupTitle(tabGroupId);
        }
        if (undoGroupMetadata.didCreateNewGroup) {
            closeDetachedTabGroup(tabGroupId);
        } else if (undoGroupMetadata.wasDestinationTabGroupCollapsed) {
            setTabGroupCollapsed(tabGroupId, true);
        }
    }

    private void moveRelatedTabsInternal(@TabId int id, int newIndex) {
        Tab tab = getTabById(id);
        if (tab == null) return;

        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId != null) {
            moveGroupToIndex(tabGroupId, newIndex);
            return;
        }

        // TODO(crbug.com/433947821): TabListMediator uses this API for individual tab reordering
        // and expects to get a notification that a group has moved for each tab. However, a single
        // tab is not a group. We should consider refactoring TabListMediator to use a different API
        // for individual tab reordering (or also listen to didMoveTab()).
        int curIndex = indexOf(tab);
        int finalIndex =
                moveTabInternal(
                        tab,
                        curIndex,
                        newIndex,
                        /* newTabGroupId= */ null,
                        /* isPinned= */ tab.getIsPinned(),
                        /* isDestinationTab= */ false);
        if (finalIndex != curIndex) {
            for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                observer.didMoveTabGroup(tab, curIndex, finalIndex);
            }
        }
    }

    private @Nullable Token addTabsToGroupInternal(@Nullable Token tabGroupId, List<Tab> tabs) {
        if (tabs.isEmpty()) return null;

        // In this method we explicitly use ungroup() before grouping to protect any collaborations
        // from being destroyed.

        // Case 1: Create a new group.
        if (tabGroupId == null) {
            ungroup(tabs);
            Tab destinationTab = tabs.get(0);
            mergeListOfTabsToGroup(tabs, destinationTab, /* notify= */ DONT_NOTIFY);
            return destinationTab.getTabGroupId();
        }

        // Case 2: Add tabs to an existing group.
        List<Tab> tabsInGroup = getTabsInGroup(tabGroupId);
        if (tabsInGroup.isEmpty()) return null;

        List<Tab> tabsToUngroup = new ArrayList<>();
        for (Tab tab : tabs) {
            if (!tabsInGroup.contains(tab)) {
                tabsToUngroup.add(tab);
            }
        }
        ungroup(tabsToUngroup);
        mergeListOfTabsToGroup(tabs, tabsInGroup.get(0), /* notify= */ DONT_NOTIFY);
        return tabGroupId;
    }

    private boolean closeTabsInternal(TabClosureParams params) {
        boolean allowUndo = !params.uponExit && params.allowUndo && supportsPendingClosures();

        if (!allowUndo) {
            // The undo stacks assumes that previous actions in the stack are undoable. If an entry
            // is not undoable then the reversal of the operations may fail or yield an invalid
            // state. Commit the rest of the closures now to ensure that doesn't occur.
            commitAllTabClosures();
        }

        if (mNativeTabCollectionTabModelImplPtr == 0) return false;

        boolean canHideTabGroups = params.hideTabGroups && canHideTabGroups();

        final List<Tab> tabsToClose;
        if (params.isAllTabs) {
            tabsToClose = getAllTabs();
            if (canHideTabGroups) {
                for (Token tabGroupId : getAllTabGroupIds()) {
                    mHidingTabGroups.add(tabGroupId);
                }
            }
        } else {
            tabsToClose = new ArrayList<>(assumeNonNull(params.tabs));
            if (canHideTabGroups) {
                Set<Tab> closingTabIds = new HashSet<>(tabsToClose);
                for (Token tabGroupId : getAllTabGroupIds()) {
                    if (closingTabIds.containsAll(getTabsInGroup(tabGroupId))) {
                        mHidingTabGroups.add(tabGroupId);
                    }
                }
            }
        }

        tabsToClose.removeIf(
                tab -> {
                    if (!mTabIdToTabs.containsKey(tab.getId())) {
                        assert false : "Attempting to close a tab that is not in the TabModel.";
                        return true;
                    } else if (tab.isClosing()) {
                        assert false : "Attempting to close a tab that is already closing.";
                        return true;
                    }
                    return false;
                });
        if (tabsToClose.isEmpty()) return false;

        addToClosingTabsCount(tabsToClose.size());

        for (Tab tab : tabsToClose) {
            tab.setClosing(true);
        }

        // TODO(crbug.com/381471263): Simplify tab closing related observer calls. The intent is for
        // there to be a single event for each stage of tab closing regardless of how many tabs were
        // closed together.
        List<Token> closingTabGroupIds =
                maybeSendCloseTabGroupEvent(tabsToClose, /* committing= */ false);
        if (params.tabCloseType == TabCloseType.MULTIPLE) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.willCloseMultipleTabs(allowUndo, tabsToClose);
            }
        } else if (params.tabCloseType == TabCloseType.ALL) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.willCloseAllTabs(isIncognito());
            }
        }

        Set<Integer> tabsToCloseIds = new HashSet<>();
        boolean didCloseAlone = params.tabCloseType == TabCloseType.SINGLE;
        for (Tab tab : tabsToClose) {
            tabsToCloseIds.add(tab.getId());
            for (TabModelObserver obs : mTabModelObservers) {
                obs.willCloseTab(tab, didCloseAlone);
            }
        }

        if (tabsToCloseIds.size() == getCount()) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.allTabsAreClosing();
            }
        }

        if (!allowUndo) {
            notifyOnFinishingMultipleTabClosure(tabsToClose, params.saveToTabRestoreService);
        }

        @TabSelectionType
        int selectionType =
                params.uponExit ? TabSelectionType.FROM_EXIT : TabSelectionType.FROM_CLOSE;
        removeTabsAndSelectNext(
                tabsToClose,
                params.recommendedNextTab,
                selectionType,
                allowUndo,
                params.tabCloseType,
                DetachReason.DELETE);

        for (Tab tab : tabsToClose) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.didRemoveTabForClosure(tab);
            }
        }

        if (allowUndo) {
            assumeNonNull(mPendingTabClosureManager);
            mPendingTabClosureManager.addTabClosureEvent(tabsToClose, params.undoRunnable);

            boolean isAllTabs = params.tabCloseType == TabCloseType.ALL;
            for (TabModelObserver obs : mTabModelObservers) {
                obs.onTabClosePending(tabsToClose, isAllTabs, params.tabClosingSource);
            }
        } else {
            for (Tab tab : tabsToClose) {
                finalizeTabClosure(
                        tab, /* notifyTabClosureCommitted= */ false, params.tabClosingSource);
            }
        }

        for (Token tabGroupId : closingTabGroupIds) {
            for (TabGroupModelFilterObserver obs : mTabGroupObservers) {
                obs.didRemoveTabGroup(
                        Tab.INVALID_TAB_ID, tabGroupId, DidRemoveTabGroupReason.CLOSE);
            }
        }

        setTabsMultiSelected(tabsToCloseIds, /* isSelected= */ false);

        return true;
    }

    private void setIndexInternal(int i, @TabSelectionType int type) {
        // TODO(crbug.com/425344200): Prevent passing negative indices.
        if (isArchivedTabModel()) return;
        if (mNativeTabCollectionTabModelImplPtr == 0) return;

        // Batch service binding updates for the tabs becoming active and inactive. The activeness
        // change usually causes visibility changes, which updates service bindings of subframes at
        // the same time.
        try (ScopedServiceBindingBatch scope = ScopedServiceBindingBatch.scoped()) {
            // When we select a tab in this model it should become the active model. This is the
            // existing behavior of TabModelImpl.
            if (!isActiveModel()) mModelDelegate.selectModel(isIncognito());

            Tab oldSelectedTab = mCurrentTabSupplier.get();
            int lastId = (oldSelectedTab == null) ? Tab.INVALID_TAB_ID : oldSelectedTab.getId();

            int currentTabCount = getCount();
            final Tab newSelectedTab;
            if (currentTabCount == 0) {
                newSelectedTab = null;
            } else {
                newSelectedTab = getTabAt(MathUtils.clamp(i, 0, currentTabCount - 1));
            }

            mCurrentTabSupplier.willSet(newSelectedTab);
            mModelDelegate.requestToShowTab(newSelectedTab, type);
            mCurrentTabSupplier.set(newSelectedTab);

            if (newSelectedTab != null) {
                Token tabGroupId = newSelectedTab.getTabGroupId();
                boolean isInGroup = tabGroupId != null;
                if (isInGroup) {
                    assumeNonNull(tabGroupId);
                    setLastShownTabForGroup(tabGroupId, newSelectedTab);
                }
                RecordHistogram.recordBooleanHistogram(
                        "TabGroups.SelectedTabInTabGroup", isInGroup);

                for (TabModelObserver obs : mTabModelObservers) {
                    obs.didSelectTab(newSelectedTab, type, lastId);
                    // Required, otherwise the previously active tab will have MULTISELECTED as its
                    // VisualState.
                    obs.onTabsSelectionChanged();
                }

                boolean wasAlreadySelected =
                        (newSelectedTab.getId() == lastId && lastId != Tab.INVALID_TAB_ID);
                if (!wasAlreadySelected && type == TabSelectionType.FROM_USER) {
                    RecordUserAction.record("MobileTabSwitched");
                }
            }
        }
    }

    private void openMostRecentlyClosedEntryInternal() {
        if (supportsPendingClosures() && mPendingTabClosureManager.openMostRecentlyClosedEntry()) {
            return;
        }

        mModelDelegate.openMostRecentlyClosedEntry(this);
        Tab oldSelectedTab = mCurrentTabSupplier.get();
        if (oldSelectedTab == null) {
            setIndex(0, TabSelectionType.FROM_NEW);
        }
    }

    private void moveTabOutOfGroupInDirectionInternal(@TabId int sourceTabId, boolean trailing) {
        Tab sourceTab = getTabById(sourceTabId);
        if (sourceTab == null) return;

        Token oldTabGroupId = sourceTab.getTabGroupId();
        if (oldTabGroupId == null) return;

        List<Tab> tabsInGroup = getTabsInGroup(oldTabGroupId);
        assert tabsInGroup.size() > 0;
        final int approximateIndex;
        if (trailing) {
            approximateIndex = indexOf(tabsInGroup.get(tabsInGroup.size() - 1));
        } else {
            approximateIndex = indexOf(tabsInGroup.get(0));
        }
        moveTabInternal(
                sourceTab,
                indexOf(sourceTab),
                approximateIndex,
                /* newTabGroupId= */ null,
                /* isPinned= */ false,
                /* isDestinationTab= */ false);

        if (detachedTabGroupExists(oldTabGroupId)) {
            closeDetachedTabGroup(oldTabGroupId);
        }
    }

    // Internal methods.

    private boolean isArchivedTabModel() {
        return mTabModelType == TabModelType.ARCHIVED;
    }

    private boolean shouldGroupWithParent(Tab tab, @Nullable Tab parentTab) {
        if (parentTab == null) return false;

        @TabLaunchType int tabLaunchType = tab.getLaunchType();
        boolean shouldGroupWithParentForTabListInterface =
                tabLaunchType == TabLaunchType.FROM_TAB_LIST_INTERFACE
                        && parentTab.getTabGroupId() != null;

        return mModelDelegate.isTabModelRestored()
                && (tabLaunchType == TabLaunchType.FROM_TAB_GROUP_UI
                        || tabLaunchType == TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP
                        || tabLaunchType == TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP
                        || tabLaunchType == TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP
                        || shouldGroupWithParentForTabListInterface);
    }

    private void finalizeTabClosure(
            Tab tab, boolean notifyTabClosureCommitted, @TabClosingSource int closingSource) {
        decrementClosingTabsCount();
        mTabContentManager.removeTabThumbnail(tab.getId());

        for (TabModelObserver obs : mTabModelObservers) {
            obs.onFinishingTabClosure(tab, closingSource);
        }

        if (notifyTabClosureCommitted) {
            for (TabModelObserver obs : mTabModelObservers) obs.tabClosureCommitted(tab);
        }

        // Destroy the native tab after the observer notifications have fired, otherwise they risk a
        // use after free or null dereference.
        tab.destroy();
    }

    private boolean canHideTabGroups() {
        Profile profile = getProfile();
        if (profile == null || !profile.isNativeInitialized()) return false;

        return !isIncognito() && TabGroupSyncFeatures.isTabGroupSyncEnabled(profile);
    }

    private void removeTabsAndSelectNext(
            List<Tab> tabsToRemove,
            @Nullable Tab recommendedNextTab,
            @TabSelectionType int selectionType,
            boolean isUndoable,
            @TabCloseType int closeType,
            @DetachReason int detachReason) {
        assert selectionType == TabSelectionType.FROM_CLOSE
                || selectionType == TabSelectionType.FROM_EXIT;

        if (tabsToRemove.isEmpty()) return;

        boolean pauseMedia = isUndoable;
        boolean updatePendingTabClosureManager = !isUndoable;

        Tab currentTabInModel = mCurrentTabSupplier.get();
        if (recommendedNextTab != null && tabsToRemove.contains(recommendedNextTab)) {
            recommendedNextTab = null;
        }
        Tab nextTab =
                recommendedNextTab != null
                        ? recommendedNextTab
                        : TabModelImplUtil.getNextTabIfClosed(
                                this,
                                mModelDelegate,
                                mCurrentTabSupplier,
                                mNextTabPolicySupplier,
                                tabsToRemove,
                                /* uponExit= */ false,
                                closeType);

        if (updatePendingTabClosureManager) commitAllTabClosures();

        Tab nearbyTab = null;
        boolean nextIsIncognito = nextTab != null && nextTab.isOffTheRecord();
        boolean nextIsInOtherModel = nextIsIncognito != isIncognito();
        if ((nextTab == null || nextIsInOtherModel) && closeType != TabCloseType.ALL) {
            nearbyTab =
                    TabModelImplUtil.findNearbyNotClosingTab(
                            this, tabsToRemove.indexOf(currentTabInModel), tabsToRemove);
        }

        if (nextTab != currentTabInModel && nextIsInOtherModel) {
            mCurrentTabSupplier.willSet(nearbyTab);
        }

        Map<Token, @Nullable Tab> tabGroupShownTabs = new HashMap<>();
        for (Tab tab : tabsToRemove) {
            assert mTabIdToTabs.containsKey(tab.getId()) : "Tab not found in tab model.";
            if (pauseMedia) TabUtils.pauseMedia(tab);

            Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId != null && !tabGroupShownTabs.containsKey(tabGroupId)) {
                Tab nextGroupTab = getNextLastShownTabForGroup(tabGroupId, tabsToRemove);
                setLastShownTabForGroup(tabGroupId, nextGroupTab);
                tabGroupShownTabs.put(tabGroupId, nextGroupTab);
            }

            // TODO(crbug.com/428692223): Vectorize this so all the tabs get removed from the
            // collection in a single pass.
            TabCollectionTabModelImplJni.get()
                    .removeTabRecursive(mNativeTabCollectionTabModelImplPtr, tab);
            tab.onRemovedFromTabModel(mCurrentTabSupplier, detachReason);
            mTabIdToTabs.remove(tab.getId());
        }
        mTabCountSupplier.set(getCount());

        if (nextTab != currentTabInModel) {
            if (nextIsInOtherModel) {
                mCurrentTabSupplier.set(nearbyTab);
            }

            TabModel nextModel = mModelDelegate.getModel(nextIsIncognito);
            nextModel.setIndex(nextModel.indexOf(nextTab), selectionType);
        }

        if (updatePendingTabClosureManager && supportsPendingClosures()) {
            mPendingTabClosureManager.resetState();
        }

        if (pauseMedia) {
            for (Tab tab : tabsToRemove) {
                if (!TabUtils.isCapturingForMedia(tab)) continue;
                // If media is being captured discard the tab to disconnect it.
                tab.discard();
            }
        }

        if (!isUndoable) {
            for (Map.Entry<Token, @Nullable Tab> tabGroupShownTab : tabGroupShownTabs.entrySet()) {
                if (tabGroupShownTab.getValue() == null) {
                    closeDetachedTabGroup(tabGroupShownTab.getKey());
                }
            }
        }
    }

    private IndexAndTab getCurrentRepresentativeIndexAndTab() {
        // TODO(crbug.com/428692223): Revisit the performance of this method by doing it in C++.
        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab == null) return new IndexAndTab(TabList.INVALID_TAB_INDEX, null);

        Token currentTabGroupId = currentTab.getTabGroupId();
        List<Tab> representativeTabList = getRepresentativeTabList();
        for (int i = 0; i < representativeTabList.size(); i++) {
            Tab tab = representativeTabList.get(i);
            if (tab == currentTab
                    || (currentTabGroupId != null
                            && currentTabGroupId.equals(tab.getTabGroupId()))) {
                return new IndexAndTab(i, tab);
            }
        }
        assert false : "Current tab not found in representative tab list.";
        return new IndexAndTab(TabList.INVALID_TAB_INDEX, null);
    }

    private void updatePinnedState(int tabId, boolean isPinned) {
        Tab tab = getTabById(tabId);
        if (tab == null || isPinned == tab.getIsPinned()) return;

        int currentIndex = indexOf(tab);
        if (currentIndex == TabList.INVALID_TAB_INDEX) return;

        if (isPinned) {
            recordPinTimestamp(tab);

            WebContents webContents = tab.getWebContents();
            if (webContents != null) {
                new UkmRecorder(webContents, UKM_METRICS_TAB_STATE_CHANGED)
                        .addBooleanMetric("IsPinned")
                        .record();
            }

        } else {
            recordPinnedDuration(tab);
        }

        // The C++ side will adjust to a valid index.
        moveTabInternal(
                tab,
                currentIndex,
                currentIndex,
                /* newTabGroupId= */ null,
                isPinned,
                /* isDestinationTab= */ false);
    }

    /**
     * Merges a list of tabs into a destination group at a specified position.
     *
     * @param tabs The list of {@link Tab}s to merge.
     * @param destinationTab The destination {@link Tab} identifying the target group.
     * @param notify Whether to notify observers to show UI like snackbars.
     * @param indexInGroup The index within the destination group to insert the tabs. Null appends
     *     to the end.
     * @param tabGroupIdForNewGroup A specific {@link Token} to use if a new group is created. Null
     *     will generate a random one.
     */
    @VisibleForTesting
    void mergeListOfTabsToGroupInternal(
            List<Tab> tabs,
            Tab destinationTab,
            @MergeNotificationType int notify,
            @Nullable Integer indexInGroup,
            @Nullable Token tabGroupIdForNewGroup) {
        assertOnUiThread();

        if (mNativeTabCollectionTabModelImplPtr == 0) return;

        Token maybeDestinationTabGroupId = destinationTab.getTabGroupId();
        if (areAnyTabsPartOfSharedGroup(this, tabs, maybeDestinationTabGroupId)) return;

        List<Token> candidateTabGroupIds = getCandidateTabGroupIdsForMerge(tabs);
        boolean wasDestinationTabInGroup = maybeDestinationTabGroupId != null;
        boolean willCreateNewGroup = candidateTabGroupIds.isEmpty() && !wasDestinationTabInGroup;
        assert tabGroupIdForNewGroup == null
                        || willCreateNewGroup
                        || tabGroupIdForNewGroup.equals(maybeDestinationTabGroupId)
                : "A new tab group ID should not be provided if the merge contains a tab group"
                        + " unless it matches the destination tab's group ID.";

        // Find a destination tab group ID.
        final Token destinationTabGroupId;
        final boolean adoptCandidateGroupId;
        if (wasDestinationTabInGroup) {
            // Case 1: The destination tab is already part of a group we will reuse it.
            destinationTabGroupId = maybeDestinationTabGroupId;
            adoptCandidateGroupId = false;
        } else if (willCreateNewGroup) {
            // Case 2: None of the tabs are part of a group and we will create a new group.
            destinationTabGroupId =
                    tabGroupIdForNewGroup == null ? Token.createRandom() : tabGroupIdForNewGroup;
            createDetachedTabGroup(destinationTabGroupId);
            adoptCandidateGroupId = false;
        } else {
            // Case 3: At least one of the tabs being merged is part of a group. Reuse the first
            // group found's metadata.
            destinationTabGroupId = candidateTabGroupIds.get(0);
            adoptCandidateGroupId = true;
        }
        assert destinationTabGroupId != null;
        // All other candidate groups will be consumed/deleted. Ensure the destinationTabGroupId is
        // kept if it came from the list of candidateTabGroupIds.
        candidateTabGroupIds.remove(destinationTabGroupId);

        // If we are using an existing group that the destination tab is not part of we need to
        // move the group to the index of the destination tab.
        int adoptedTabGroupIndex = INVALID_TAB_INDEX;
        int originalDestinationIndex = indexOf(destinationTab);
        if (adoptCandidateGroupId) {
            List<Tab> tabsInAdoptedGroup = getTabsInGroup(destinationTabGroupId);
            adoptedTabGroupIndex = indexOf(tabsInAdoptedGroup.get(0));
            // If the undo operation will move the adopted group to a higher index, we need to
            // offset the restored index of the adopted group to account for its size.
            if (originalDestinationIndex < adoptedTabGroupIndex) {
                adoptedTabGroupIndex += tabsInAdoptedGroup.size() - 1;
            }
            assert indexInGroup == null
                    : "indexInGroup should not be set when adopting a candidate group.";
            moveGroupToIndex(destinationTabGroupId, originalDestinationIndex);
        }

        // Move the destination tab into the group if it is not already part of the group.
        int destinationTabIndex = indexOf(destinationTab);
        UndoGroupTabData undoGroupDestinationTabData = null;
        if (!wasDestinationTabInGroup) {
            undoGroupDestinationTabData =
                    new UndoGroupTabData(
                            destinationTab,
                            originalDestinationIndex,
                            destinationTab.getIsPinned(),
                            destinationTab.getTabGroupId());
            moveTabInternal(
                    destinationTab,
                    destinationTabIndex,
                    destinationTabIndex,
                    destinationTabGroupId,
                    /* isPinned= */ false,
                    /* isDestinationTab= */ true);
        }

        // Adopt the title of the first candidate group with a title that was merged into the
        // destination group if the destination group does not have a title.
        boolean adoptedGroupTitle = false;
        if (TextUtils.isEmpty(getTabGroupTitle(destinationTabGroupId))) {
            for (Token tabGroupId : candidateTabGroupIds) {
                String title = getTabGroupTitle(tabGroupId);
                if (!TextUtils.isEmpty(title)) {
                    adoptedGroupTitle = true;
                    setTabGroupTitle(destinationTabGroupId, title);
                    break;
                }
            }
        }

        // Ensure the destination group is not collapsed.
        boolean wasDestinationTabGroupCollapsed = getTabGroupCollapsed(destinationTabGroupId);
        if (wasDestinationTabGroupCollapsed) {
            setTabGroupCollapsed(destinationTabGroupId, false);
        }

        // Calculate the initial insertion point in the tab model.
        int destinationIndexInTabModel;
        if (wasDestinationTabInGroup || adoptCandidateGroupId) {
            // If we adopt a candidate group, indexInGroup should be null. ie, add all tabs to
            // after destination tab.
            List<Tab> tabsInDestGroup = getTabsInGroup(destinationTabGroupId);
            int groupSize = tabsInDestGroup.size();
            int insertionIndexInGroup =
                    (indexInGroup == null)
                            ? groupSize
                            : MathUtils.clamp(indexInGroup, 0, groupSize);
            Tab firstTabInGroup = tabsInDestGroup.get(0);
            int firstTabModelIndex = indexOf(firstTabInGroup);
            destinationIndexInTabModel = firstTabModelIndex + insertionIndexInGroup;
        } else {
            // New group is being formed; position relative to the destination tab.
            destinationIndexInTabModel =
                    (indexInGroup != null && indexInGroup == 0)
                            ? destinationTabIndex
                            : destinationTabIndex + 1;
        }

        UndoGroupMetadataImpl undoGroupMetadata =
                new UndoGroupMetadataImpl(
                        destinationTabGroupId,
                        isIncognito(),
                        undoGroupDestinationTabData,
                        adoptedTabGroupIndex,
                        candidateTabGroupIds,
                        willCreateNewGroup,
                        adoptedGroupTitle,
                        wasDestinationTabGroupCollapsed);

        // Move all tabs into the destination group.
        for (Tab tab : tabs) {
            int currentIndex = indexOf(tab);
            assert currentIndex != TabModel.INVALID_TAB_INDEX;

            int adjustedDestinationIndexInTabModel = destinationIndexInTabModel;

            // A "move" is conceptually a "remove" then an "add". When moving a tab from
            // a position *before* the destination (e.g., from index 2 to 5), the removal
            // causes all subsequent elements, including the destination, to shift left by one.
            // We must decrement the destination index to compensate for this shift and ensure
            // the tab is inserted at the correct final position.
            //
            // This adjustment is not needed when moving a tab backward (e.g., from 5 to 2),
            // as the removal does not affect the indices of items that come before it.
            if (currentIndex < destinationIndexInTabModel) {
                adjustedDestinationIndexInTabModel--;
            }

            boolean oldIsPinned = tab.getIsPinned();
            Token oldTabGroupId = tab.getTabGroupId();

            // Iff the tab is already a part of the destination group, and is at the required index,
            // we can skip the move. We require the tab to be at the right index to ensure the order
            // of the tabs in the list of tabs being merged is replicated in the tab group.
            if (destinationTabGroupId.equals(oldTabGroupId)
                    && currentIndex == adjustedDestinationIndexInTabModel) {
                destinationIndexInTabModel++;
                continue;
            }

            moveTabInternal(
                    tab,
                    currentIndex,
                    adjustedDestinationIndexInTabModel,
                    destinationTabGroupId,
                    /* isPinned= */ false,
                    /* isDestinationTab= */ false);

            if (currentIndex >= destinationIndexInTabModel) {
                destinationIndexInTabModel++;
            }

            undoGroupMetadata.addMergedTab(tab, currentIndex, oldIsPinned, oldTabGroupId);
        }

        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            if (willCreateNewGroup) {
                observer.didCreateNewGroup(destinationTab, this);
            }

            for (Token tabGroupId : candidateTabGroupIds) {
                observer.didRemoveTabGroup(
                        Tab.INVALID_TAB_ID, tabGroupId, DidRemoveTabGroupReason.MERGE);
            }
        }

        if (willCreateNewGroup) {
            for (TabModelObserver observer : mTabModelObservers) {
                observer.onTabGroupCreated(destinationTabGroupId);
            }
        }

        if ((notify == NOTIFY_IF_NOT_NEW_GROUP && !willCreateNewGroup) || notify == NOTIFY_ALWAYS) {
            for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                observer.showUndoGroupSnackbar(undoGroupMetadata);
            }
        } else {
            for (Token tabGroupId : candidateTabGroupIds) {
                closeDetachedTabGroup(tabGroupId);
            }
        }
    }

    /**
     * Moves a tab to a new index. Firing observer events for any changes in pinned or tab group
     * state. Note the pinned and grouped states are mutually exclusive.
     *
     * @param tab The tab to move.
     * @param index The current index of the tab.
     * @param newIndex The new index of the tab. This might be adjusted in C++ to a valid index.
     * @param newTabGroupId The new tab group id of the tab.
     * @param isPinned Whether the tab is pinned.
     * @param isDestinationTab Whether the tab is the destination tab in a merge operation.
     * @return The final index of the tab.
     */
    private int moveTabInternal(
            Tab tab,
            int index,
            int newIndex,
            @Nullable Token newTabGroupId,
            boolean isPinned,
            boolean isDestinationTab) {
        assert newTabGroupId == null || !isPinned
                : "Pinned and grouped tabs are mutually exclusive.";

        // TODO(crbug.com/437141942): Update the list of undoable tabs instead of committing it.
        commitAllTabClosures();

        Token oldTabGroupId = tab.getTabGroupId();
        boolean isMovingWithinGroup = false;
        boolean isMovingOutOfGroup = false;
        if (oldTabGroupId != null) {
            isMovingWithinGroup = oldTabGroupId.equals(newTabGroupId);
            isMovingOutOfGroup = !isMovingWithinGroup;
        }
        // Moving tabs within a group does not count as merging despite newTabGroupId being
        // non-null. However, if the oldTabGroupId was null or does not match newTabGroupId we want
        // to fire this event if a newTabGroupId is provided.
        boolean isMergingIntoGroup = !isMovingWithinGroup && newTabGroupId != null;
        boolean isChangingPinState = tab.getIsPinned() != isPinned;

        if (isChangingPinState) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.willChangePinState(tab);
            }
        }

        if (isMovingOutOfGroup) {
            assumeNonNull(oldTabGroupId);
            for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                observer.willMoveTabOutOfGroup(tab, newTabGroupId);
            }
            if (getLastShownTabForGroup(oldTabGroupId) == tab) {
                Tab nextGroupTab =
                        getNextLastShownTabForGroup(oldTabGroupId, Collections.singletonList(tab));
                setLastShownTabForGroup(oldTabGroupId, nextGroupTab);
            }
        }

        if (isMergingIntoGroup) {
            assumeNonNull(newTabGroupId);
            for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                observer.willMergeTabToGroup(tab, Tab.INVALID_TAB_ID, newTabGroupId);
            }
            if (getLastShownTabForGroup(newTabGroupId) == null) {
                setLastShownTabForGroup(newTabGroupId, tab);
            }
        }

        int finalIndex =
                TabCollectionTabModelImplJni.get()
                        .moveTabRecursive(
                                mNativeTabCollectionTabModelImplPtr,
                                index,
                                newIndex,
                                newTabGroupId,
                                isPinned);

        // Ensure the current tab is always the last shown tab in its group.
        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab != null) {
            Token currentTabGroupId = currentTab.getTabGroupId();
            if (currentTabGroupId != null) {
                setLastShownTabForGroup(currentTabGroupId, currentTab);
            }
        }

        if (supportsPendingClosures()) {
            mPendingTabClosureManager.resetState();
        }

        if (index != finalIndex) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.didMoveTab(tab, finalIndex, index);
            }
        }

        if (isMovingWithinGroup) {
            for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                observer.didMoveWithinGroup(tab, index, finalIndex);
            }
        }

        if (isMovingOutOfGroup) {
            assumeNonNull(oldTabGroupId);
            boolean wasLastTabInGroup =
                    wasLastTabInGroupAndNotifyDidMoveTabOutOfGroup(tab, oldTabGroupId);
            if (wasLastTabInGroup && newTabGroupId == null) {
                final @DidRemoveTabGroupReason int reason;
                if (isPinned) {
                    reason = DidRemoveTabGroupReason.PIN;
                } else if (isMergingIntoGroup) {
                    reason = DidRemoveTabGroupReason.MERGE;
                } else {
                    reason = DidRemoveTabGroupReason.UNGROUP;
                }
                for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                    observer.didRemoveTabGroup(Tab.INVALID_TAB_ID, oldTabGroupId, reason);
                }
            }
        }

        if (isMergingIntoGroup) {
            for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                observer.didMergeTabToGroup(tab, isDestinationTab);
            }
        }

        if (isChangingPinState) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.didChangePinState(tab);
            }
        }
        return finalIndex;
    }

    private List<Token> getCandidateTabGroupIdsForMerge(List<Tab> tabsToMerge) {
        HashSet<Token> processedTabGroups = new HashSet<>();
        List<Token> candidateTabGroupIds = new ArrayList<>();
        for (Tab tab : tabsToMerge) {
            Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null || processedTabGroups.contains(tabGroupId)) continue;

            if (tabsToMerge.containsAll(getTabsInGroup(tabGroupId))) {
                candidateTabGroupIds.add(tabGroupId);
            }
            processedTabGroups.add(tabGroupId);
        }
        return candidateTabGroupIds;
    }

    private void createDetachedTabGroup(Token tabGroupId) {
        String title = TabGroupVisualDataStore.getTabGroupTitle(tabGroupId);

        int storedColorId = TabGroupVisualDataStore.getTabGroupColor(tabGroupId);
        @TabGroupColorId int colorId;
        if (storedColorId != TabGroupColorUtils.INVALID_COLOR_ID) {
            colorId = storedColorId;
        } else {
            colorId = TabGroupColorUtils.getNextSuggestedColorId(this);
            TabGroupVisualDataStore.storeTabGroupColor(tabGroupId, colorId);
        }
        boolean isCollapsed = TabGroupVisualDataStore.getTabGroupCollapsed(tabGroupId);

        TabCollectionTabModelImplJni.get()
                .createTabGroup(
                        mNativeTabCollectionTabModelImplPtr,
                        tabGroupId,
                        title,
                        colorId,
                        isCollapsed);
    }

    /**
     * Notifies observers that a tab has moved out of a group. Returns true if the tab was the last
     * tab in the group.
     */
    private boolean wasLastTabInGroupAndNotifyDidMoveTabOutOfGroup(Tab tab, Token oldTabGroupId) {
        int prevFilterIndex = representativeIndexOf(getLastShownTabForGroup(oldTabGroupId));
        boolean isLastTabInGroup = prevFilterIndex == TabList.INVALID_TAB_INDEX;
        if (isLastTabInGroup) {
            prevFilterIndex = representativeIndexOf(tab);
        }
        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            observer.didMoveTabOutOfGroup(tab, prevFilterIndex);
        }
        return isLastTabInGroup;
    }

    private @Nullable Tab getLastShownTabForGroup(Token tabGroupId) {
        return TabCollectionTabModelImplJni.get()
                .getLastShownTabForGroup(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    private void setLastShownTabForGroup(Token tabGroupId, @Nullable Tab tab) {
        TabCollectionTabModelImplJni.get()
                .setLastShownTabForGroup(mNativeTabCollectionTabModelImplPtr, tabGroupId, tab);
    }

    private @Nullable Tab getNextLastShownTabForGroup(Token tabGroupId, List<Tab> tabsToExclude) {
        List<Tab> tabsInGroup = getTabsInGroup(tabGroupId);
        if (tabsInGroup.isEmpty()) return null;

        Tab lastShownTab = getLastShownTabForGroup(tabGroupId);
        if (lastShownTab == null) {
            // TODO(crbug.com/428692223): Worst case O(n^2) it can be made faster with sets, but
            // this should be a very niche case and in most cases both lists will be very small.
            for (Tab tab : tabsInGroup) {
                if (!tabsToExclude.contains(tab)) return tab;
            }
            return null;
        }

        if (!tabsToExclude.contains(lastShownTab)) return lastShownTab;

        int indexInGroup = tabsInGroup.indexOf(lastShownTab);
        return TabModelImplUtil.findNearbyNotClosingTab(tabsInGroup, indexInGroup, tabsToExclude);
    }

    private void notifyOnFinishingMultipleTabClosure(
            List<Tab> tabs, boolean saveToTabRestoreService) {
        for (TabModelObserver obs : mTabModelObservers) {
            obs.onFinishingMultipleTabClosure(tabs, saveToTabRestoreService);
        }
        maybeSendCloseTabGroupEvent(tabs, /* committing= */ true);
    }

    private List<Token> maybeSendCloseTabGroupEvent(List<Tab> tabs, boolean committing) {
        LazyOneshotSupplier<Set<Token>> tabGroupIdsInComprehensiveModel =
                getLazyAllTabGroupIds(tabs, /* includePendingClosures= */ committing);
        Set<Token> processedTabGroups = new HashSet<>();
        List<Token> closingTabGroupIds = new ArrayList<>();
        for (Tab tab : tabs) {
            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null) continue;

            boolean alreadyProcessed = !processedTabGroups.add(tabGroupId);
            if (alreadyProcessed) continue;

            // If the tab group still exists in the comprehensive tab model we should not send an
            // event.
            if (assumeNonNull(tabGroupIdsInComprehensiveModel.get()).contains(tabGroupId)) continue;

            closingTabGroupIds.add(tabGroupId);

            boolean hiding;
            if (committing) {
                hiding = mHidingTabGroups.remove(tabGroupId);
                if (detachedTabGroupExists(tabGroupId)) {
                    closeDetachedTabGroup(tabGroupId);
                }
                for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                    observer.committedTabGroupClosure(tabGroupId, hiding);
                }
            } else {
                hiding = mHidingTabGroups.contains(tabGroupId);
                for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                    observer.willCloseTabGroup(tabGroupId, hiding);
                }
                for (TabModelObserver obs : mTabModelObservers) {
                    obs.onTabGroupRemoving(tabGroupId);
                }
            }
        }
        return closingTabGroupIds;
    }

    private void closeDetachedTabGroup(Token tabGroupId) {
        TabGroupVisualDataStore.deleteAllVisualDataForGroup(tabGroupId);
        TabCollectionTabModelImplJni.get()
                .closeDetachedTabGroup(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    private void restoreTabGroupVisualData(Token tabGroupId) {
        setTabGroupTitle(tabGroupId, getTabGroupTitle(tabGroupId));
        setTabGroupColor(tabGroupId, getTabGroupColor(tabGroupId));
        setTabGroupCollapsed(tabGroupId, getTabGroupCollapsed(tabGroupId));
    }

    @VisibleForTesting
    boolean detachedTabGroupExists(Token tabGroupId) {
        assertOnUiThread();
        assert mNativeTabCollectionTabModelImplPtr != 0;
        return TabCollectionTabModelImplJni.get()
                .detachedTabGroupExists(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    private void addToClosingTabsCount(int count) {
        if (mClosingTabsCount == null) mClosingTabsCount = 0;
        mClosingTabsCount += count;
    }

    private void decrementClosingTabsCount() {
        assert mClosingTabsCount != null;
        assert mClosingTabsCount > 0;
        mClosingTabsCount--;
    }

    void setPendingTabClosureManagerForTesting(
            @Nullable PendingTabClosureManager pendingTabClosureManager) {
        mPendingTabClosureManager = pendingTabClosureManager;
        ResettersForTesting.register(() -> mPendingTabClosureManager = null);
    }

    @NativeMethods
    interface Natives {
        long init(TabCollectionTabModelImpl javaObject, @JniType("Profile*") Profile profile);

        void destroy(long nativeTabCollectionTabModelImpl);

        int getTabCountRecursive(long nativeTabCollectionTabModelImpl);

        int getIndexOfTabRecursive(
                long nativeTabCollectionTabModelImpl, @JniType("TabAndroid*") Tab tab);

        @JniType("TabAndroid*")
        Tab getTabAtIndexRecursive(long nativeTabCollectionTabModelImpl, int index);

        int moveTabRecursive(
                long nativeTabCollectionTabModelImpl,
                int currentIndex,
                int newIndex,
                @JniType("std::optional<base::Token>") @Nullable Token tabGroupId,
                boolean isPinned);

        int addTabRecursive(
                long nativeTabCollectionTabModelImpl,
                @JniType("TabAndroid*") Tab tab,
                int index,
                @JniType("std::optional<base::Token>") @Nullable Token tabGroupId,
                boolean isAttachingGroup,
                boolean isPinned);

        void removeTabRecursive(
                long nativeTabCollectionTabModelImpl, @JniType("TabAndroid*") Tab tabs);

        void createTabGroup(
                long nativeTabCollectionTabModelImpl,
                @JniType("base::Token") Token tabGroupId,
                @JniType("std::u16string") String title,
                @TabGroupColorId int colorId,
                boolean isCollapsed);

        int moveTabGroupTo(
                long nativeTabCollectionTabModelImpl,
                @JniType("base::Token") Token tabGroupId,
                int newIndex);

        @JniType("std::vector<TabAndroid*>")
        List<Tab> getTabsInGroup(
                long nativeTabCollectionTabModelImpl, @JniType("base::Token") Token tabGroupId);

        int getTabCountForGroup(
                long nativeTabCollectionTabModelImpl, @JniType("base::Token") Token tabGroupId);

        boolean tabGroupExists(
                long nativeTabCollectionTabModelImpl, @JniType("base::Token") Token tabGroupId);

        int getIndividualTabAndGroupCount(long nativeTabCollectionTabModelImpl);

        int getTabGroupCount(long nativeTabCollectionTabModelImpl);

        int getIndexOfTabInGroup(
                long nativeTabCollectionTabModelImpl,
                @JniType("TabAndroid*") Tab tab,
                @JniType("base::Token") Token tabGroupId);

        void updateTabGroupVisualData(
                long nativeTabCollectionTabModelImpl,
                @JniType("base::Token") Token tabGroupId,
                @JniType("std::optional<std::u16string>") @Nullable String title,
                @JniType("std::optional<int>") @Nullable @TabGroupColorId Integer colorId,
                @JniType("std::optional<bool>") @Nullable Boolean isCollapsed);

        @JniType("std::u16string")
        String getTabGroupTitle(
                long nativeTabCollectionTabModelImpl, @JniType("base::Token") Token tabGroupId);

        int getTabGroupColor(
                long nativeTabCollectionTabModelImpl, @JniType("base::Token") Token tabGroupId);

        boolean getTabGroupCollapsed(
                long nativeTabCollectionTabModelImpl, @JniType("base::Token") Token tabGroupId);

        boolean detachedTabGroupExists(
                long nativeTabCollectionTabModelImpl, @JniType("base::Token") Token tabGroupId);

        void closeDetachedTabGroup(
                long nativeTabCollectionTabModelImpl, @JniType("base::Token") Token tabGroupId);

        @JniType("std::vector<TabAndroid*>")
        List<Tab> getAllTabs(long nativeTabCollectionTabModelImpl);

        @JniType("std::vector<base::Token>")
        List<Token> getAllTabGroupIds(long nativeTabCollectionTabModelImpl);

        @JniType("std::vector<TabAndroid*>")
        List<Tab> getRepresentativeTabList(long nativeTabCollectionTabModelImpl);

        void setLastShownTabForGroup(
                long nativeTabCollectionTabModelImpl,
                @JniType("base::Token") Token tabGroupId,
                @JniType("TabAndroid*") @Nullable Tab tab);

        @JniType("TabAndroid*")
        Tab getLastShownTabForGroup(
                long nativeTabCollectionTabModelImpl, @JniType("base::Token") Token tabGroupId);

        int getIndexOfFirstNonPinnedTab(long nativeTabCollectionTabModelImpl);

        @JniType("tabs::TabStripCollection*")
        TabStripCollection getTabStripCollection(long nativeTabCollectionTabModelImpl);
    }
}
