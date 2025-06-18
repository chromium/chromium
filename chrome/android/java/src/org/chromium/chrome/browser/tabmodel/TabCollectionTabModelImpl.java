// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.JNINamespace;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This class is a work-in progress drop-in replacement for {@link TabModelImpl} and {@link
 * TabGroupModelFilterImpl}. Rather than being backed with an array of tabs it is backed with a tab
 * collection which represents tabs in logical groupings in an n-ary tree structure.
 */
@NullMarked
@JNINamespace("tabs")
public class TabCollectionTabModelImpl extends TabModelJniBridge
        implements TabGroupModelFilterInternal {
    private final ObserverList<TabModelObserver> mTabModelObservers = new ObserverList<>();
    private final ObserverList<TabGroupModelFilterObserver> mTabGroupObservers =
            new ObserverList<>();
    private final ObservableSupplierImpl<@Nullable Tab> mCurrentTabSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>(0);

    // Efficient lookup of tabs by id rather than index (stored in C++). Also ensures the Java Tab
    // objects are not GC'd as the C++ TabAndroid objects only hold weak references to their Java
    // counterparts.
    private final Map<Integer, Tab> mTabIdToTabs = new HashMap<>();

    private final boolean mIsArchivedTabModel;
    private final TabCreator mRegularTabCreator;
    private final TabCreator mIncognitoTabCreator;
    private final TabModelOrderController mOrderController;
    private final TabModelDelegate mModelDelegate;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    // TODO(crbug.com/405343634): Replace this with the appropriate TabUngrouper.
    private final TabUngrouper mTabUngrouper = new PassthroughTabUngrouper(() -> this);

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
     * @param modelDelegate The {@link TabModelDelegate} for interacting outside the tab model.
     * @param asyncTabParamsManager To detect if an async tab operation is in progress.
     */
    public TabCollectionTabModelImpl(
            Profile profile,
            @ActivityType int activityType,
            boolean isArchivedTabModel,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabModelDelegate modelDelegate,
            AsyncTabParamsManager asyncTabParamsManager) {
        super(profile);
        assertOnUiThread();
        mIsArchivedTabModel = isArchivedTabModel;
        mRegularTabCreator = regularTabCreator;
        mIncognitoTabCreator = incognitoTabCreator;
        mOrderController = orderController;
        mModelDelegate = modelDelegate;
        mAsyncTabParamsManager = asyncTabParamsManager;

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
        return assumeNonNull(null);
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
    public @Nullable Tab getTabById(int tabId) {
        return mTabIdToTabs.get(tabId);
    }

    @Override
    public TabRemover getTabRemover() {
        return new EmptyTabRemover();
    }

    @Override
    public @Nullable Tab getNextTabIfClosed(int id, boolean uponExit) {
        return null;
    }

    @Override
    public boolean supportsPendingClosures() {
        return false;
    }

    @Override
    public boolean isClosurePending(int tabId) {
        return false;
    }

    @Override
    public void commitAllTabClosures() {}

    @Override
    public void commitTabClosure(int tabId) {}

    @Override
    public void cancelTabClosure(int tabId) {}

    @Override
    public void openMostRecentlyClosedEntry() {}

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
    public void moveTab(int id, int newIndex) {}

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

        tab.onAddedToTabModel(mCurrentTabSupplier);
        mTabIdToTabs.put(tab.getId(), tab);
        mTabCountSupplier.set(getCount());

        tabAddedToModel(tab);
        for (TabModelObserver obs : mTabModelObservers) {
            obs.didAddTab(tab, type, creationState, selectTab);
        }

        if (selectTab) setIndex(finalIndex, TabSelectionType.FROM_NEW);
    }

    // TabCloser overrides.

    @Override
    public boolean closeTabs(TabClosureParams tabClosureParams) {
        return false;
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
    public void removeTab(Tab tab) {}

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
    public void forceCloseAllTabs() {}

    @Override
    public boolean closeTabAt(int index) {
        return false;
    }

    @Override
    protected boolean createTabWithWebContents(
            Tab parent, Profile profile, WebContents webContents, boolean select) {
        return false;
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
            boolean isRendererInitiated) {}

    @Override
    protected @Nullable Tab createNewTabForDevTools(GURL url, boolean newWindow) {
        // TODO(crbug.com/405343634): This should be non-null once implemented.
        return null;
    }

    @Override
    protected int getTabCountNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {
        return 0;
    }

    @Override
    protected void closeTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {}

    @Override
    protected boolean isSessionRestoreInProgress() {
        assertOnUiThread();
        return !mModelDelegate.isTabModelRestored();
    }

    @Override
    protected void openTabProgrammatically(GURL url, int index) {}

    @Override
    protected void moveTabToIndex(int index, int newIndex) {}

    @Override
    protected Tab[] getAllTabs() {
        return new Tab[0];
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
        return Collections.emptyList();
    }

    @Override
    public int getIndividualTabAndGroupCount() {
        return 0;
    }

    @Override
    public int getCurrentRepresentativeTabIndex() {
        return TabList.INVALID_TAB_INDEX;
    }

    @Override
    public @Nullable Tab getCurrentRepresentativeTab() {
        return null;
    }

    @Override
    public @Nullable Tab getRepresentativeTabAt(int index) {
        return null;
    }

    @Override
    public int representativeIndexOf(@Nullable Tab tab) {
        return TabList.INVALID_TAB_INDEX;
    }

    @Override
    public int getTabGroupCount() {
        return 0;
    }

    @Override
    public int getTabCountForGroup(@Nullable Token tabGroupId) {
        return 0;
    }

    @Override
    public boolean tabGroupExists(@Nullable Token tabGroupId) {
        return false;
    }

    @Override
    public @TabId int getRootIdFromTabGroupId(@Nullable Token tabGroupId) {
        return Tab.INVALID_TAB_ID;
    }

    @Override
    public @Nullable Token getTabGroupIdFromRootId(@TabId int rootId) {
        return null;
    }

    @Override
    public List<Tab> getRelatedTabList(@TabId int tabId) {
        return Collections.emptyList();
    }

    @Override
    public List<Tab> getTabsInGroup(@Nullable Token tabGroupId) {
        return Collections.emptyList();
    }

    @Override
    public boolean isTabInTabGroup(Tab tab) {
        return false;
    }

    @Override
    public int getIndexOfTabInGroup(Tab tab) {
        return TabList.INVALID_TAB_INDEX;
    }

    @Override
    public @TabId int getGroupLastShownTabId(@Nullable Token tabGroupId) {
        return Tab.INVALID_TAB_ID;
    }

    @Override
    public void moveRelatedTabs(@TabId int id, int newIndex) {}

    @Override
    public boolean willMergingCreateNewGroup(List<Tab> tabsToMerge) {
        return false;
    }

    @Override
    public void createSingleTabGroup(Tab tab) {}

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
        return Collections.emptySet();
    }

    @Override
    public int getValidPosition(Tab tab, int proposedPosition) {
        return TabList.INVALID_TAB_INDEX;
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
    public @Nullable String getTabGroupTitle(@TabId int rootId) {
        return null;
    }

    @Override
    public void setTabGroupTitle(@TabId int rootId, @Nullable String title) {}

    @Override
    public void deleteTabGroupTitle(@TabId int rootId) {}

    @Override
    public int getTabGroupColor(@TabId int rootId) {
        return TabGroupColorUtils.INVALID_COLOR_ID;
    }

    @Override
    public @TabGroupColorId int getTabGroupColorWithFallback(@TabId int rootId) {
        return TabGroupColorId.GREY;
    }

    @Override
    public void setTabGroupColor(@TabId int rootId, @TabGroupColorId int color) {}

    @Override
    public void deleteTabGroupColor(@TabId int rootId) {}

    @Override
    public boolean getTabGroupCollapsed(@TabId int rootId) {
        return false;
    }

    @Override
    public void setTabGroupCollapsed(@TabId int rootId, boolean isCollapsed, boolean animate) {}

    @Override
    public void deleteTabGroupCollapsed(@TabId int rootId) {}

    @Override
    public void deleteTabGroupVisualData(@TabId int rootId) {}

    // TabGroupModelFilterInternal overrides.

    @Override
    public void markTabStateInitialized() {
        // Intentional no-op. This is handled by mModelDelegate#isTabModelRestored().
    }

    @Override
    public void moveTabOutOfGroupInDirection(int sourceTabId, boolean trailing) {}

    // Internal methods.

    private TabCreator getTabCreator(boolean incognito) {
        return incognito ? mIncognitoTabCreator : mRegularTabCreator;
    }

    @NativeMethods
    interface Natives {
        long init(TabCollectionTabModelImpl javaObject, Profile profile);

        void destroy(long nativeTabCollectionTabModelImpl);

        int getTabCountRecursive(long nativeTabCollectionTabModelImpl);

        int getIndexOfTabRecursive(long nativeTabCollectionTabModelImpl, Tab tab);

        Tab getTabAtIndexRecursive(long nativeTabCollectionTabModelImpl, int index);

        void addTabRecursive(
                long nativeTabCollectionTabModelImpl,
                Tab tab,
                int index,
                @Nullable Token tabGroupId,
                boolean isPinned);
    }
}
