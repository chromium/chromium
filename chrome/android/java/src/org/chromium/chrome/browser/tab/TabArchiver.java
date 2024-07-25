// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.chrome.browser.tab.Tab.INVALID_TIMESTAMP;
import static org.chromium.chrome.browser.tabmodel.TabList.INVALID_TAB_INDEX;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tab.state.ArchivePersistedTabData;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
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
    @FunctionalInterface
    /** Provides the current timestamp. */
    public interface Clock {
        long currentTimeMillis();
    }

    private final TabModel mArchivedTabModel;
    private final TabCreator mArchivedTabCreator;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final TabWindowManager mTabWindowManager;
    private final TabArchiveSettings mTabArchiveSettings;
    private final Clock mClock;

    private boolean mDeclutterInitCalled;

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

    /** Initialize the archiving process by observing TabWindowManager for new TabModelSelectors. */
    public void initDeclutter() {
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
        assert mDeclutterInitCalled;

        // Trigger archival of inactive tabs for the current selectors.
        for (int i = 0; i < mTabWindowManager.getMaxSimultaneousSelectors(); i++) {
            TabModelSelector selector = mTabWindowManager.getTabModelSelectorById(i);
            if (selector == null) continue;
            onTabModelSelectorAdded(selector);
        }

        // Trigger auto-deletion after archiving tabs.
        deleteEligibleArchivedTabs();
    }

    /** Delete eligible archived tabs. */
    public void deleteEligibleArchivedTabs() {
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
                            mArchivedTabModel.closeTab(tab);
                        }
                    });
        }
    }

    /**
     * Rescue archived tabs, moving them back to the regular TabModel. This is done when the feature
     * is disabled, but there are still archived tabs.
     */
    public void rescueArchivedTabs(TabCreator regularTabCreator) {
        while (mArchivedTabModel.getCount() > 0) {
            Tab tab = mArchivedTabModel.getTabAt(0);
            unarchiveAndRestoreTab(regularTabCreator, tab);
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
        TabState tabState = TabStateExtractor.from(tab);
        // Scrub the parent id prior to archiving to avoid ordering issues within the tab model.
        tabState.parentId = Tab.INVALID_TAB_ID;
        Tab newTab = mArchivedTabCreator.createFrozenTab(tabState, tab.getId(), INVALID_TAB_INDEX);
        tabModel.closeTab(tab);

        ArchivePersistedTabData.from(
                newTab,
                (archivePersistedTabData) -> {
                    // Persisted tab data requires a true supplier before saving to disk.
                    archivePersistedTabData.registerIsTabSaveEnabledSupplier(
                            new ObservableSupplierImpl<>(true));
                    archivePersistedTabData.setArchivedTimeMs(mClock.currentTimeMillis());
                });

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
        TabState tabState = TabStateExtractor.from(tab);
        // Scrub the parent id prior to restoration to avoid ordering issues within the tab model.
        tabState.parentId = Tab.INVALID_TAB_ID;
        mArchivedTabModel.removeTab(tab);
        mAsyncTabParamsManager.add(tab.getId(), new TabReparentingParams(tab, null));
        tabCreator.createFrozenTab(tabState, tab.getId(), INVALID_TAB_INDEX);
    }

    // TabWindowManager.Observer implementation.

    @Override
    public void onTabModelSelectorAdded(TabModelSelector selector) {
        if (!mTabArchiveSettings.getArchiveEnabled()) return;

        TabModelUtils.runOnTabStateInitialized(
                selector, this::archiveEligibleTabsFromTabModelSelector);
    }

    // Private functions.

    private void archiveEligibleTabsFromTabModelSelector(TabModelSelector selector) {
        TabModel model = selector.getModel(/* isIncognito= */ false);
        int activeTabId = TabModelUtils.getCurrentTabId(model);
        for (int i = 0; i < model.getCount(); ) {
            Tab tab = model.getTabAt(i);
            if (activeTabId != tab.getId() && isTabEligibleForArchive(tab)) {
                archiveAndRemoveTab(model, tab);
            } else {
                i++;
            }
        }
    }

    private boolean isTabEligibleForArchive(Tab tab) {
        // Explicitly prevent grouped tabs from getting archived.
        if (tab.getTabGroupId() != null) return false;
        TabState tabState = TabStateExtractor.from(tab);
        if (tabState.contentsState == null) return false;

        return isTimestampWithinTargetHours(
                tab.getTimestampMillis(), mTabArchiveSettings.getArchiveTimeDeltaHours());
    }

    private boolean isArchivedTabEligibleForDeletion(
            ArchivePersistedTabData archivePersistedTabData) {
        if (archivePersistedTabData == null) return false;

        return isTimestampWithinTargetHours(
                archivePersistedTabData.getArchivedTimeMs(),
                mTabArchiveSettings.getAutoDeleteTimeDeltaHours());
    }

    private boolean isTimestampWithinTargetHours(long timestampMillis, int targetHours) {
        if (timestampMillis == INVALID_TIMESTAMP) return false;

        long ageHours = TimeUnit.MILLISECONDS.toHours(mClock.currentTimeMillis() - timestampMillis);
        return ageHours >= targetHours;
    }
}
