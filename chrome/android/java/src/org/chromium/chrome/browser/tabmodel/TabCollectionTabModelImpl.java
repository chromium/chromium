// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
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
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This class is a work-in-progress drop-in replacement for {@link TabModelImpl} and {@link
 * TabGroupModelFilterImpl}. Rather than being backed with an array of tabs it is backed with a tab
 * collection which represents tabs in logical groupings in an n-ary tree structure.
 */
@NullMarked
@JNINamespace("tabs")
public class TabCollectionTabModelImpl extends TabModelJniBridge
        implements TabGroupModelFilterInternal {
    /** Holds a tab and its index in the tab collection. */
    private static class IndexAndTab {
        public final int index;
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
            TabUngrouper tabUngrouper) {
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

        initializeNative(activityType, isArchivedTabModel);
    }

    @Override
    public void destroy() {
        assertOnUiThread();
        for (Tab tab : mTabIdToTabs.values()) {
            if (mModelDelegate.isReparentingInProgress()
                    && mAsyncTabParamsManager.hasParamsForTabId(tab.getId())) {
                continue;
            }

            // TabStripCollection in native only holds weak ptrs to tabs and will be deleted shortly
            // so this is safe.
            if (tab.isInitialized()) tab.destroy();
        }

        mTabCountSupplier.set(0);
        mTabIdToTabs.clear();
        mTabModelObservers.clear();
        mTabGroupObservers.clear();

        if (mNativeTabCollectionTabModelImplPtr != 0) {
            TabCollectionTabModelImplJni.get().destroy(mNativeTabCollectionTabModelImplPtr);
            mNativeTabCollectionTabModelImplPtr = 0;
        }

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
    public boolean supportsPendingClosures() {
        return false;
    }

    @Override
    public boolean isClosurePending(@TabId int tabId) {
        return false;
    }

    @Override
    public void commitAllTabClosures() {}

    @Override
    public void commitTabClosure(@TabId int tabId) {}

    @Override
    public void cancelTabClosure(@TabId int tabId) {}

    @Override
    public void openMostRecentlyClosedEntry() {
        assertOnUiThread();
        mModelDelegate.openMostRecentlyClosedEntry(this);
        if (!mCurrentTabSupplier.hasValue()) {
            setIndex(0, TabSelectionType.FROM_NEW);
        }
    }

    @Override
    public TabList getComprehensiveModel() {
        return this;
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

        // When we select a tab in this model it should become the active model. This is the
        // existing behavior of TabModelImpl.
        if (!isActiveModel()) mModelDelegate.selectModel(isIncognitoBranded());

        Tab oldSelectedTab = mCurrentTabSupplier.get();
        int lastId = (oldSelectedTab == null) ? Tab.INVALID_TAB_ID : oldSelectedTab.getId();

        int currentTabCount = getCount();
        final Tab newSelectedTab;
        if (currentTabCount == 0) {
            newSelectedTab = null;
        } else {
            newSelectedTab = getTabAt(MathUtils.clamp(i, 0, currentTabCount - 1));
        }
        mCurrentTabSupplier.set(newSelectedTab);
        mModelDelegate.requestToShowTab(newSelectedTab, type);

        if (newSelectedTab != null) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.didSelectTab(newSelectedTab, type, lastId);
                // Required, otherwise the previously active tab will have MULTISELECTED as its
                // VisualState.
                obs.onTabSelectionChanged();
            }

            boolean wasAlreadySelected =
                    (newSelectedTab.getId() == lastId && lastId != Tab.INVALID_TAB_ID);
            if (!wasAlreadySelected && type == TabSelectionType.FROM_USER) {
                RecordUserAction.record("MobileTabSwitched");
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
        if (currentIndex == TabList.INVALID_TAB_INDEX || currentIndex == newIndex) {
            return;
        }

        // Clamp negative values here to ensure the tab moves to index 0 if negative. The size_t
        // cast in C++ otherwise results in the tab going to the end of the list which is not
        // intended.
        newIndex = Math.max(0, newIndex);
        moveTabInternal(tab, currentIndex, newIndex, tab.getTabGroupId(), tab.getIsPinned());
    }

    @Override
    public void pinTab(int tabId) {
        updatePinnedState(tabId, /* isPinned= */ true);
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
        return getTabCreator(isIncognitoBranded());
    }

    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        assertOnUiThread();
        assert !mTabIdToTabs.containsKey(tab.getId())
                : "Attempting to add a duplicate tab id=" + tab.getId();
        if (tab.isOffTheRecord() != isOffTheRecord()) {
            throw new IllegalStateException("Attempting to open a tab in the wrong model.");
        }
        if (mNativeTabCollectionTabModelImplPtr == 0) {
            assert false : "Trying to add a tab to a destroyed TabCollectionTabModelImpl.";
            return;
        }

        // TODO(crbug.com/429145597): Handle auto grouping for certain launch types and on restore.

        for (TabModelObserver obs : mTabModelObservers) obs.willAddTab(tab, type);

        boolean hasAnyTabs = mCurrentTabSupplier.hasValue();
        boolean selectTab =
                mOrderController.willOpenInForeground(type, isIncognitoBranded())
                        || (!hasAnyTabs && type == TabLaunchType.FROM_LONGPRESS_BACKGROUND);
        index = mOrderController.determineInsertionIndex(type, index, tab);

        assert !(tab.getTabGroupId() != null && tab.getIsPinned())
                : "Pinned and grouped states are mutually exclusive.";
        TabCollectionTabModelImplJni.get()
                .addTabRecursive(
                        mNativeTabCollectionTabModelImplPtr,
                        tab,
                        index,
                        tab.getTabGroupId(),
                        tab.getIsPinned());
        int finalIndex = indexOf(tab);

        // When adding the first background tab make sure to select it.
        if (!isActiveModel() && !hasAnyTabs && !selectTab) {
            mCurrentTabSupplier.set(tab);
        }

        tab.onAddedToTabModel(mCurrentTabSupplier, this::isTabMultiSelected);
        mTabIdToTabs.put(tab.getId(), tab);
        mTabCountSupplier.set(getCount());

        tabAddedToModel(tab);
        for (TabModelObserver obs : mTabModelObservers) {
            obs.didAddTab(tab, type, creationState, selectTab);
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
        if (!mCurrentTabSupplier.hasValue()) return 0;
        // If no other tabs are in multi-selection, this returns 1, as the active tab is always
        // considered selected.
        return mMultiSelectedTabs.isEmpty() ? 1 : mMultiSelectedTabs.size();
    }

    // TabCloser overrides.

    @Override
    public boolean closeTabs(TabClosureParams params) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return false;

        final List<Tab> tabsToClose;
        if (params.isAllTabs) {
            tabsToClose = getAllTabs();
        } else {
            tabsToClose = new ArrayList<>(assumeNonNull(params.tabs));
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

        // TODO(crbug.com/381471263): Simplify the observer calls.
        if (params.tabCloseType == TabCloseType.ALL) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.willCloseAllTabs(isIncognitoBranded());
            }
        } else if (params.tabCloseType == TabCloseType.MULTIPLE) {
            // TODO(crbug.com/428977566): Add support for undo.
            for (TabModelObserver obs : mTabModelObservers) {
                obs.willCloseMultipleTabs(false, tabsToClose);
            }
        }

        boolean isSingle = params.tabCloseType == TabCloseType.SINGLE;
        for (Tab tab : tabsToClose) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.willCloseTab(tab, isSingle);
            }
        }

        // TODO(crbug.com/428977566): Add support for undo and only emit this if non-undoable.
        for (TabModelObserver obs : mTabModelObservers) {
            obs.onFinishingMultipleTabClosure(tabsToClose, params.saveToTabRestoreService);
        }

        @TabSelectionType
        int selectionType =
                params.uponExit ? TabSelectionType.FROM_EXIT : TabSelectionType.FROM_CLOSE;
        // Since undo is not supported, pauseMedia is false.
        removeTabsAndSelectNext(
                tabsToClose, params.recommendedNextTab, selectionType, false, params.tabCloseType);

        // TODO(crbug.com/428977566): Add support for undo and only do this if non-undoable.
        for (Tab tab : tabsToClose) {
            finalizeTabClosure(tab, params.tabClosingSource);
        }
        // TODO(crbug.com/429145597): Close any detached tab group if this is not an undoable
        // closure.

        return true;
    }

    // TabModelInternal overrides.

    @Override
    public void completeInitialization() {
        assertOnUiThread();
        assert !mInitializationComplete : "TabCollectionTabModelImpl initialized multiple times.";
        mInitializationComplete = true;

        if (getCount() != 0 && !mCurrentTabSupplier.hasValue()) {
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
                null,
                TabSelectionType.FROM_CLOSE,
                /* pauseMedia= */ false,
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

        // TODO(crbug.com/432297442): See if anything cares about sequencing this after all the tabs
        // are moved.
        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            for (TabModelObserver observer : mTabModelObservers) {
                observer.didMoveTab(tab, finalIndex + i, curIndex + i);
            }
        }

        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            observer.didMoveTabGroup(firstTab, finalIndex, curIndex);
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
        return null;
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
        assertOnUiThread();
        return this;
    }

    @Override
    public List<Tab> getRepresentativeTabList() {
        // TODO(crbug.com/429145597): TabGroupModelFilterImpl uses the last selected tab in a tab
        // group as the representative tab. Ideally, we'd change this to use the first tab in the
        // tab group as the representative tab. However, the tab TabList* code still depends on
        // this being the last selected tab. A refactor of TabList* code is needed to change this.
        return Collections.emptyList();
    }

    @Override
    public int getIndividualTabAndGroupCount() {
        // TODO(crbug.com/428692223): Revisit the performance of this method as compared to checking
        // this in C++ and returning a count.
        return getRepresentativeTabList().size();
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
        return getRepresentativeTabList().indexOf(tab);
    }

    @Override
    public int getTabGroupCount() {
        // TODO(crbug.com/428692223): Revisit the performance of this method by doing it in C++.
        return getAllTabGroupIds().size();
    }

    @Override
    public int getTabCountForGroup(@Nullable Token tabGroupId) {
        // TODO(crbug.com/428692223): Revisit the performance of this method as compared to checking
        // this in C++ and returning a count.
        return getTabsInGroup(tabGroupId).size();
    }

    @Override
    public boolean tabGroupExists(@Nullable Token tabGroupId) {
        // TODO(crbug.com/428692223): Revisit the performance of this method as compared to checking
        // this in C++ and returning a boolean.
        return getTabsInGroup(tabGroupId).size() > 0;
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
        // TODO(crbug.com/428692223): Revisit the performance of this method as compared to
        // computing the index in C++ and returning it.
        return getTabsInGroup(tab.getTabGroupId()).indexOf(tab);
    }

    @Override
    public @TabId int getGroupLastShownTabId(@Nullable Token tabGroupId) {
        return Tab.INVALID_TAB_ID;
    }

    @Override
    public void moveRelatedTabs(@TabId int id, int newIndex) {
        Tab tab = getTabById(id);
        if (tab == null) return;

        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId == null) return;

        moveGroupToIndex(tabGroupId, newIndex);
    }

    @Override
    public boolean willMergingCreateNewGroup(List<Tab> tabsToMerge) {
        assertOnUiThread();
        for (Tab tab : tabsToMerge) {
            if (tab.getTabGroupId() != null) return false;
        }
        return true;
    }

    @Override
    public void createSingleTabGroup(Tab tab) {
        assertOnUiThread();
        assert tab.getTabGroupId() == null;

        // TODO(crbug.com/429145597): Investigate using mergeTabsToGroup instead.
        Token tabGroupId = createDetachedTabGroup();
        int index = indexOf(tab);
        moveTabInternal(tab, index, index, tabGroupId, /* isPinned= */ false);

        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            observer.didCreateNewGroup(tab, this);
        }
    }

    @Override
    public void createTabGroupForTabGroupSync(List<Tab> tabs, Token tabGroupId) {}

    @Override
    public void mergeTabsToGroup(
            @TabId int sourceTabId, @TabId int destinationTabId, boolean skipUpdateTabModel) {}

    @Override
    public void mergeListOfTabsToGroup(List<Tab> tabs, Tab destinationTab, boolean notify) {}

    @Override
    public TabUngrouper getTabUngrouper() {
        return mTabUngrouper;
    }

    @Override
    public void undoGroupedTab(
            Tab tab,
            int originalIndex,
            @TabId int originalRootId,
            @Nullable Token originalTabGroupId) {}

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
        return false;
    }

    @Override
    public LazyOneshotSupplier<Set<Token>> getLazyAllTabGroupIds(
            List<Tab> tabsToExclude, boolean includePendingClosures) {
        return LazyOneshotSupplier.fromValue(Collections.emptySet());
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
    public @Nullable String getTabGroupTitle(@TabId int rootId) {
        return null;
    }

    @Override
    public void setTabGroupTitle(Token tabGroupId, @Nullable String title) {
        assertOnUiThread();
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
    public void setTabGroupTitle(@TabId int rootId, @Nullable String title) {}

    @Override
    public void deleteTabGroupTitle(Token tabGroupId) {
        if (!tabGroupExists(tabGroupId)) return;
        setTabGroupTitle(tabGroupId, "");
    }

    @Override
    public void deleteTabGroupTitle(@TabId int rootId) {}

    @Override
    public int getTabGroupColor(Token tabGroupId) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return TabGroupColorUtils.INVALID_COLOR_ID;
        return TabCollectionTabModelImplJni.get()
                .getTabGroupColor(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    @Override
    public int getTabGroupColor(@TabId int rootId) {
        return TabGroupColorUtils.INVALID_COLOR_ID;
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
    public @TabGroupColorId int getTabGroupColorWithFallback(@TabId int rootId) {
        return TabGroupColorId.GREY;
    }

    @Override
    public void setTabGroupColor(Token tabGroupId, @TabGroupColorId int color) {
        assertOnUiThread();
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
    public void setTabGroupColor(@TabId int rootId, @TabGroupColorId int color) {}

    @Override
    public void deleteTabGroupColor(Token tabGroupId) {
        if (!tabGroupExists(tabGroupId)) return;
        setTabGroupColor(tabGroupId, TabGroupColorId.GREY);
    }

    @Override
    public void deleteTabGroupColor(@TabId int rootId) {}

    @Override
    public boolean getTabGroupCollapsed(Token tabGroupId) {
        assertOnUiThread();
        if (mNativeTabCollectionTabModelImplPtr == 0) return false;
        return TabCollectionTabModelImplJni.get()
                .getTabGroupCollapsed(mNativeTabCollectionTabModelImplPtr, tabGroupId);
    }

    @Override
    public boolean getTabGroupCollapsed(@TabId int rootId) {
        return false;
    }

    @Override
    public void setTabGroupCollapsed(Token tabGroupId, boolean isCollapsed, boolean animate) {
        assertOnUiThread();
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
    public void setTabGroupCollapsed(@TabId int rootId, boolean isCollapsed, boolean animate) {}

    @Override
    public void deleteTabGroupCollapsed(Token tabGroupId) {
        if (!tabGroupExists(tabGroupId)) return;
        setTabGroupCollapsed(tabGroupId, false, false);
    }

    @Override
    public void deleteTabGroupCollapsed(@TabId int rootId) {}

    @Override
    public void deleteTabGroupVisualData(Token tabGroupId) {
        if (!tabGroupExists(tabGroupId)) return;
        deleteTabGroupTitle(tabGroupId);
        deleteTabGroupColor(tabGroupId);
        deleteTabGroupCollapsed(tabGroupId);
    }

    @Override
    public void deleteTabGroupVisualData(@TabId int rootId) {}

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
                /* isPinned= */ false);
    }

    // Internal methods.

    private void finalizeTabClosure(Tab tab, @TabClosingSource int closingSource) {
        mTabContentManager.removeTabThumbnail(tab.getId());

        for (TabModelObserver obs : mTabModelObservers) {
            obs.onFinishingTabClosure(tab, closingSource);
        }

        // Destroy the native tab after the observer notifications have fired, otherwise they risk a
        // use after free or null dereference.
        tab.destroy();
    }

    private void removeTabsAndSelectNext(
            List<Tab> tabsToRemove,
            @Nullable Tab recommendedNextTab,
            @TabSelectionType int selectionType,
            boolean pauseMedia,
            @TabCloseType int closeType) {
        assert selectionType == TabSelectionType.FROM_CLOSE
                || selectionType == TabSelectionType.FROM_EXIT;

        if (tabsToRemove.isEmpty()) return;

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
        Tab nearbyTab = null;
        boolean nextIsIncognito = nextTab == null ? false : nextTab.isIncognitoBranded();
        boolean nextIsInOtherModel = nextIsIncognito != isIncognitoBranded();
        if ((nextTab == null || nextIsInOtherModel) && closeType != TabCloseType.ALL) {
            nearbyTab =
                    TabModelImplUtil.findNearbyNotClosingTab(
                            this, tabsToRemove.indexOf(currentTabInModel), tabsToRemove);
        }

        for (Tab tab : tabsToRemove) {
            assert mTabIdToTabs.containsKey(tab.getId()) : "Tab not found in tab model.";
            if (pauseMedia) TabUtils.pauseMedia(tab);
            // TODO(crbug.com/428692223): Vectorize this.
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

        if (ChromeFeatureList.sTabFreezeOnUndoableClosureKillSwitch.isEnabled() && pauseMedia) {
            for (Tab tab : tabsToRemove) {
                if (!TabUtils.isCapturingForMedia(tab)) continue;
                // If media is being captured freeze the tab to disconnect it.
                tab.freeze();
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

        // The C++ side will adjust to a valid index.
        moveTabInternal(
                tab, currentIndex, currentIndex, /* newTabGroupId= */ null, isPinned);
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
     * @return The final index of the tab.
     */
    private int moveTabInternal(
            Tab tab, int index, int newIndex, @Nullable Token newTabGroupId, boolean isPinned) {
        assert newTabGroupId == null || !isPinned
                : "Pinned and grouped tabs are mutually exclusive.";

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
            for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                observer.willMoveTabOutOfGroup(tab, newTabGroupId);
            }
        }

        if (isMergingIntoGroup) {
            for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
                observer.willMergeTabToGroup(tab, Tab.INVALID_TAB_ID, newTabGroupId);
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

        if (isChangingPinState) {
            for (TabModelObserver obs : mTabModelObservers) {
                obs.didChangePinState(tab);
            }
        }

        if (isMovingOutOfGroup) {
            assumeNonNull(oldTabGroupId);
            boolean wasLastTabInGroup =
                    wasLastTabInGroupAndNotifyDidMoveTabOutOfGroup(tab, oldTabGroupId, finalIndex);
            // TODO(crbug.com/429145597): Also close the detached tab group if this is not an
            // undoable merge.
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
                observer.didMergeTabToGroup(tab);
            }
        }

        return finalIndex;
    }

    private Token createDetachedTabGroup() {
        Token tabGroupId = Token.createRandom();
        @TabGroupColorId int colorId = TabGroupColorUtils.getNextSuggestedColorId(this);

        TabCollectionTabModelImplJni.get()
                .createTabGroup(
                        mNativeTabCollectionTabModelImplPtr,
                        tabGroupId,
                        /* title= */ "",
                        colorId,
                        /* isCollapsed= */ false);
        return tabGroupId;
    }

    /**
     * Notifies observers that a tab has moved out of a group. Returns true if the tab was the last
     * tab in the group.
     */
    private boolean wasLastTabInGroupAndNotifyDidMoveTabOutOfGroup(
            Tab tab, Token oldTabGroupId, int finalIndex) {
        int prevFilterIndex = getFirstTabIndexInGroup(oldTabGroupId);
        boolean isLastTabInGroup = prevFilterIndex == TabList.INVALID_TAB_INDEX;
        if (isLastTabInGroup) {
            prevFilterIndex = finalIndex;
        }
        for (TabGroupModelFilterObserver observer : mTabGroupObservers) {
            observer.didMoveTabOutOfGroup(tab, prevFilterIndex);
        }
        return isLastTabInGroup;
    }

    private int getFirstTabIndexInGroup(Token tabGroupId) {
        // TODO(crbug.com/428692223): Optimize this by requesting it from the collection.
        List<Tab> tabsInGroup = getTabsInGroup(tabGroupId);
        if (tabsInGroup.isEmpty()) return TabList.INVALID_TAB_INDEX;
        return indexOf(tabsInGroup.get(0));
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

        void addTabRecursive(
                long nativeTabCollectionTabModelImpl,
                @JniType("TabAndroid*") Tab tab,
                int index,
                @JniType("std::optional<base::Token>") @Nullable Token tabGroupId,
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

        void closeDetachedTabGroup(
                long nativeTabCollectionTabModelImpl, @JniType("base::Token") Token tabGroupId);

        @JniType("std::vector<TabAndroid*>")
        List<Tab> getAllTabs(long nativeTabCollectionTabModelImpl);

        @JniType("std::vector<base::Token>")
        List<Token> getAllTabGroupIds(long nativeTabCollectionTabModelImpl);
    }
}
