// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.app.tabmodel.ShadowTabStoreValidator.CUSTOM_TAG;
import static org.chromium.chrome.browser.app.tabmodel.TabPersistentStoreFactory.buildAuthoritativeStore;
import static org.chromium.chrome.browser.app.tabmodel.TabPersistentStoreFactory.buildShadowStore;

import android.app.Activity;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
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

    private static final String WINDOW_PREFIX = TabPersistentStoreImpl.CLIENT_TAG_CUSTOM + "_";
    private final AccumulatingTabCreator mRegularShadowTabCreator = new AccumulatingTabCreator();
    private final AccumulatingTabCreator mIncognitoShadowTabCreator = new AccumulatingTabCreator();

    private @Nullable TabPersistentStore mShadowTabPersistentStore;

    @Override
    public void destroy() {
        if (mShadowTabPersistentStore != null) {
            mShadowTabPersistentStore.destroy();
        }

        super.destroy();
    }

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
                        false);

        // Instantiate TabPersistentStore
        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        mTabPersistencePolicy = persistencePolicy;
        mTabPersistentStore =
                buildAuthoritativeStore(
                        TabPersistentStoreImpl.CLIENT_TAG_CUSTOM,
                        mTabPersistencePolicy,
                        mTabModelSelector,
                        tabCreatorManager,
                        tabWindowManager,
                        cipherFactory,
                        /* recordLegacyTabCountMetrics= */ true);

        profileProviderSupplier.onAvailable(
                provider -> {
                    Profile profile = provider.getOriginalProfile();
                    assert profile != null;

                    String windowTag = WINDOW_PREFIX + activity.getTaskId();
                    mShadowTabPersistentStore =
                            buildShadowStore(
                                    profile,
                                    mRegularShadowTabCreator,
                                    mIncognitoShadowTabCreator,
                                    mTabModelSelector,
                                    mTabPersistencePolicy,
                                    mTabPersistentStore,
                                    windowTag,
                                    cipherFactory,
                                    CUSTOM_TAG);
                });

        wireSelectorAndStore();
        markTabModelsInitialized();
    }
}
