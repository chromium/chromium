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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tab.state.ArchivePersistedTabData;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;

import java.util.ArrayList;
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
    private final TabModel mArchivedTabModel;
    private final TabCreator mArchivedTabCreator;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final TabWindowManager mTabWindowManager;
    private final TabArchiveSettings mTabArchiveSettings;
    private final Clock mClock;

    private boolean mDeclutterInitCalled;
    private int mSelectorsQueuedForDeclutter;

    /**
     * @param archivedTabModel The archived {@link TabModel}.
     * @param archivedTabCreator The {@link TabCreator} for the archived TabModel.
     * @param asyncTabParamsManager The {@link AsyncTabParamsManager} used when unarchiving tabs.
     * @param tabWindowManager The {@link TabWindowManager} used for accessing TabModelSelectors.
     * @param tabArchiveSettings The settings for tab archiving/deletion.
     * @param clock A clock object to get the current time..
     */
    public TabArchiver(
            TabModel archivedTabModel,
            TabCreator archivedTabCreator,
            AsyncTabParamsManager asyncTabParamsManager,
            TabWindowManager tabWindowManager,
            TabArchiveSettings tabArchiveSettings,
            Clock clock) {
        mArchivedTabModel = archivedTabModel;
        mArchivedTabCreator = archivedTabCreator;
        mAsyncTabParamsManager = asyncTabParamsManager;
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

        // Trigger archival of inactive tabs for the current selectors.
        for (int i = 0; i < mTabWindowManager.getMaxSimultaneousSelectors(); i++) {
            TabModelSelector selector = mTabWindowManager.getTabModelSelectorById(i);
            if (selector == null) continue;
            mSelectorsQueuedForDeclutter++;
            onTabModelSelectorAdded(selector);
        }

        // Trigger auto-deletion after archiving tabs.
        deleteEligibleArchivedTabs();
        ensureArchivedTabsHaveCorrectFields();
    }

    /** Delete eligible archived tabs. */
    public void deleteEligibleArchivedTabs() {
        ThreadUtils.assertOnUiThread();
        if (!mTabArchiveSettings.isAutoDeleteEnabled()) return;

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mArchivedTabModel.getCount(); i++) {
            tabs.add(mArchivedTabModel.getTabAt(i));
        }

        for (Tab tab : tabs) {
            ArchivePersistedTabData.from(
                    tab,
                    (archivePersistedTabData) -> {
                        if (isArchivedTabEligibleForDeletion(archivePersistedTabData)) {
                            int tabAgeDays =
                                    timestampMillisToDays(
                                            archivePersistedTabData.getArchivedTimeMs());
                            mArchivedTabModel.closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(false).build());
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
        while (mArchivedTabModel.getCount() > 0) {
            Tab tab = mArchivedTabModel.getTabAt(0);
            unarchiveAndRestoreTab(regularTabCreator, tab);
            RecordUserAction.record("Tabs.ArchivedTabRescued");
        }
    }

    /**
     * Create an archived copy of the given Tab in the archived TabModel, and close the Tab in the
     * regular TabModel. Must be called on the UI thread.
     *
     * @param tabModel The {@link TabModel} the tab currently belongs to.
     * @param tab The {@link Tab} to unarchive.
     * @return The archived {@link Tab}.
     */
    public Tab archiveAndRemoveTab(TabModel tabModel, Tab tab) {
        ThreadUtils.assertOnUiThread();
        int tabAgeDays = timestampMillisToDays(tab.getTimestampMillis());
        TabState tabState = prepareTabState(tab);
        Tab newTab = mArchivedTabCreator.createFrozenTab(tabState, tab.getId(), INVALID_TAB_INDEX);
        tabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(false).build());

        ArchivePersistedTabData.from(
                newTab,
                (archivePersistedTabData) -> {
                    // Persisted tab data requires a true supplier before saving to disk.
                    archivePersistedTabData.registerIsTabSaveEnabledSupplier(
                            new ObservableSupplierImpl<>(true));
                    archivePersistedTabData.setArchivedTimeMs(mClock.currentTimeMillis());
                });

        RecordHistogram.recordCount1000Histogram("Tabs.TabArchived.AfterNDays", tabAgeDays);
        RecordUserAction.record("Tabs.TabArchived");
        return newTab;
    }

    /**
     * Unarchive the given tab, moving it into the normal TabModel. The tab is reused between the
     * archived/regular TabModels. Must be called on the UI thread.
     *
     * @param tabCreator The {@link TabCreator} to use when recreating the tabs.
     * @param tab The {@link Tab} to unarchive.
     */
    public void unarchiveAndRestoreTab(TabCreator tabCreator, Tab tab) {
        ThreadUtils.assertOnUiThread();
        TabState tabState = prepareTabState(tab);
        mArchivedTabModel.removeTab(tab);
        mAsyncTabParamsManager.add(tab.getId(), new TabReparentingParams(tab, null));
        tabCreator.createFrozenTab(tabState, tab.getId(), INVALID_TAB_INDEX);
        RecordUserAction.record("Tabs.ArchivedTabRestored");
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
                            int numExistingRegularTabsFound = 0;
                            TabModel model = selector.getModel(/* isIncognito= */ false);
                            int activeTabId = TabModelUtils.getCurrentTabId(model);
                            for (int i = 0; i < model.getCount(); ) {
                                Tab tab = model.getTabAt(i);
                                // If there's an existing archived tab for the tab id, then we've
                                // run into a case where the tab metadata file wasn't updated after
                                // an archive or restore pass. Remove the tab from the regular tab
                                // model since the tab was already archived.
                                Tab archivedTab = mArchivedTabModel.getTabById(tab.getId());
                                if (archivedTab != null) {
                                    numExistingRegularTabsFound++;
                                    model.closeTabs(
                                            TabClosureParams.closeTab(tab)
                                                    .allowUndo(false)
                                                    .build());
                                } else if (activeTabId != tab.getId()
                                        && isTabEligibleForArchive(tab)) {
                                    archiveAndRemoveTab(model, tab);
                                } else {
                                    i++;
                                }
                            }
                            mSelectorsQueuedForDeclutter--;
                            RecordHistogram.recordCount1000Histogram(
                                    "Tabs.TabArchived.FoundDuplicateInRegularModel",
                                    numExistingRegularTabsFound);

                            if (mSelectorsQueuedForDeclutter == 0) {
                                for (Observer obs : mObservers) {
                                    obs.onDeclutterPassCompleted();
                                }
                            }
                        }));
    }

    private boolean isTabEligibleForArchive(Tab tab) {
        // Explicitly prevent grouped tabs from getting archived.
        if (tab.getTabGroupId() != null) return false;
        TabState tabState = TabStateExtractor.from(tab);
        if (tabState.contentsState == null) return false;

        long timestampMillis = tab.getTimestampMillis();
        int tabAgeDays = timestampMillisToDays(timestampMillis);
        boolean result =
                isTimestampWithinTargetHours(
                        timestampMillis, mTabArchiveSettings.getArchiveTimeDeltaHours());
        RecordHistogram.recordCount1000Histogram(
                "Tabs.TabArchiveEligibilityCheck.AfterNDays", tabAgeDays);
        return result;
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

    @VisibleForTesting
    void ensureArchivedTabsHaveCorrectFields() {
        for (int i = 0; i < mArchivedTabModel.getCount(); i++) {
            Tab archivedTab = mArchivedTabModel.getTabAt(i);
            // Archived tabs shouldn't have a root id or parent id. It's possible that there's
            // stale data around for clients that have archived tabs prior to crrev.com/c/5750590
            // landing. Fix those fields so that they're corrected in the tab state file.
            archivedTab.setRootId(archivedTab.getId());
            archivedTab.setParentId(Tab.INVALID_TAB_ID);
        }
    }
}
