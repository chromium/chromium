// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.app.tabmodel.TabPersistentStoreFactory.buildAuthoritativeStore;

import android.app.Activity;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelType;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

/**
 * Glue-level class that manages lifetime of root .tabmodel objects: {@link TabPersistentStore} and
 * {@link TabModelSelectorImpl} for custom tabs.
 */
@NullMarked
public class CustomTabsTabModelOrchestrator extends TabModelOrchestrator {
    public CustomTabsTabModelOrchestrator() {}

    private static final String CUSTOM_WINDOW_PREFIX =
            TabPersistentStoreImpl.CLIENT_TAG_CUSTOM + "_";

    /** Creates the TabModelSelector and the TabPersistentStore. */
    public void createTabModels(
            Activity activity,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            TabPersistencePolicy persistencePolicy,
            @ActivityType int activityType,
            AsyncTabParamsManager asyncTabParamsManager,
            CipherFactory cipherFactory) {
        // Instantiate TabModelSelectorImpl
        NextTabPolicySupplier nextTabPolicySupplier = () -> NextTabPolicy.LOCATIONAL;
        mTabModelSelector =
                new TabModelSelectorImpl(
                        activity,
                        /* modalDialogManager= */ null,
                        profileProviderSupplier,
                        tabCreatorManager,
                        nextTabPolicySupplier,
                        /* multiInstanceManager= */ null,
                        asyncTabParamsManager,
                        false,
                        activityType,
                        TabModelType.STANDARD,
                        false);

        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        tabWindowManager.registerCustomTabsTabModelSelector(
                activity.getTaskId(), mTabModelSelector);

        // Instantiate TabPersistentStore
        mTabPersistencePolicy = persistencePolicy;
        mTabPersistentStore =
                buildAuthoritativeStore(
                        TabPersistentStoreImpl.CLIENT_TAG_CUSTOM,
                        /* migrationManager= */ null,
                        mTabPersistencePolicy,
                        mTabModelSelector,
                        tabCreatorManager,
                        tabWindowManager,
                        getCustomTabsWindowTag(activity.getTaskId()),
                        cipherFactory,
                        /* recordLegacyTabCountMetrics= */ true);

        wireSelectorAndStore();
        markTabModelsInitialized();
    }

    @Override
    public void destroy() {
        assert mTabModelSelector != null;
        TabWindowManagerSingleton.getInstance()
                .unregisterCustomTabsTabModelSelector(mTabModelSelector);
        super.destroy();
    }

    /**
     * Get the window tag for a custom tab.
     *
     * @param taskId The task ID for the activity the orchestrator is associated with.
     */
    public static String getCustomTabsWindowTag(int taskId) {
        return CUSTOM_WINDOW_PREFIX + taskId;
    }
}
