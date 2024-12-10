// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.chrome.browser.tab.Tab.INVALID_TIMESTAMP;
import static org.chromium.chrome.browser.tabmodel.TabList.INVALID_TAB_INDEX;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.state.ArchivePersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Responsible for moving tabs to/from the archived {@link TabModel}. */
public class TabArchiver implements TabWindowManager.Observer {
    /** Provides the current timestamp. */
    @FunctionalInterface
    public interface Clock {
        long currentTimeMillis();
    }

    /** Provides an interface to observer the declutter process. */
    public interface Observer {
        void onDeclutterPassCompleted();
    }

    private final CallbackController mCallbackController = new CallbackController();
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final TabGroupModelFilter mArchivedTabGroupModelFilter;
    private final TabCreator mArchivedTabCreator;
    private final TabWindowManager mTabWindowManager;
    private final TabArchiveSettings mTabArchiveSettings;

    private Clock mClock;
    private boolean mDeclutterInitCalled;
    private int mSelectorsQueuedForDeclutter;

    /**
     * @param archivedTabGroupModelFilter The archived {@link TabGroupModelFilter}.
     * @param archivedTabCreator The {@link TabCreator} for the archived TabModel.
     * @param tabWindowManager The {@link TabWindowManager} used for accessing TabModelSelectors.
     * @param tabArchiveSettings The settings for tab archiving/deletion.
     * @param clock A clock object to get the current time..
     */
    public TabArchiver(
            TabGroupModelFilter archivedTabGroupModelFilter,
            TabCreator archivedTabCreator,
            TabWindowManager tabWindowManager,
            TabArchiveSettings tabArchiveSettings,
            Clock clock) {
        mArchivedTabGroupModelFilter = archivedTabGroupModelFilter;
        mArchivedTabCreator = archivedTabCreator;
        mTabWindowManager = tabWindowManager;
        mTabArchiveSettings = tabArchiveSettings;
        mClock = clock;
    }

    /** Destroys this object. */
    public void destroy() {
        mCallbackController.destroy();
        mTabWindowManager.removeObserver(this);
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /** Initialize the archiving process by observing TabWindowManager for new TabModelSelectors. */
    public void initDeclutter() {
        ThreadUtils.assertOnUiThread();
        // Observe new TabModelSelectors being added so inactive tabs are archived automatically
        // as new selectors are activated.
        mTabWindowManager.addObserver(this);

        mDeclutterInitCalled = true;
    }

    /**
     * 1. Iterates through all known tab model selects, and archives inactive tabs. 2. Iterates
     * through all archived tabs, and automatically deletes those old enough.
     */
    public void triggerScheduledDeclutter() {
        ThreadUtils.assertOnUiThread();
        assert mDeclutterInitCalled;

        // Wait for the declutter pass to complete, then do follow-up tasks.
        addObserver(
                new Observer() {
                    @Override
                    public void onDeclutterPassCompleted() {
                        removeObserver(this);
                        // Trigger auto-deletion after archiving tabs.
                        deleteEligibleArchivedTabs();
                        ensureArchivedTabsHaveCorrectFields();
                    }
                });

        // Trigger archival of inactive tabs for the current selectors.
        for (int i = 0; i < mTabWindowManager.getMaxSimultaneousSelectors(); i++) {
            TabModelSelector selector = mTabWindowManager.getTabModelSelectorById(i);
            if (selector == null) continue;
            mSelectorsQueuedForDeclutter++;
            onTabModelSelectorAdded(selector);
        }
    }

    /** Delete eligible archived tabs. */
    public void deleteEligibleArchivedTabs() {
        ThreadUtils.assertOnUiThread();
        if (!mTabArchiveSettings.isAutoDeleteEnabled()) return;

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mArchivedTabGroupModelFilter.getTabModel().getCount(); i++) {
            tabs.add(mArchivedTabGroupModelFilter.getTabModel().getTabAt(i));
        }

        for (Tab tab : tabs) {
            ArchivePersistedTabData.from(
                    tab,
                    (archivePersistedTabData) -> {
                        if (isArchivedTabEligibleForDeletion(archivePersistedTabData)) {
                            int tabAgeDays =
                                    timestampMillisToDays(
                                            archivePersistedTabData.getArchivedTimeMs());
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
                    });
        }
    }

    /**
     * Rescue archived tabs, moving them back to the regular TabModel. This is done when the feature
     * is disabled, but there are still archived tabs.
     */
    public void rescueArchivedTabs(TabCreator regularTabCreator) {
        ThreadUtils.assertOnUiThread();
        unarchiveAndRestoreTabs(
                regularTabCreator,
                TabModelUtils.convertTabListToListOfTabs(mArchivedTabGroupModelFilter),
                /* updateTimestamp= */ false,
                /* areTabsBeingOpened= */ false);
        RecordUserAction.record("Tabs.ArchivedTabRescued");
    }

    /**
     * Create an archived copy of the given Tab in the archived TabModel, and close the Tab in the
     * regular TabModel. Must be called on the UI thread.
     *
     * @param tabModel The {@link TabModel} the tab currently belongs to.
     * @param tabs The list {@link Tab}s to unarchive.
     */
    public void archiveAndRemoveTabs(TabModel tabModel, List<Tab> tabs) {
        ThreadUtils.assertOnUiThread();

        List<Tab> archivedTabs = new ArrayList<>();
        // Add tabs to the archived tab model first to prevent tab loss if the operation is aborted.
        for (Tab tab : tabs) {
            TabState tabState = prepareTabState(tab);
            Tab archivedTab =
                    mArchivedTabCreator.createFrozenTab(tabState, tab.getId(), INVALID_TAB_INDEX);
            archivedTabs.add(archivedTab);
        }

        int tabCount = tabs.size();
        // Once the archived tabs are added, do a bulk closure from the regular tab model.
        tabModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(tabs).allowUndo(false).build(),
                        /* allowDialog= */ false);
        RecordHistogram.recordCount1000Histogram("Tabs.TabArchived.TabCount", tabCount);

        for (Tab archivedTab : archivedTabs) {
            // Post initializing the tab data to prevent more work in an already heavy function.
            ThreadUtils.postOnUiThread(
                    () -> {
                        initializePersistedTabData(archivedTab);
                    });
        }
    }

    void initializePersistedTabData(Tab archivedTab) {
        ArchivePersistedTabData.from(
                archivedTab,
                (archivePersistedTabData) -> {
                    if (archivePersistedTabData == null) {
                        return;
                    }
                    // Persisted tab data requires a true supplier before saving to disk.
                    archivePersistedTabData.registerIsTabSaveEnabledSupplier(
                            new ObservableSupplierImpl<>(true));
                    archivePersistedTabData.setArchivedTimeMs(mClock.currentTimeMillis());
                });
    }

    /**
     * Unarchive the given tab, moving it into the normal TabModel. The tab is reused between the
     * archived/regular TabModels. Must be called on the UI thread.
     *
     * @param tabCreator The {@link TabCreator} to use when recreating the tabs.
     * @param tabs The {@link Tab}s to unarchive.
     * @param updateTimestamp Whether the Tab's timestamp should be updated.
     * @param areTabsBeingOpened Whether the restored tab is being opened.
     */
    public void unarchiveAndRestoreTabs(
            TabCreator tabCreator,
            List<Tab> tabs,
            boolean updateTimestamp,
            boolean areTabsBeingOpened) {
        ThreadUtils.assertOnUiThread();
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
            newTab.onTabRestoredFromArchivedTabModel();
        }

        int tabCount = tabs.size();
        mArchivedTabGroupModelFilter
                .getTabModel()
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTabs(tabs).allowUndo(false).build(),
                        /* allowDialog= */ false);
        RecordHistogram.recordCount1000Histogram("Tabs.ArchivedTabRestored.TabCount", tabCount);
    }

    // TabWindowManager.Observer implementation.

    @Override
    public void onTabModelSelectorAdded(TabModelSelector selector) {
        ThreadUtils.assertOnUiThread();
        if (!mTabArchiveSettings.getArchiveEnabled()) return;

        TabModelUtils.runOnTabStateInitialized(
                selector, this::archiveEligibleTabsFromTabModelSelector);
    }

    // Private functions.

    private void archiveEligibleTabsFromTabModelSelector(TabModelSelector selector) {
        ThreadUtils.postOnUiThread(
                mCallbackController.makeCancelable(
                        () -> {
                            TabGroupModelFilter regularTabGroupModelFilter =
                                    selector.getTabGroupModelFilterProvider()
                                            .getCurrentTabGroupModelFilter();
                            TabModel model = regularTabGroupModelFilter.getTabModel();
                            int activeTabId = TabModelUtils.getCurrentTabId(model);
                            List<Tab> tabsToClose = new ArrayList<>();
                            List<Tab> tabsToArchive = new ArrayList<>();
                            HashMap<Token, Boolean> groupIdToArchiveEligibilityMap =
                                    new HashMap<>();
                            HashMap<GURL, Long> tabUrlToLastActiveTimestampMap = new HashMap<>();
                            if (shouldArchiveDuplicateTabs()) {
                                recordTabUrlLastActiveTimestamps(
                                        tabUrlToLastActiveTimestampMap, model);
                            }

                            for (int i = 0; i < model.getCount(); i++) {
                                Tab tab = model.getTabAt(i);
                                // If there's an existing archived tab for the tab id, then we've
                                // run into a case where the tab metadata file wasn't updated after
                                // an archive or restore pass. Remove the tab from the regular tab
                                // model since the tab was already archived.
                                Tab archivedTab =
                                        mArchivedTabGroupModelFilter
                                                .getTabModel()
                                                .getTabById(tab.getId());
                                if (archivedTab != null) {
                                    tabsToClose.add(tab);
                                } else if (activeTabId != tab.getId()) {
                                    // If the tab is not part of a tab group or the flag is not
                                    // enabled, bypass this for the original check on a single tab.
                                    boolean isTabOrGroupTabEligibleForArchive =
                                            tab.getTabGroupId() != null
                                                    ? isGroupTabEligibleForArchive(
                                                            regularTabGroupModelFilter,
                                                            groupIdToArchiveEligibilityMap,
                                                            tabUrlToLastActiveTimestampMap,
                                                            tab)
                                                    : isTabEligibleForArchive(
                                                            tabUrlToLastActiveTimestampMap, tab);
                                    if (isTabOrGroupTabEligibleForArchive) {
                                        tabsToArchive.add(tab);
                                    }
                                }
                            }
                            if (tabsToClose.size() > 0) {
                                model.getTabRemover()
                                        .closeTabs(
                                                TabClosureParams.closeTabs(tabsToClose)
                                                        .allowUndo(false)
                                                        .build(),
                                                /* allowDialog= */ false);
                            }
                            if (tabsToArchive.size() > 0) {
                                archiveAndRemoveTabs(model, tabsToArchive);
                            }
                            mSelectorsQueuedForDeclutter--;
                            RecordHistogram.recordCount1000Histogram(
                                    "Tabs.TabArchived.FoundDuplicateInRegularModel",
                                    tabsToClose.size());

                            if (mSelectorsQueuedForDeclutter == 0) {
                                for (Observer obs : mObservers) {
                                    obs.onDeclutterPassCompleted();
                                }
                            }
                        }));
    }

    // Check if tab groups are eligible for archive. Only archive a tab group if all tabs in that
    // group pass archiving eligibility criteria.
    private boolean isGroupTabEligibleForArchive(
            TabGroupModelFilter regularTabGroupModelFilter,
            HashMap<Token, Boolean> groupIdToArchiveEligibilityMap,
            HashMap<GURL, Long> tabUrlToLastActiveTimestampMap,
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
            HashMap<GURL, Long> tabUrlToLastActiveTimestampMap,
            Tab tab) {
        List<Tab> relatedTabList =
                regularTabGroupModelFilter.getRelatedTabListForRootId(tab.getRootId());
        for (Tab relatedTab : relatedTabList) {
            if (!isTabEligibleForArchive(tabUrlToLastActiveTimestampMap, relatedTab)) {
                return false;
            }
        }
        return true;
    }

    private boolean isTabEligibleForArchive(
            HashMap<GURL, Long> tabUrlToLastActiveTimestampMap, Tab tab) {
        TabState tabState = TabStateExtractor.from(tab);
        if (tabState.contentsState == null) return false;

        long timestampMillis = tab.getTimestampMillis();
        int tabAgeDays = timestampMillisToDays(timestampMillis);
        boolean isTabTimestampEligibleForArchive =
                isTimestampWithinTargetHours(
                        timestampMillis, mTabArchiveSettings.getArchiveTimeDeltaHours());
        boolean isDuplicateTabEligibleForArchive =
                shouldArchiveDuplicateTabs()
                        ? isDuplicateTab(tabUrlToLastActiveTimestampMap, tab)
                        : false;
        RecordHistogram.recordCount1000Histogram(
                "Tabs.TabArchiveEligibilityCheck.AfterNDays", tabAgeDays);
        return isTabTimestampEligibleForArchive || isDuplicateTabEligibleForArchive;
    }

    private boolean isArchivedTabEligibleForDeletion(
            ArchivePersistedTabData archivePersistedTabData) {
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
    private boolean isDuplicateTab(HashMap<GURL, Long> tabUrlToLastActiveTimestampMap, Tab tab) {
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
    private void recordTabUrlLastActiveTimestamps(
            HashMap<GURL, Long> tabUrlToLastActiveTimestampMap, TabModel model) {
        for (int i = 0; i < model.getCount(); i++) {
            Tab tab = model.getTabAt(i);
            GURL url = tab.getUrl();
            long tabLastActiveTimestamp = tab.getTimestampMillis();

            // Only record tabs that are not part of a tab group to avoid deduplicating them.
            if (tab.getTabGroupId() == null) {
                if (tabUrlToLastActiveTimestampMap.containsKey(url)) {
                    long currentUrlLastActiveTimestamp = tabUrlToLastActiveTimestampMap.get(url);
                    if (tabLastActiveTimestamp <= currentUrlLastActiveTimestamp) {
                        continue;
                    }
                }
                tabUrlToLastActiveTimestampMap.put(url, tabLastActiveTimestamp);
            }
        }
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
        TabState tabState = TabStateExtractor.from(tab);
        // Strip the parent id to avoid ordering issues within the tab model.
        tabState.parentId = Tab.INVALID_TAB_ID;
        // Strip the root id to avoid re-using the old rootId from the tab state file.
        tabState.rootId = Tab.INVALID_TAB_ID;
        return tabState;
    }

    private boolean shouldArchiveDuplicateTabs() {
        return ChromeFeatureList.sAndroidTabDeclutterArchiveDuplicateTabs.isEnabled();
    }

    @VisibleForTesting
    void ensureArchivedTabsHaveCorrectFields() {
        for (int i = 0; i < mArchivedTabGroupModelFilter.getTabModel().getCount(); i++) {
            Tab archivedTab = mArchivedTabGroupModelFilter.getTabModel().getTabAt(i);
            // Archived tabs shouldn't have a root id or parent id. It's possible that there's
            // stale data around for clients that have archived tabs prior to crrev.com/c/5750590
            // landing. Fix those fields so that they're corrected in the tab state file.
            archivedTab.setRootId(archivedTab.getId());
            archivedTab.setParentId(Tab.INVALID_TAB_ID);
        }
    }

    // Testing-specific methods.

    public void setClockForTesting(Clock clock) {
        mClock = clock;
    }

    ObserverList<Observer> getObserversForTesting() {
        return mObservers;
    }
}
