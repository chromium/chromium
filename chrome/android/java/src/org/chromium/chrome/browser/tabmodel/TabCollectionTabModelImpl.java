// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType.DONT_NOTIFY;
import static org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType.NOTIFY_ALWAYS;
import static org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP;
import static org.chromium.chrome.browser.tabmodel.TabGroupUtils.areAnyTabsPartOfSharedGroup;

import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.process_launcher.ScopedServiceBindingBatch;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
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
import org.chromium.components.tab_groups.TabGroupColorId;
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
 * This class is a drop-in replacement for {@link TabModelImpl} and {@link TabGroupModelFilterImpl}.
 * To minimize duplication, many common methods are implemented in {@link TabModelJniBridge}. Until
 * this class if fully launched, parity between this class and {@link TabModelImpl} & {@link
 * TabGroupModelFilterImpl} should be maintained.
 *
 * <p>The class uses the tab collection tree-like structure available in components/tabs/ to
 * organize tabs. The tabs in C++ tab collections are only cached with weak ptr references to the
 * C++ TabAndroid objects for memory safety; however, a strong reference is kept in {@code
 * mTabIdToTabs} to ensure the Java Tab objects are not GC'd. As a result of the weak ptrs, most of
 * this class's public methods can only be used on the UI thread.
 *
 * <p>Ideally, more of the observers and logic should be moved to be in C++ or shared with desktop's
 * tab strip model. However, to achieve drop-in compatibility, most of the observer events and the
 * interfaces were kept as-is for now and can be migrated to C++ later.
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
            assert !tab.isDestroyed() : "Attempting to undo tab that is destroyed.";

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
    private final ObservableSupplierImpl<@Nullable Tab> mCurrentTabSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>(0);
    private final Set<Integer> mMultiSelectedTabs = new HashSet<>();
    private final Set<Token> mHidingTabGroups = new HashSet<>();

    // Efficient lookup of tabs by id rather than index (stored in C++). Also ensures the Java Tab
    // objects are not GC'd as the C++ TabAndroid objects only hold weak references to their Java
    // counterparts.
    private final Map<Integer, Tab> mTabIdToTabs = new HashMap<>();

    private final boolean mIsArchivedTabModel;
    private final TabCreator mRegularTabCreator;
    private final TabCreator mIncognitoTabCreator;
    private final TabModelOrderController mOrderController;
    private final TabContentManager mTabContentManager;
    private final NextTabPolicySupplier mNextTabPolicySupplier;
    private final TabModelDelegate mModelDelegate;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final TabRemover mTabRemover;
    private final TabUngrouper mTabUngrouper;
    private @Nullable PendingTabClosureManager mPendingTabClosureManager;

    private long mNativeTabCollectionTabModelImplPtr;
    // Only ever true for the regular tab model. Called after tab state is initialized, before
    // broadcastSessionRestoreComplete().
    private boolean mInitializationComplete;
    private boolean mActive;

    /**
     * @param profile The {@link Profile} tabs in the tab collection tab model belongs to.
     * @param activityType The type of activity this tab collection tab model is for.
     * @param isArchivedTabModel Whether the tab collection tab model stored archived tabs.
     * @param regularTabCreator The tab creator for regular tabs.
     * @param incognitoTabCreator The tab creator for incognito tabs.
     * @param orderController Controls logic for selecting and positioning tabs.
     * @param tabContentManager Manages tab content.
     * @param nextTabPolicySupplier Supplies the next tab policy.
     * @param modelDelegate The {@link TabModelDelegate} for interacting outside the tab model.
     * @param asyncTabParamsManager To detect if an async tab operation is in progress.
     * @param tabRemover For removing tabs.
     * @param tabUngrouper For ungrouping tabs.
     * @param supportUndo Whether the tab model supports undo functionality.
     */
    public TabCollectionTabModelImpl(
            Profile profile,
            @ActivityType int activityType,
            boolean isArchivedTabModel,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            TabModelDelegate modelDelegate,
            AsyncTabParamsManager asyncTabParamsManager,
            TabRemover tabRemover,
            TabUngrouper tabUngrouper,
            boolean supportUndo) {
        super(profile);
        assertOnUiThread();
        mIsArchivedTabModel = isArchivedTabModel;
        mRegularTabCreator = regularTabCreator;
        mIncognitoTabCreator = incognitoTabCreator;
        mOrderController = orderController;
        mTabContentManager = tabContentManager;
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mModelDelegate = modelDelegate;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mTabRemover = tabRemover;
        mTabUngrouper = tabUngrouper;
        if (supportUndo && !isIncognito()) {
            mPendingTabClosureManager =
                    new PendingTabClosureManager(this, new PendingTabClosureDelegateImpl());
        }

        initializeNative(activityType, isArchivedTabModel);
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

        super.destroy();
    }

    // TabList overrides except those overridden by TabModelJniBridge.

    @Override
    public int index() {
        assertOnUiThread();
        if (mIsArchivedTabModel) return TabList.INVALID_TAB_INDEX;
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
        if (mIsArchivedTabModel) return null;
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

        mPendingTabClosureManager.cancelTabClosure(tabId);
    }

    @Override
    public void openMostRecentlyClosedEntry() {
        if (supportsPendingClosures() && mPendingTabClosureManager.openMostRecentlyClosedEntry()) {
            return;
        }

        mModelDelegate.openMostRecentlyClosedEntry(this);
        Tab oldSelectedTab = mCurrentTabSupplier.get();
        if (oldSelectedTab == null) {
            setIndex(0, TabSelectionType.FROM_NEW);
        }
    }

    @Override
    public TabList getComprehensiveModel() {
        if (!supportsPendingClosures()) return this;
        return mPendingTabClosureManager.getRewoundList();
    }

    @Override
    public ObservableSupplier<@Nullable Tab> getCurrentTabSupplier() {
        return mCurrentTabSupplier;
    }

    @Override
    public void setIndex(int i, final @TabSelectionType int type) {
        assertOnUiThread();

        // TODO(crbug.com/425344200): Prevent passing negative indices.
        if (mIsArchivedTabModel) return;
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
        moveTabInternal(
                tab,
                currentIndex,
                newIndex,
                tab.getTabGroupId(),
                tab.getIsPinned(),
                /* isDestinationTab= */ false);
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
    public ObservableSupplier<Integer> getTabCountSupplier() {
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
        mModelDelegate.moveTabToWindow(tab, activity, newIndex);
    }

    @Override
    public void moveTabGroupToWindow(Token tabGroupId, Activity activity, int newIndex) {
        mModelDelegate.moveTabGroupToWindow(tabGroupId, activity, newIndex, isIncognito());
    }

    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        assertOnUiThread();
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

        for (TabModelObserver obs : mTabModelObservers) obs.willAddTab(tab, type);
        // Clear the multi-selection set before adding the tab.
        clearMultiSelection(/* notifyObservers= */ false);
        boolean hasAnyTabs = mCurrentTabSupplier.get() != null;
        boolean selectTab =
                mOrderController.willOpenInForeground(type, isIncognito())
                        || (!hasAnyTabs && type == TabLaunchType.FROM_LONGPRESS_BACKGROUND);
        index = mOrderController.determineInsertionIndex(type, index, tab);

        // TODO(crbug.com/437141942): Update the list of undoable tabs instead of committing it.
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
        // When migrating to tab collections we cease the use of root id. After reading any
        // necessary data to restore a tab group's metadata we no longer need the root id and can
        // reset it to the tab's id. If tab collections is turned off TabGroupModelFilterImpl has a
        // back-migration pathway that rebuilds the correct root id structure from tab group id.
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
        if (!isActiveModel() && !hasAnyTabs && !selectTab) {
            mCurrentTabSupplier.set(tab);
        }

        tab.onAddedToTabModel(mCurrentTabSupplier, this::isTabMultiSelected);
        mTabIdToTabs.put(tab.getId(), tab);
        mTabCountSupplier.set(getCount());

        if (tabGroupId != null && getTabsInGroup(tabGroupId).size() == 1) {
            setLastShownTabForGroup(tabGroupId, tab);
        }

        if (supportsPendingClosures()) {
            mPendingTabClosureManager.resetState();
        }

        tabAddedToModel(tab);
        for (TabModelObserver obs : mTabModelObservers) {
            obs.didAddTab(tab, type, creationState, selectTab);
        }
        if (groupWithParent) {
            // TODO(crbug.com/434015906): The sequencing here is incorrect as the tab is already
            // grouped at this point; however, current clients don't care and we may be able to
            // remove `willMergeTabToGroup` from the observer interface entirely one tab collections
            // is fully launched.

            // Wait until after didAddTab before notifying observers so the tabs are present in the
            // collection.
            for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                observer.willMergeTabToGroup(tab, Tab.INVALID_TAB_ID, tabGroupId);
                observer.didMergeTabToGroup(tab, /* isDestinationTab= */ false);
            }
        }

        if (selectTab) setIndex(finalIndex, TabSelectionType.FROM_NEW);
    }

    @Override
    public void setTabsMultiSelected(Set<Integer> tabIds, boolean isSelected) {
        assertOnUiThread();
        TabModelImplUtil.setTabsMultiSelected(
                tabIds, isSelected, mMultiSelectedTabs, mTabModelObservers);
        assert mMultiSelectedTabs.isEmpty()
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

        boolean didCloseAlone = params.tabCloseType == TabCloseType.SINGLE;
        for (Tab tab : tabsToClose) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.willCloseTab(tab, didCloseAlone);
            }
        }

        Set<Integer> tabsToCloseIds = new HashSet<>();
        for (Tab tab : tabsToClose) {
            tabsToCloseIds.add(tab.getId());
        }
        setTabsMultiSelected(tabsToCloseIds, /* isSelected= */ false);

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
                params.tabCloseType);

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

        return true;
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
        removeTabsAndSelectNext(
                Collections.singletonList(tab),
                /* recommendedNextTab= */ null,
                TabSelectionType.FROM_CLOSE,
                /* isUndoable= */ false,
                TabCloseType.SINGLE);

        for (TabModelObserver obs : mTabModelObservers) obs.tabRemoved(tab);
    }

    @Override
    public void setActive(boolean active) {
        mActive = active;
    }

    // TabModelJniBridge overrides.

    @Override
    public void initializeNative(@ActivityType int activityType, boolean isArchivedTabModel) {
        super.initializeNative(activityType, isArchivedTabModel);
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
    }

    @Override
    protected List<Tab> getAllTabs() {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return Collections.emptyList();
        return TabCollectionTabModelImplJni.get().getAllTabs(mNativeTabCollectionTabModelImplPtr);
    }

    @Override
    protected @Nullable Token addTabsToGroup(@Nullable Token tabGroupId, List<Tab> tabs) {
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

    @Override
    public boolean willMergingCreateNewGroup(List<Tab> tabsToMerge) {
        assertOnUiThread();
        return getCandidateTabGroupIdsForMerge(tabsToMerge).isEmpty();
    }

    @Override
    public void createSingleTabGroup(Tab tab) {
        assertOnUiThread();
        assert tab.getTabGroupId() == null;

        mergeListOfTabsToGroup(
                Collections.singletonList(tab), tab, /* notify= */ NOTIFY_IF_NOT_NEW_GROUP);
    }

    @Override
    public void createTabGroupForTabGroupSync(List<Tab> tabs, Token tabGroupId) {
        if (tabs.isEmpty()) return;

        mergeListOfTabsToGroupInternal(
                tabs, tabs.get(0), /* notify= */ DONT_NOTIFY, /* indexInGroup= */ null, tabGroupId);
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
        mergeListOfTabsToGroup(
                tabsToMerge,
                destinationTab,
                skipUpdateTabModel ? DONT_NOTIFY : NOTIFY_IF_NOT_NEW_GROUP);
    }

    @Override
    public void mergeListOfTabsToGroup(
            List<Tab> tabs,
            Tab destinationTab,
            @Nullable Integer indexInGroup,
            @MergeNotificationType int notify) {
        mergeListOfTabsToGroupInternal(
                tabs,
                destinationTab,
                notify,
                /* indexInGroup= */ indexInGroup,
                /* tabGroupIdForNewGroup= */ null);
    }

    @Override
    public TabUngrouper getTabUngrouper() {
        return mTabUngrouper;
    }

    @Override
    public void performUndoGroupOperation(UndoGroupMetadata undoGroupMetadata) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return;

        UndoGroupMetadataImpl undoGroupMetadataImpl = (UndoGroupMetadataImpl) undoGroupMetadata;
        Token tabGroupId = undoGroupMetadataImpl.getTabGroupId();

        // Move each of the merged tabs back to their original state in reverse order. If the
        // destination tab was moved it will be moved last.
        List<UndoGroupTabData> mergedTabs = undoGroupMetadataImpl.mergedTabsData;
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
        if (undoGroupMetadataImpl.adoptedTabGroupOriginalIndex != INVALID_TAB_INDEX) {
            moveGroupToIndex(tabGroupId, undoGroupMetadataImpl.adoptedTabGroupOriginalIndex);
        }

        // Reset or delete the state of the undone group.
        if (undoGroupMetadataImpl.adoptedTabGroupTitle) {
            deleteTabGroupTitle(tabGroupId);
        }
        if (undoGroupMetadataImpl.didCreateNewGroup) {
            closeDetachedTabGroup(tabGroupId);
        } else if (undoGroupMetadataImpl.wasDestinationTabGroupCollapsed) {
            setTabGroupCollapsed(tabGroupId, true);
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
    public @Nullable String getTabGroupTitle(Token tabGroupId) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return null;
        return TabCollectionTabModelImplJni.get()
                .getTabGroupTitle(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    @Override
    public @Nullable String getTabGroupTitle(Tab groupedTab) {
        Token tabGroupId = groupedTab.getTabGroupId();
        assert tabGroupId != null;
        return getTabGroupTitle(tabGroupId);
    }

    @Override
    public void setTabGroupTitle(Token tabGroupId, @Nullable String title) {
        assertOnUiThread();
        TabGroupVisualDataStore.storeTabGroupTitle(tabGroupId, title);
        if (mNativeTabCollectionTabModelImplPtr == 0) return;
        TabCollectionTabModelImplJni.get()
                .updateTabGroupVisualData(
                        mNativeTabCollectionTabModelImplPtr,
                        tabGroupId,
                        title,
                        /* colorId= */ null,
                        /* isCollapsed= */ null);
        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            observer.didChangeTabGroupTitle(tabGroupId, title);
        }
    }

    @Override
    public void deleteTabGroupTitle(Token tabGroupId) {
        if (!tabGroupExists(tabGroupId)) return;
        TabGroupVisualDataStore.deleteTabGroupTitle(tabGroupId);
        setTabGroupTitle(tabGroupId, "");
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
        TabGroupVisualDataStore.storeTabGroupColor(tabGroupId, color);
        if (mNativeTabCollectionTabModelImplPtr == 0) return;
        TabCollectionTabModelImplJni.get()
                .updateTabGroupVisualData(
                        mNativeTabCollectionTabModelImplPtr,
                        tabGroupId,
                        /* title= */ null,
                        color,
                        /* isCollapsed= */ null);
        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            observer.didChangeTabGroupColor(tabGroupId, color);
        }
    }

    @Override
    public void deleteTabGroupColor(Token tabGroupId) {
        if (!tabGroupExists(tabGroupId)) return;
        TabGroupVisualDataStore.deleteTabGroupColor(tabGroupId);
        setTabGroupColor(tabGroupId, TabGroupColorId.GREY);
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
        TabGroupVisualDataStore.storeTabGroupCollapsed(tabGroupId, isCollapsed);
        if (mNativeTabCollectionTabModelImplPtr == 0) return;
        TabCollectionTabModelImplJni.get()
                .updateTabGroupVisualData(
                        mNativeTabCollectionTabModelImplPtr,
                        tabGroupId,
                        /* title= */ null,
                        /* colorId= */ null,
                        isCollapsed);
        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            observer.didChangeTabGroupCollapsed(tabGroupId, isCollapsed, animate);
        }
    }

    @Override
    public void deleteTabGroupCollapsed(Token tabGroupId) {
        if (!tabGroupExists(tabGroupId)) return;
        TabGroupVisualDataStore.deleteTabGroupCollapsed(tabGroupId);
        setTabGroupCollapsed(tabGroupId, false, false);
    }

    // TabGroupModelFilterInternal overrides.

    @Override
    public void markTabStateInitialized() {
        // Intentional no-op. This is handled by mModelDelegate#isTabModelRestored().
    }

    @Override
    public void moveTabOutOfGroupInDirection(int sourceTabId, boolean trailing) {
        assertOnUiThread();
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
            @TabCloseType int closeType) {
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
            tab.onRemovedFromTabModel(mCurrentTabSupplier);
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
                // If media is being captured freeze the tab to disconnect it.
                tab.freeze();
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
    public void mergeListOfTabsToGroupInternal(
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
        String storedTitle = TabGroupVisualDataStore.getTabGroupTitle(tabGroupId);
        String title = (storedTitle != null) ? storedTitle : "";

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
