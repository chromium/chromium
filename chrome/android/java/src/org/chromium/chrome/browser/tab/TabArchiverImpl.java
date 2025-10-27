// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tab.Tab.INVALID_TIMESTAMP;
import static org.chromium.chrome.browser.tabmodel.TabList.INVALID_TAB_INDEX;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.ThemeType;
import org.chromium.chrome.browser.tab.TabArchiver.Observer;
import org.chromium.chrome.browser.tab.state.ArchivePersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Responsible for moving tabs to/from the archived {@link TabModel}. */
@NullMarked
public class TabArchiverImpl implements TabArchiver {
    // The {@link ThemeType} checked at the end of each declutter pass.
    private static @ThemeType int sUiThemeSetting = NightModeUtils.getThemeSetting();

    /** Provides the current timestamp. */
    // TODO(crbug.com/389152957): Collect Clock implementations in base for code reuse.
    @FunctionalInterface
    public interface Clock {
        long currentTimeMillis();
    }

    private final CallbackController mCallbackController = new CallbackController();
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final TabGroupModelFilter mArchivedTabGroupModelFilter;
    private final TabCreator mArchivedTabCreator;
    private final TabArchiveSettings mTabArchiveSettings;
    private final TabGroupSyncService mTabGroupSyncService;

    private Clock mClock;

    /**
     * @param archivedTabGroupModelFilter The archived {@link TabGroupModelFilter}.
     * @param archivedTabCreator The {@link TabCreator} for the archived TabModel.
     * @param tabArchiveSettings The settings for tab archiving/deletion.
     * @param clock A clock object to get the current time.
     * @param tabGroupSyncService The {@link TabGroupSyncService}.
     */
    public TabArchiverImpl(
            TabGroupModelFilter archivedTabGroupModelFilter,
            TabCreator archivedTabCreator,
            TabArchiveSettings tabArchiveSettings,
            Clock clock,
            TabGroupSyncService tabGroupSyncService) {
        mArchivedTabGroupModelFilter = archivedTabGroupModelFilter;
        mArchivedTabCreator = archivedTabCreator;
        mTabArchiveSettings = tabArchiveSettings;
        mClock = clock;
        mTabGroupSyncService = tabGroupSyncService;
    }

    @Override
    public void destroy() {
        mCallbackController.destroy();
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void doArchivePass(TabModelSelector selectorToArchive) {
        ThreadUtils.assertOnUiThread();
        assert mTabArchiveSettings.getArchiveEnabled();
        long startTimeMs = mClock.currentTimeMillis();

        // Wait for the declutter pass to complete, then do follow-up tasks.
        addObserver(
                new Observer() {
                    @Override
                    public void onDeclutterPassCompleted() {
                        removeObserver(this);
                        ensureArchivedTabsHaveCorrectFields();
                    }
                });

        TabGroupModelFilter regularTabGroupModelFilter =
                selectorToArchive
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(/* isIncognito= */ false);
        assert regularTabGroupModelFilter != null;
        TabModel model = regularTabGroupModelFilter.getTabModel();

        // Skip archiving if the declutter pass arises from a UI theme change or user is inactive.
        if (!isUserActive(model) || wasUiThemeChanged()) {
            broadcastDeclutterComplete();
            return;
        }

        // Get the tabs to archive, which moves them to the archived TabModel.
        List<Tab> tabsToArchive = getTabsToArchive(regularTabGroupModelFilter);
        // Get the tabs which exist in both the regular & archived TabModel.
        List<Tab> tabsToClose = getTabsWithExistingArchivedTabs(regularTabGroupModelFilter);

        if (tabsToArchive.size() > 0) {
            archiveAndRemoveTabs(regularTabGroupModelFilter, tabsToArchive);
        }

        if (tabsToClose.size() > 0) {
            model.getTabRemover()
                    .closeTabs(
                            TabClosureParams.closeTabs(tabsToClose).allowUndo(false).build(),
                            /* allowDialog= */ false);
        }

        RecordHistogram.recordCount1000Histogram(
                "Tabs.TabArchived.FoundDuplicateInRegularModel", tabsToClose.size());
        RecordHistogram.recordTimesHistogram(
                "Tabs.ArchivePass.DurationMs", mClock.currentTimeMillis() - startTimeMs);

        broadcastDeclutterComplete();
    }

    @VisibleForTesting
    List<Tab> getTabsToArchive(TabGroupModelFilter regularTabGroupModelFilter) {
        List<Tab> tabsToArchive = new ArrayList<>();
        TabModel model = regularTabGroupModelFilter.getTabModel();
        int activeTabId = TabModelUtils.getCurrentTabId(model);
        if (activeTabId == Tab.INVALID_TAB_ID) return tabsToArchive;

        Tab activeTab = model.getTabByIdChecked(activeTabId);
        // Maps unique URLs to their MRU timestamp, used to declutter duplicate tabs.
        Map<GURL, Long> tabUrlToLastActiveTimestampMap = createUrlToMruTimestampMap(model);
        // Maps unique tab group tokens to the eligibility of that group.
        Map<Token, Boolean> tabGroupIdToArchiveEligibilityMap = new HashMap<>();

        int maxSimultaneousArchives = mTabArchiveSettings.getMaxSimultaneousArchives();
        for (Tab tab : model) {
            // TODO(crbug.com/369845089): Investigate a more graceful fix to
            // batch these so all relevant tabs still get archived in the same
            // session.
            if (tabsToArchive.size() >= maxSimultaneousArchives) {
                RecordHistogram.recordCount100000Histogram(
                        "Tabs.ArchivedTabs.MaxLimitReachedAt", maxSimultaneousArchives);
                break;
            }

            // The active tab is never archived, including if the active tab is actually a group.
            if (activeTab.getId() == tab.getId()
                    || (activeTab.getTabGroupId() != null
                            && activeTab.getTabGroupId().equals(tab.getTabGroupId()))) {
                continue;
            }

            // Pinned tabs are never archived.
            if (tab.getIsPinned()) {
                continue;
            }

            // Handle regular tabs and tab groups separately.
            if (tab.getTabGroupId() == null
                    && isTabEligibleForArchive(tabUrlToLastActiveTimestampMap, tab)) {
                tabsToArchive.add(tab);
            } else if (tab.getTabGroupId() != null
                    && isGroupTabEligibleForArchive(
                            regularTabGroupModelFilter,
                            tabGroupIdToArchiveEligibilityMap,
                            tabUrlToLastActiveTimestampMap,
                            tab)) {
                tabsToArchive.add(tab);
            }
        }

        return tabsToArchive;
    }

    private List<Tab> getTabsWithExistingArchivedTabs(
            TabGroupModelFilter regularTabGroupModelFilter) {
        TabModel model = regularTabGroupModelFilter.getTabModel();
        List<Tab> tabsToClose = new ArrayList<>();

        for (Tab tab : model) {
            Tab archivedTab = mArchivedTabGroupModelFilter.getTabModel().getTabById(tab.getId());
            if (archivedTab != null) {
                tabsToClose.add(tab);
            }
        }

        return tabsToClose;
    }

    @Override
    public void doAutodeletePass() {
        ThreadUtils.assertOnUiThread();
        if (!mTabArchiveSettings.isAutoDeleteEnabled()) return;
        long startTimeMs = mClock.currentTimeMillis();

        List<Tab> tabs = new ArrayList<>();
        List<SavedTabGroup> tabGroups = new ArrayList<>();

        for (Tab tab : mArchivedTabGroupModelFilter.getTabModel()) {
            tabs.add(tab);
        }

        final boolean tabGroupDeclutterEnabled =
                ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled();
        final int autodeleteTaskCount = tabGroupDeclutterEnabled ? 2 : 1;
        final AtomicInteger autodeleteTasksRemaining = new AtomicInteger(autodeleteTaskCount);

        deleteArchivedTabsIfEligibleAsync(tabs, startTimeMs, autodeleteTasksRemaining);

        if (tabGroupDeclutterEnabled) {
            for (String syncGroupId : mTabGroupSyncService.getAllGroupIds()) {
                SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(syncGroupId);
                if (savedTabGroup != null && savedTabGroup.archivalTimeMs != null) {
                    tabGroups.add(savedTabGroup);
                }
            }
            deleteArchivedTabGroupsIfEligibleAsync(
                    tabGroups, startTimeMs, autodeleteTasksRemaining);
        }
    }

    @Override
    public void archiveAndRemoveTabs(
            TabGroupModelFilter regularTabGroupModelFilter, List<Tab> tabs) {
        ThreadUtils.assertOnUiThread();

        TabModel tabModel = regularTabGroupModelFilter.getTabModel();
        List<Tab> singleTabsToClose = new ArrayList<>();
        List<Tab> archivedTabs = new ArrayList<>();
        Set<Token> archivedTabGroupIds = new HashSet<>();
        // Add tabs to the archived tab model first to prevent tab loss if the operation is aborted.
        for (Tab tab : tabs) {
            // Do not add tabs that are part of tab groups to the archived tab model.
            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()
                    && tabGroupId != null) {
                archivedTabGroupIds.add(tabGroupId);
                continue;
            }

            TabState tabState = prepareTabState(tab);
            Tab archivedTab =
                    mArchivedTabCreator.createFrozenTab(tabState, tab.getId(), INVALID_TAB_INDEX);
            archivedTabs.add(archivedTab);
            singleTabsToClose.add(tab);
        }

        if (ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()
                && mTabGroupSyncService != null) {
            int archivedTabGroups = 0;
            for (Token tabGroupId : archivedTabGroupIds) {
                LocalTabGroupId localTabGroupId = new LocalTabGroupId(tabGroupId);
                SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(localTabGroupId);
                if (savedTabGroup != null && savedTabGroup.syncId != null) {
                    mTabGroupSyncService.updateArchivalStatus(
                            savedTabGroup.syncId, /* archivalStatus= */ true);
                    archivedTabGroups++;
                    RecordHistogram.recordCount1000Histogram(
                            "TabGroups.TabGroupDeclutter.ArchivedTabGroupTabCount",
                            savedTabGroup.savedTabs.size());
                }
            }
            RecordHistogram.recordCount1000Histogram(
                    "TabGroups.TabGroupDeclutter.ArchivedTabGroups", archivedTabGroups);
        }

        int tabCount = tabs.size();
        // Once the archived tabs are added, do a bulk closure from the regular tab model.
        tabModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(singleTabsToClose).allowUndo(false).build(),
                        /* allowDialog= */ false);
        if (ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()) {
            for (Token tabGroupId : archivedTabGroupIds) {
                tabModel.getTabRemover()
                        .closeTabs(
                                assumeNonNull(
                                                TabClosureParams.forCloseTabGroup(
                                                        regularTabGroupModelFilter, tabGroupId))
                                        .hideTabGroups(true)
                                        .allowUndo(false)
                                        .build(),
                                /* allowDialog= */ false);
            }
        }

        RecordHistogram.recordCount1000Histogram("Tabs.TabArchived.TabCount", tabCount);
        initializePersistedTabDataAsync(archivedTabs);
    }

    @Override
    public void unarchiveAndRestoreTabs(
            TabCreator tabCreator,
            List<Tab> tabs,
            boolean updateTimestamp,
            boolean areTabsBeingOpened) {
        ThreadUtils.assertOnUiThread();
        int tabCount = 0;
        for (Tab tab : tabs) {
            // Update the timestamp so that the tab isn't immediately re-archived on the next pass.
            if (updateTimestamp) {
                tab.setTimestampMillis(System.currentTimeMillis());
            }

            TabState tabState = prepareTabState(tab);
            // Restore tab at the "start" of the list.
            Tab newTab =
                    tabCreator.createFrozenTab(
                            tabState, tab.getId(), areTabsBeingOpened ? INVALID_TAB_INDEX : 0);
            if (newTab != null) {
                tabCount++;
                newTab.onTabRestoredFromArchivedTabModel();
            }
        }

        mArchivedTabGroupModelFilter
                .getTabModel()
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(tabs).allowUndo(false).build(),
                        /* allowDialog= */ false);
        RecordHistogram.recordCount1000Histogram("Tabs.ArchivedTabRestored.TabCount", tabCount);
    }

    @Override
    public void rescueArchivedTabs(TabCreator regularTabCreator) {
        ThreadUtils.assertOnUiThread();
        unarchiveAndRestoreTabs(
                regularTabCreator,
                TabModelUtils.convertTabListToListOfTabs(
                        mArchivedTabGroupModelFilter.getTabModel()),
                /* updateTimestamp= */ false,
                /* areTabsBeingOpened= */ false);
        RecordUserAction.record("Tabs.ArchivedTabRescued");
    }

    // Private functions.

    @VisibleForTesting
    void initializePersistedTabDataAsync(List<Tab> archivedTabs) {
        postUiThreadCancellableTask(
                () ->
                        initializePersistedTabDataAsyncImpl(
                                archivedTabs, /* currentIndex= */ 0, mClock.currentTimeMillis()));
    }

    void initializePersistedTabDataAsyncImpl(
            List<Tab> archivedTabs, int currentIndex, long startTimeMs) {
        if (currentIndex >= archivedTabs.size()) {
            RecordHistogram.recordTimesHistogram(
                    "Tabs.InitializePTD.DurationMs", mClock.currentTimeMillis() - startTimeMs);
            broadcastPersistedTabDataCreated();
            return;
        }

        Callback<@Nullable ArchivePersistedTabData> callback =
                (@Nullable ArchivePersistedTabData archivePersistedTabData) -> {
                    if (archivePersistedTabData != null) {
                        // Persisted tab data requires a true supplier before saving to
                        // disk.
                        archivePersistedTabData.registerIsTabSaveEnabledSupplier(
                                new ObservableSupplierImpl<>(true));
                        archivePersistedTabData.setArchivedTimeMs(mClock.currentTimeMillis());
                    }

                    postUiThreadCancellableTask(
                            () ->
                                    initializePersistedTabDataAsyncImpl(
                                            archivedTabs, currentIndex + 1, startTimeMs));
                };
        callback = mCallbackController.makeCancelable(callback);

        ArchivePersistedTabData.from(archivedTabs.get(currentIndex), callback);
    }

    void deleteArchivedTabsIfEligibleAsync(
            List<Tab> tabs, long startTimeMs, AtomicInteger autodeleteTasksRemaining) {
        postUiThreadCancellableTask(
                () ->
                        deleteArchivedTabsIfEligibleAsyncImpl(
                                tabs,
                                /* currentIndex= */ 0,
                                startTimeMs,
                                autodeleteTasksRemaining));
    }

    void deleteArchivedTabsIfEligibleAsyncImpl(
            List<Tab> tabs,
            int currentIndex,
            long startTimeMs,
            AtomicInteger autodeleteTasksRemaining) {
        if (currentIndex >= tabs.size()) {
            RecordHistogram.recordTimesHistogram(
                    "Tabs.DeleteWithPTD.DurationMs", mClock.currentTimeMillis() - startTimeMs);
            if (autodeleteTasksRemaining.decrementAndGet() == 0) {
                broadcastAutodeletePassComplete();
            }
            return;
        }

        Tab tab = tabs.get(currentIndex);
        Callback<@Nullable ArchivePersistedTabData> callback =
                (@Nullable ArchivePersistedTabData archivePersistedTabData) -> {
                    if (isArchivedTabEligibleForDeletion(archivePersistedTabData)) {
                        int tabAgeDays =
                                timestampMillisToDays(archivePersistedTabData.getArchivedTimeMs());
                        mArchivedTabGroupModelFilter
                                .getTabModel()
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                        /* allowDialog= */ false);
                        RecordHistogram.recordCount1000Histogram(
                                "Tabs.TabAutoDeleted.AfterNDays", tabAgeDays);
                        RecordUserAction.record("Tabs.ArchivedTabAutoDeleted");
                    }
                    postUiThreadCancellableTask(
                            () ->
                                    deleteArchivedTabsIfEligibleAsyncImpl(
                                            tabs,
                                            currentIndex + 1,
                                            startTimeMs,
                                            autodeleteTasksRemaining));
                };
        callback = mCallbackController.makeCancelable(callback);

        ArchivePersistedTabData.from(tab, callback);
    }

    void deleteArchivedTabGroupsIfEligibleAsync(
            List<SavedTabGroup> tabGroups,
            long startTimeMs,
            AtomicInteger autodeleteTasksRemaining) {
        postUiThreadCancellableTask(
                () ->
                        deleteArchivedTabGroupsIfEligibleAsyncImpl(
                                tabGroups,
                                /* currentIndex= */ 0,
                                startTimeMs,
                                autodeleteTasksRemaining));
    }

    void deleteArchivedTabGroupsIfEligibleAsyncImpl(
            List<SavedTabGroup> tabGroups,
            int currentIndex,
            long startTimeMs,
            AtomicInteger autodeleteTasksRemaining) {
        if (currentIndex >= tabGroups.size()) {
            RecordHistogram.recordTimesHistogram(
                    "TabGroups.AutodeletePass.DurationMs",
                    mClock.currentTimeMillis() - startTimeMs);
            if (autodeleteTasksRemaining.decrementAndGet() == 0) {
                broadcastAutodeletePassComplete();
            }
            return;
        }

        SavedTabGroup tabGroup = tabGroups.get(currentIndex);
        if (tabGroup != null && tabGroup.archivalTimeMs != null) {
            int tabGroupArchivedDays = timestampMillisToDays(tabGroup.archivalTimeMs);
            RecordHistogram.recordCount1000Histogram(
                    "TabGroups.TabGroupAutoDeleteEligibilityCheck.AfterNDays",
                    tabGroupArchivedDays);
            if (isTimestampWithinTargetHours(
                            tabGroup.archivalTimeMs,
                            mTabArchiveSettings.getAutoDeleteTimeDeltaHours())
                    && tabGroup.syncId != null) {
                mTabGroupSyncService.updateArchivalStatus(
                        tabGroup.syncId, /* archivalStatus= */ false);
                RecordHistogram.recordCount1000Histogram(
                        "TabGroups.TabGroupAutoDeleted.TabCount", tabGroup.savedTabs.size());
                RecordUserAction.record("TabGroups.ArchivedTabGroupAutoDeleted");
            }
        }

        postUiThreadCancellableTask(
                () ->
                        deleteArchivedTabGroupsIfEligibleAsyncImpl(
                                tabGroups,
                                currentIndex + 1,
                                startTimeMs,
                                autodeleteTasksRemaining));
    }

    // Check if tab groups are eligible for archive. Only archive a tab group if all tabs in that
    // group pass archiving eligibility criteria.
    private boolean isGroupTabEligibleForArchive(
            TabGroupModelFilter regularTabGroupModelFilter,
            Map<Token, Boolean> groupIdToArchiveEligibilityMap,
            Map<GURL, Long> tabUrlToLastActiveTimestampMap,
            Tab tab) {
        if (ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()) {
            // Create a map between group id tokens and their archive eligibility. If a group has
            // not been checked yet, check all related tabs and assign a status so that tabs with
            // that group id token can be bypassed in future iterations of this checking cycle.
            Token tabGroupId = tab.getTabGroupId();
            if (groupIdToArchiveEligibilityMap.containsKey(tabGroupId)) {
                return groupIdToArchiveEligibilityMap.get(tabGroupId);
            } else {
                boolean isTabGroupEligibleForArchive =
                        isTabGroupEligibleForArchive(
                                regularTabGroupModelFilter, tabUrlToLastActiveTimestampMap, tab);
                groupIdToArchiveEligibilityMap.put(tabGroupId, isTabGroupEligibleForArchive);
                return isTabGroupEligibleForArchive;
            }
        } else {
            return false;
        }
    }

    private boolean isTabGroupEligibleForArchive(
            TabGroupModelFilter regularTabGroupModelFilter,
            Map<GURL, Long> tabUrlToLastActiveTimestampMap,
            Tab tab) {
        // Do not archived shared tab groups, defined by a null collaboration ID.
        if (TabShareUtils.getCollaborationIdOrNull(
                        tab.getId(), regularTabGroupModelFilter.getTabModel(), mTabGroupSyncService)
                != null) {
            return false;
        }

        List<Tab> relatedTabList = regularTabGroupModelFilter.getTabsInGroup(tab.getTabGroupId());
        for (Tab relatedTab : relatedTabList) {
            if (!isTabEligibleForArchive(tabUrlToLastActiveTimestampMap, relatedTab)) {
                return false;
            }
        }
        return true;
    }

    private boolean isTabEligibleForArchive(
            Map<GURL, Long> tabUrlToLastActiveTimestampMap, Tab tab) {
        TabState tabState = TabStateExtractor.from(tab);
        if (tabState == null || tabState.contentsState == null) return false;

        long timestampMillis = tab.getTimestampMillis();
        int tabAgeDays = timestampMillisToDays(timestampMillis);
        boolean isTabTimestampEligibleForArchive =
                isTimestampWithinTargetHours(
                        timestampMillis, mTabArchiveSettings.getArchiveTimeDeltaHours());
        boolean isDuplicateTabEligibleForArchive =
                mTabArchiveSettings.isArchiveDuplicateTabsEnabled()
                        ? isDuplicateTab(tabUrlToLastActiveTimestampMap, tab)
                        : false;
        RecordHistogram.recordCount1000Histogram(
                "Tabs.TabArchiveEligibilityCheck.AfterNDays", tabAgeDays);
        if (isDuplicateTabEligibleForArchive) {
            RecordUserAction.record("Tabs.ArchivedDuplicateTab");
        }
        return isTabTimestampEligibleForArchive || isDuplicateTabEligibleForArchive;
    }

    @Contract("null -> false")
    private boolean isArchivedTabEligibleForDeletion(
            @Nullable ArchivePersistedTabData archivePersistedTabData) {
        if (archivePersistedTabData == null) return false;

        long archivedTimeMillis = archivePersistedTabData.getArchivedTimeMs();
        int tabAgeDays = timestampMillisToDays(archivedTimeMillis);
        boolean result =
                isTimestampWithinTargetHours(
                        archivedTimeMillis, mTabArchiveSettings.getAutoDeleteTimeDeltaHours());
        RecordHistogram.recordCount1000Histogram(
                "Tabs.TabAutoDeleteEligibilityCheck.AfterNDays", tabAgeDays);
        return result;
    }

    // A tab is marked as a duplicate tab if its last active timestamp is not the same as the stored
    // last active timestamp in the model wide hashmap.
    private boolean isDuplicateTab(Map<GURL, Long> tabUrlToLastActiveTimestampMap, Tab tab) {
        GURL url = tab.getUrl();

        // If the tab URL does not exist in the map or the tab is part of a group, no op.
        if (!tabUrlToLastActiveTimestampMap.containsKey(url) || tab.getTabGroupId() != null) {
            return false;
        }

        long tabLastActiveTimestamp = tab.getTimestampMillis();
        long currentUrlLastActiveTimestamp = tabUrlToLastActiveTimestampMap.get(url);

        return currentUrlLastActiveTimestamp > tabLastActiveTimestamp;
    }

    // Check all tabs in the tab model and record unique URLs and the latest last active timestamp.
    private Map<GURL, Long> createUrlToMruTimestampMap(TabModel model) {
        Map<GURL, Long> urlToTimestampMap = new HashMap<>();
        if (!mTabArchiveSettings.isArchiveDuplicateTabsEnabled()) {
            return urlToTimestampMap;
        }
        for (Tab tab : model) {
            GURL url = tab.getUrl();
            long tabLastActiveTimestamp = tab.getTimestampMillis();

            // Only record tabs that are not part of a tab group to avoid deduplicating them.
            if (tab.getTabGroupId() == null) {
                if (urlToTimestampMap.containsKey(url)) {
                    long currentUrlLastActiveTimestamp = urlToTimestampMap.get(url);
                    if (tabLastActiveTimestamp <= currentUrlLastActiveTimestamp) {
                        continue;
                    }
                }
                urlToTimestampMap.put(url, tabLastActiveTimestamp);
            }
        }
        return urlToTimestampMap;
    }

    private boolean isTimestampWithinTargetHours(long timestampMillis, int targetHours) {
        if (timestampMillis == INVALID_TIMESTAMP) return false;

        long ageHours = TimeUnit.MILLISECONDS.toHours(mClock.currentTimeMillis() - timestampMillis);
        return ageHours >= targetHours;
    }

    private int timestampMillisToDays(long timestampMillis) {
        if (timestampMillis == INVALID_TIMESTAMP) return (int) INVALID_TIMESTAMP;

        return (int) TimeUnit.MILLISECONDS.toDays(mClock.currentTimeMillis() - timestampMillis);
    }

    /** Extracts the tab state and prepares it for archive/restore. */
    private TabState prepareTabState(Tab tab) {
        TabState tabState = assumeNonNull(TabStateExtractor.from(tab));
        // Strip the parent id to avoid ordering issues within the tab model.
        tabState.parentId = Tab.INVALID_TAB_ID;
        // Strip the root id to avoid re-using the old rootId from the tab state file.
        tabState.rootId = Tab.INVALID_TAB_ID;
        return tabState;
    }

    @VisibleForTesting
    void ensureArchivedTabsHaveCorrectFields() {
        TabModel model = mArchivedTabGroupModelFilter.getTabModel();
        for (Tab archivedTab : model) {
            // Archived tabs shouldn't have a root id or parent id. It's possible that there's
            // stale data around for clients that have archived tabs prior to crrev.com/c/5750590
            // landing. Fix those fields so that they're corrected in the tab state file.
            archivedTab.setRootId(archivedTab.getId());
            archivedTab.setParentId(Tab.INVALID_TAB_ID);
        }
    }

    private void broadcastDeclutterComplete() {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    for (Observer obs : mObservers) {
                        obs.onDeclutterPassCompleted();
                    }
                });

        // Store the UI {@link ThemeType} at the current instant to compare with the up-to-date
        // theme setting during the next declutter pass.
        sUiThemeSetting = NightModeUtils.getThemeSetting();
    }

    private void broadcastPersistedTabDataCreated() {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    for (Observer obs : mObservers) {
                        obs.onArchivePersistedTabDataCreated();
                    }
                });
    }

    private void broadcastAutodeletePassComplete() {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    for (Observer obs : mObservers) {
                        obs.onAutodeletePassCompleted();
                    }
                });
    }

    // Determine if the user was active during the declutter inactivity period by checking all tabs
    // in the tab model to see if the youngest tab is outside of that threshold.
    private boolean isUserActive(TabModel model) {
        if (ChromeFeatureList.sAndroidTabDeclutterArchiveAllButActiveTab.isEnabled()
                || !ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()) {
            return true;
        }

        long lastActiveTabTimestamp = 0L;
        for (Tab tab : model) {
            if (TabModelUtils.getCurrentTabId(model) == tab.getId()) {
                continue;
            }
            lastActiveTabTimestamp =
                    Math.max(
                            lastActiveTabTimestamp,
                            tab.getLastNavigationCommittedTimestampMillis());
        }

        // If the last active tab's navigation timestamp is within the target hours (they exceed
        // the inactivity grace period provided by delta hours), do not perform a declutter pass
        // as the user is considered inactive.
        if (isTimestampWithinTargetHours(
                lastActiveTabTimestamp, mTabArchiveSettings.getArchiveTimeDeltaHours())) {
            return false;
        }
        return true;
    }

    // Returns whether the UI theme was changed since the time of last check as it causes an app
    // restart and runs a declutter pass.
    private boolean wasUiThemeChanged() {
        return sUiThemeSetting != NightModeUtils.getThemeSetting();
    }

    // Helper method to reduce boilerplate needed when posting cancellable task.
    private void postUiThreadCancellableTask(Runnable runnable) {
        PostTask.postTask(TaskTraits.UI_DEFAULT, mCallbackController.makeCancelable(runnable));
    }

    // Testing-specific methods.

    public void setClockForTesting(Clock clock) {
        mClock = clock;
    }

    ObserverList<Observer> getObserversForTesting() {
        return mObservers;
    }
}
