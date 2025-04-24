// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.base.ContextUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncControllerImpl;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.HeadlessTabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.TabGroupSyncController;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Performs the same purpose as the other orchestrators, but does not currently share any interface
 * or allow polymorphism as its lifecycle is substantially different.
 */
public class HeadlessTabModelOrchestrator implements Destroyable {
    private final TabPersistentStore mTabPersistentStore;
    private final TabModelSelectorImpl mTabModelSelector;
    private final TabGroupSyncController mTabGroupSyncController;

    /**
     * @param windowId The id of the window to load tabs for.
     * @param profile The profile to scope access to.
     */
    public HeadlessTabModelOrchestrator(@WindowId int windowId, Profile profile) {
        TabPersistencePolicy policy =
                new TabbedModeTabPersistencePolicy(
                        windowId, /* mergeTabsOnStartup= */ false, /* tabMergingEnabled= */ false);
        HeadlessTabCreator tabCreator = new HeadlessTabCreator(profile);
        HeadlessTabCreator incogTabCreator = new HeadlessTabCreator(profile);
        TabCreatorManager tabCreatorManager = (incog) -> incog ? tabCreator : incogTabCreator;

        mTabModelSelector = new HeadlessTabModelSelectorImpl(profile, tabCreatorManager);
        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        mTabPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_HEADLESS,
                        policy,
                        mTabModelSelector,
                        tabCreatorManager,
                        tabWindowManager,
                        new CipherFactory());
        mTabModelSelector.selectModel(false);
        mTabPersistentStore.addObserver(
                new TabPersistentStoreObserver() {
                    @Override
                    public void onStateLoaded() {
                        mTabModelSelector.markTabStateInitialized();
                        mTabPersistentStore.removeObserver(this);
                    }
                });
        TabContentManager tabContentManager =
                new TabContentManager(
                        ContextUtils.getApplicationContext(),
                        new HeadlessBrowserControlsStateProvider(),
                        /* snapshotsEnabled= */ false,
                        mTabModelSelector::getTabById,
                        TabWindowManagerSingleton.getInstance());
        mTabModelSelector.onNativeLibraryReady(tabContentManager);
        policy.setTabContentManager(tabContentManager);

        mTabPersistentStore.onNativeLibraryReady();
        mTabPersistentStore.loadState(/* ignoreIncognitoFiles= */ false);
        mTabPersistentStore.restoreTabs(/* setActiveTab= */ false);

        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        PrefService prefs = UserPrefs.get(profile);
        Supplier<Boolean> isActive = () -> false;
        mTabGroupSyncController =
                new TabGroupSyncControllerImpl(
                        mTabModelSelector, tabGroupSyncService, prefs, isActive);
    }

    @Override
    public void destroy() {
        mTabPersistentStore.destroy();
        mTabModelSelector.destroy();
        mTabGroupSyncController.destroy();
    }

    /** Returns the owned selector that this orchestrator is managing. */
    public TabModelSelector getTabModelSelector() {
        return mTabModelSelector;
    }
}
