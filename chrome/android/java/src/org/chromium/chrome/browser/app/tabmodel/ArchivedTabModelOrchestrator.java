// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
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

import java.util.concurrent.TimeUnit;

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
    @VisibleForTesting
    static final ApplicationStatus.ApplicationStateListener sApplicationStateListener =
            new ApplicationStateListener() {
                @Override
                public void onApplicationStateChange(@ApplicationState int newState) {
                    if (ApplicationStatus.isEveryActivityDestroyed()) {
                        destroyProfileKeyedMap();
                    }
                }
            };

    private final Profile mProfile;
    // TODO(crbug.com/331689555): Figure out how to do synchronization. Only one instance should
    // really be using this at a time and it makes things like undo messy if it is supported in
    // multiple places simultaneously.
    private final TabCreatorManager mArchivedTabCreatorManager;
    private final AsyncTabParamsManager mAsyncTabParamsManager;

    private TaskRunner mTaskRunner;
    private WindowAndroid mWindow;
    private TabArchiver mTabArchiver;
    private TabArchiveSettings mTabArchiveSettings;
    private TabCreator mArchivedTabCreator;
    private boolean mInitCalled;
    private boolean mNativeLibraryReadyCalled;
    private boolean mLoadStateCalled;
    private boolean mRestoreTabsCalled;
    private boolean mDeclutterInitializationCalled;
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
        return sProfileMap.getForProfile(
                profile,
                (originalProfile) ->
                        new ArchivedTabModelOrchestrator(
                                originalProfile,
                                PostTask.createTaskRunner(TaskTraits.UI_BEST_EFFORT)));
    }

    /** Destroys the singleton profile keyed map. */
    public static void destroyProfileKeyedMap() {
        // This block can be called at times where sProfileMap may be null
        // (crbug.com/335684785). Probably not necessary now that the application
        // state listener is unregistered.
        if (sProfileMap == null) return;
        // Null it out so if we go from 1 -> 0 -> 1 activities, #getForProfile
        // will still work.
        sProfileMap.destroy();
        sProfileMap = null;
        ApplicationStatus.unregisterApplicationStateListener(sApplicationStateListener);
    }

    private ArchivedTabModelOrchestrator(Profile profile, TaskRunner taskRunner) {
        mProfile = profile;
        mTaskRunner = taskRunner;
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
        if (mWindow != null) {
            mWindow.destroy();
            mWindow = null;
        }

        super.destroy();
    }

    /**
     * Creates and initiailzes the class and fields, this must be called in the UI thread and can be
     * expensive therefore it should be called from DeferredStartupHandler. Although the lifecycle
     * methods inherited from {@link TabModelOrchestrator} are public, they aren't meant to be
     * called directly. - The {@link TabModelSelector} and the {@link TabPersistentStore} are
     * created. - The #onNativeLibraryReady method is called which plumbs these signals to the
     * TabModelSelector and TabPersistentStore. - The tab state is loaded. - The tab state is
     * restored.
     *
     * <p>Calling this multiple times (e.g. from separate chrome windows) has no effect and is safe
     * to do.
     */
    public void maybCreateAndInitTabModels(TabContentManager tabContentManager) {
        if (mInitCalled) return;
        ThreadUtils.assertOnUiThread();
        assert tabContentManager != null;

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

        // This will be called from a deferred task which sets up the entire class, so therefore all
        // of the methods required for proper initialization need to be called here.
        onNativeLibraryReady(tabContentManager);
        loadState(/* ignoreIncognitoFiles= */ true, /* onStandardActiveIndexRead= */ null);
        restoreTabs(/* setActiveTab= */ false);
        mInitCalled = true;
    }

    /** Begins the process of decluttering tabs if it hasn't been started already. */
    public void maybeBeginDeclutter() {
        if (mDeclutterInitializationCalled) return;

        assert ChromeFeatureList.sAndroidTabDeclutter.isEnabled();
        assert mTabArchiver != null;
        mTabArchiver.initDeclutter();
        runDeclutterAndScheduleNext();

        mDeclutterInitializationCalled = true;
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

    // TabModelOrchestrator lifecycle methods.

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

    // Private methods

    /**
     * Schedules a declutter event to happen after a certain interval. See {@link
     * TabArchiveSettings#getDeclutterIntervalTimeDeltaHours} for details.
     */
    private void runDeclutterAndScheduleNext() {
        mTabArchiver.triggerScheduledDeclutter();
        mTaskRunner.postDelayedTask(
                this::runDeclutterAndScheduleNext,
                TimeUnit.HOURS.toMillis(mTabArchiveSettings.getDeclutterIntervalTimeDeltaHours()));
    }

    // Testing-specific methods

    /** Returns the {@link TabCreator} for archived tabs. */
    public TabCreator getArchivedTabCreatorForTesting() {
        return mArchivedTabCreatorManager.getTabCreator(false);
    }

    public void resetBeginDeclutterForTesting() {
        mDeclutterInitializationCalled = false;
    }

    public TabArchiveSettings getArchiveSettingsForTesting() {
        return mTabArchiveSettings;
    }

    public void setTaskRunnerForTesting(TaskRunner taskRunner) {
        mTaskRunner = taskRunner;
    }
}
