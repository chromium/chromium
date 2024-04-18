// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.content.Context;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.ArchivedTabCreator;
import org.chromium.chrome.browser.tabmodel.ArchivedTabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.ui.base.WindowAndroid;

/**
 * Glue-level class that manages the lifetime of {@link TabPersistentStore} and {@link
 * TabModelSelectorImpl} for archived tabs. Uses the base logic from TabModelOrchestrator to wire
 * the store and selector. This class is tied to a profile, and will be cleaned up when the profile
 * goes away.
 */
public class ArchivedTabModelOrchestrator extends TabModelOrchestrator implements Destroyable {
    public static final String ARCHIVED_TAB_SELECTOR_UNIQUE_TAG = "archived";

    private static ProfileKeyedMap<ArchivedTabModelOrchestrator> sProfileMap;

    // TODO(crbug.com/333572160): Rely on PKM destroy infra when it's working.
    private static final ApplicationStatus.ApplicationStateListener sApplicationStateListener =
            new ApplicationStateListener() {
                @Override
                public void onApplicationStateChange(@ApplicationState int newState) {
                    if (ApplicationStatus.isEveryActivityDestroyed()) {
                        // Destroy the profile map, which will also destroy all orchestrators.
                        // Null it out so if we go from 1 -> 0 -> 1 activities, #getForProfile
                        // will still work.
                        sProfileMap.destroy();
                        sProfileMap = null;
                    }
                }
            };

    private final Profile mProfile;
    // TODO(crbug.com/331689555): Figure out how to do synchronization. Only one instance should
    // really be using this at a time and it makes things like undo messy if it is supported in
    // multiple places simultaneously.
    private final TabCreatorManager mArchivedTabCreatorManager;
    private final AsyncTabParamsManager mAsyncTabParamsManager;

    private WindowAndroid mWindow;
    private TabArchiver mTabArchiver;
    private TabArchiveSettings mTabArchiveSettings;
    private TabCreator mArchivedTabCreator;
    private boolean mNativeLibraryReadyCalled;
    private boolean mLoadStateCalled;
    private boolean mRestoreTabsCalled;
    private boolean mDestroyCalled;
    private boolean mBeginDeclutterCalled;
    private boolean mRescueTabsCalled;

    /**
     * Returns the ArchivedTabModelOrchestrator that corresponds to the given profile. Must be
     * called after native initialization
     *
     * @param profile The {@link Profile} to build the ArchivedTabModelOrchestrator with.
     * @return The corresponding {@link ArchivedTabModelOrchestrator}.
     */
    public static ArchivedTabModelOrchestrator getForProfile(Profile profile) {
        if (sProfileMap == null) {
            ThreadUtils.assertOnUiThread();
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
            ApplicationStatus.registerApplicationStateListener(sApplicationStateListener);
        }
        return sProfileMap.getForProfile(profile, ArchivedTabModelOrchestrator::new);
    }

    /** Destroys the singleton profile keyed map for testing. */
    public static void destroyProfileKeyedMapForTesting() {
        sProfileMap.destroy();
        sProfileMap = null;
    }

    private ArchivedTabModelOrchestrator(Profile profile) {
        mProfile = profile;
        mArchivedTabCreatorManager =
                new TabCreatorManager() {
                    @Override
                    public TabCreator getTabCreator(boolean incognito) {
                        assert !incognito : "Archived tab model does not support incognito.";
                        return mArchivedTabCreator;
                    }
                };
        mAsyncTabParamsManager = AsyncTabParamsManagerSingleton.getInstance();
    }

    @Override
    public void destroy() {
        if (mDestroyCalled) return;

        mWindow.destroy();
        mWindow = null;

        super.destroy();
        mDestroyCalled = true;
    }

    /**
     * Creates the {@link TabModelSelector} and the {@link TabPersistentStore} if not already
     * created.
     */
    public void maybeCreateTabModels() {
        if (mArchivedTabCreator != null) return;

        Context context = ContextUtils.getApplicationContext();
        // TODO(crbug.com/331841977): Investigate removing the WindowAndroid requirement when
        // creating tabs.
        mWindow = new WindowAndroid(context);
        mArchivedTabCreator = new ArchivedTabCreator(mWindow);

        mTabModelSelector =
                new ArchivedTabModelSelectorImpl(
                        mProfile,
                        mArchivedTabCreatorManager,
                        new ChromeTabModelFilterFactory(context),
                        () -> NextTabPolicy.LOCATIONAL,
                        mAsyncTabParamsManager);

        mTabPersistencePolicy =
                new TabbedModeTabPersistencePolicy(
                        TabPersistentStore.getMetadataFileName(ARCHIVED_TAB_SELECTOR_UNIQUE_TAG),
                        /* otherMetadataFileName= */ null,
                        /* mergeTabsOnStartup= */ false,
                        /* tabMergingEnabled= */ false);
        mTabPersistentStore =
                new TabPersistentStore(
                        mTabPersistencePolicy, mTabModelSelector, mArchivedTabCreatorManager);

        wireSelectorAndStore();
        markTabModelsInitialized();
    }

    /** Begins the process of decluttering tabs if it hasn't been started already. */
    public void maybeBeginDeclutter() {
        if (mBeginDeclutterCalled) return;

        assert ChromeFeatureList.sAndroidTabDeclutter.isEnabled();
        assert mTabArchiver != null;
        mTabArchiver.beginDeclutter();

        mBeginDeclutterCalled = true;
    }

    /**
     * Begins the process of rescuing archived tabs if it hasn't been started already. Rescuing tabs
     * will move them from the archived tab model into the normal tab model of the context this is
     * called from.
     */
    public void maybeRescueArchivedTabs(TabCreator regularTabCreator) {
        if (mRescueTabsCalled) return;

        assert ChromeFeatureList.sAndroidTabDeclutterRescueKillSwitch.isEnabled();
        mTabArchiver.rescueArchivedTabs(regularTabCreator);

        mRescueTabsCalled = true;
    }

    @Override
    public void onNativeLibraryReady(TabContentManager tabContentManager) {
        if (mNativeLibraryReadyCalled) return;
        mNativeLibraryReadyCalled = true;

        super.onNativeLibraryReady(tabContentManager);

        mTabArchiveSettings = new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        mTabArchiver =
                new TabArchiver(
                        mTabModelSelector.getModel(false),
                        mArchivedTabCreator,
                        mAsyncTabParamsManager,
                        TabWindowManagerSingleton.getInstance(),
                        mTabArchiveSettings,
                        System::currentTimeMillis);
    }

    @Override
    public void loadState(
            boolean ignoreIncognitoFiles, Callback<String> onStandardActiveIndexRead) {
        if (mLoadStateCalled) return;
        mLoadStateCalled = true;
        assert ignoreIncognitoFiles : "Must ignore incognito files for archived tabs.";
        super.loadState(ignoreIncognitoFiles, onStandardActiveIndexRead);
    }

    @Override
    public void restoreTabs(boolean setActiveTab) {
        if (mRestoreTabsCalled) return;
        mRestoreTabsCalled = true;
        assert !setActiveTab : "Cannot set active tab on archived tabs.";
        super.restoreTabs(setActiveTab);
    }

    @Override
    public void cleanupInstance(int instanceId) {
        assert false : "Not reached.";
    }

    /** Returns the {@link TabCreator} for archived tabs. */
    public TabCreator getArchivedTabCreator() {
        return mArchivedTabCreatorManager.getTabCreator(false);
    }

    /** Returns the {@link TabArchiver}. */
    public TabArchiver getTabArchiver() {
        return mTabArchiver;
    }

    // Testing-specific methods

    public void resetBeginDeclutterForTesting() {
        mBeginDeclutterCalled = false;
    }

    public void resetArchiveSettingsForTesting() {
        mTabArchiveSettings.resetSettingsForTesting();
    }
}
