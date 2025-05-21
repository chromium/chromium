// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ObserverList;
import org.chromium.base.Token;
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
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This class is a work-in progress drop-in replacement for {@link TabModelImpl} and {@link
 * TabGroupModelFilterImpl}. Rather than being backed with an array of tabs it is backed with a tab
 * collection which represents tabs in logical groupings in an n-ary tree structure.
 */
@NullMarked
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
    // objects are not
    // GC'd as the C++ TabAndroid objects only hold weak references to their Java counterparts.
    private final Map<Integer, Tab> mTabIdToTabs = new HashMap<>();

    private final TabCreator mRegularTabCreator;
    private final TabCreator mIncognitoTabCreator;
    // TODO(crbug.com/405343634): Replace this with the appropriate TabUngrouper.
    private final TabUngrouper mTabUngrouper = new PassthroughTabUngrouper(() -> this);

    private boolean mInitializationComplete;
    private boolean mActive;

    /**
     * @param profile The {@link Profile} tabs in the tab collection tab model belongs to.
     * @param activityType The type of activity this tab collection tab model is for.
     * @param isArchivedTabModel Whether the tab collection tab model stored archived tabs.
     * @param regularTabCreator The tab creator for regular tabs.
     * @param incognitoTabCreator The tab creator for incognito tabs.
     */
    public TabCollectionTabModelImpl(
            Profile profile,
            @ActivityType int activityType,
            boolean isArchivedTabModel,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator) {
        super(profile, activityType, isArchivedTabModel);
        mRegularTabCreator = regularTabCreator;
        mIncognitoTabCreator = incognitoTabCreator;

        initializeNative(profile);
    }

    @Override
    public void destroy() {
        // TODO(crbug.com/405343634): Destroy any still open tabs.
        mTabCountSupplier.set(0);
        mTabIdToTabs.clear();
        mTabModelObservers.clear();
        mTabGroupObservers.clear();
        super.destroy();
    }

    // TabList overrides except those overridden by TabModelJniBridge.

    @Override
    public int index() {
        return TabList.INVALID_TAB_INDEX;
    }

    @Override
    public int getCount() {
        return 0;
    }

    @Override
    public @Nullable Tab getTabAt(int index) {
        return null;
    }

    @Override
    public int indexOf(@Nullable Tab tab) {
        return TabList.INVALID_TAB_INDEX;
    }

    // SupportsTabModelObserver overrides.

    @Override
    public void addObserver(TabModelObserver observer) {
        mTabModelObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
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
    public void setIndex(int i, final @TabSelectionType int type) {}

    @Override
    public boolean isActiveModel() {
        return mActive;
    }

    @Override
    public boolean isInitializationComplete() {
        return mInitializationComplete;
    }

    @Override
    public void moveTab(int id, int newIndex) {}

    @Override
    public ObservableSupplier<Integer> getTabCountSupplier() {
        return mTabCountSupplier;
    }

    @Override
    public TabCreator getTabCreator() {
        return getTabCreator(isIncognitoBranded());
    }

    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {

        assert !mTabIdToTabs.containsKey(tab.getId())
                : "Attempting to add a duplicate tab id=" + tab.getId();
        if (tab.isOffTheRecord() != isOffTheRecord()) {
            throw new IllegalStateException("Attempting to open a tab in the wrong model.");
        }

        // TODO(crbug.com/405343634): Add the tab to the collection and select the tab if required.

        mTabIdToTabs.put(tab.getId(), tab);
    }

    // TabCloser overrides.

    @Override
    public boolean closeTabs(TabClosureParams tabClosureParams) {
        return false;
    }

    // TabModelInternal overrides.

    @Override
    public void completeInitialization() {
        assert !mInitializationComplete : "TabCollectionTabModelImpl initialized multiple times.";
        mInitializationComplete = true;

        // TODO(crbug.com/405343634): set activated and current index if applicable.

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
        return false;
    }

    @Override
    protected void openTabProgrammatically(GURL url, int index) {}

    // TabGroupModelFilter overrides.

    @Override
    public void addTabGroupObserver(TabGroupModelFilterObserver observer) {
        mTabGroupObservers.addObserver(observer);
    }

    @Override
    public void removeTabGroupObserver(TabGroupModelFilterObserver observer) {
        mTabGroupObservers.removeObserver(observer);
    }

    @Override
    public TabModel getTabModel() {
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
        return false;
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
    public void markTabStateInitialized() {}

    @Override
    public void moveTabOutOfGroupInDirection(int sourceTabId, boolean trailing) {}

    // Internal methods.

    private TabCreator getTabCreator(boolean incognito) {
        return incognito ? mIncognitoTabCreator : mRegularTabCreator;
    }
}
