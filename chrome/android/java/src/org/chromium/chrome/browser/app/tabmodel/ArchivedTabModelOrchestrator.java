// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tab.TabArchiverImpl;
import org.chromium.chrome.browser.tab.tab_restore.HistoricalTabModelObserver;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.ArchivedTabCountSupplier;
import org.chromium.chrome.browser.tabmodel.ArchivedTabCreator;
import org.chromium.chrome.browser.tabmodel.ArchivedTabModelSelectorHolder;
import org.chromium.chrome.browser.tabmodel.ArchivedTabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/**
 * Glue-level class that manages the lifetime of {@link TabPersistentStore} and {@link
 * TabModelSelectorImpl} for archived tabs. Uses the base logic from TabModelOrchestrator to wire
 * the store and selector. This class is tied to a profile, and will be cleaned up when the profile
 * goes away.
 */
@NullMarked
public class ArchivedTabModelOrchestrator extends TabModelOrchestrator implements Destroyable {
    public static final String ARCHIVED_TAB_SELECTOR_UNIQUE_TAG = "archived";

    /** Observer for the ArchivedTabModelOrchestrator class. */
    public interface Observer {
        /**
         * Called when the archived {@link TabModel} is created.
         *
         * @param archivedTabModel The {@link TabModel} that was created.
         */
        void onTabModelCreated(TabModel archivedTabModel);
    }

    private static @Nullable ProfileKeyedMap<ArchivedTabModelOrchestrator> sProfileMap;
    private static @Nullable ArchivedTabModelOrchestrator sInstanceForTesting;

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

    private final TabArchiveSettings.Observer mTabArchiveSettingsObserver =
            new TabArchiveSettings.Observer() {
                @Override
                public void onSettingChanged() {
                    // In the case where CTA was destroyed in the background, skip rescuing
                    // archived tabs. It will be picked up when CTA is re-created, and the tab
                    // model orchestrator is re-registered.
                    assertNativeReady();
                    if (!mTabArchiveSettings.getArchiveEnabled()
                            && !mActivityTabModelOrchestrators.isEmpty()) {
                        rescueArchivedTabs(mActivityTabModelOrchestrators.get(0));
                    }
                }
            };

    private final TabArchiver.Observer mTabArchiverObserver =
            new TabArchiver.Observer() {
                @Override
                public void onDeclutterPassCompleted() {
                    if (!ChromeFeatureList.sTabModelInitFixes.isEnabled()) {
                        saveState();
                    }
                }

                @Override
                public void onArchivePersistedTabDataCreated() {
                    assumeNonNull(mTabArchiver);
                    if (mTriggerAutodeleteAfterDataCreated) {
                        mTabArchiver.doAutodeletePass();
                        mTriggerAutodeleteAfterDataCreated = false;
                    }
                }
            };

    private final Profile mProfile;
    // TODO(crbug.com/331689555): Figure out how to do synchronization. Only one instance should
    // really be using this at a time and it makes things like undo messy if it is supported in
    // multiple places simultaneously.
    private final TabCreatorManager mArchivedTabCreatorManager;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final TabWindowManager mTabWindowManager;
    // The set of {@link TabModelOrchestrators}  which have registered themselves as active for
    // declutter.
    private final List<TabbedModeTabModelOrchestrator> mActivityTabModelOrchestrators =
            new ArrayList<>();

    private @MonotonicNonNull WindowAndroid mWindow;
    private @MonotonicNonNull TabArchiver mTabArchiver;
    private @MonotonicNonNull TabArchiveSettings mTabArchiveSettings;
    private @MonotonicNonNull TabCreator mArchivedTabCreator;
    private boolean mInitCalled;
    private boolean mNativeLibraryReadyCalled;
    private boolean mLoadStateCalled;
    private boolean mRestoreTabsCalled;
    private boolean mRescueTabsCalled;
    private boolean mRescueTabGroupsCalled;
    private CallbackController mCallbackController = new CallbackController();
    private @Nullable HistoricalTabModelObserver mHistoricalTabModelObserver;
    private boolean mTriggerAutodeleteAfterDataCreated;
    private @Nullable TabGroupSyncService mTabGroupSyncService;
    private ArchivedTabCountSupplier mArchivedTabCountSupplier = new ArchivedTabCountSupplier();

    /**
     * Returns the ArchivedTabModelOrchestrator that corresponds to the given profile. Must be
     * called after native initialization
     *
     * @param profile The {@link Profile} to build the ArchivedTabModelOrchestrator with.
     * @return The corresponding {@link ArchivedTabModelOrchestrator}.
     */
    public static ArchivedTabModelOrchestrator getForProfile(Profile profile) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }

        if (sProfileMap == null) {
            ThreadUtils.assertOnUiThread();
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
            ApplicationStatus.registerApplicationStateListener(sApplicationStateListener);
        }

        return sProfileMap.getForProfile(
                profile, (originalProfile) -> new ArchivedTabModelOrchestrator(originalProfile));
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

    private ArchivedTabModelOrchestrator(Profile profile) {
        mProfile = profile;
        mArchivedTabCreatorManager =
                new TabCreatorManager() {
                    @Override
                    public TabCreator getTabCreator(boolean incognito) {
                        assert !incognito : "Archived tab model does not support incognito.";
                        assert mArchivedTabCreator != null;
                        return mArchivedTabCreator;
                    }
                };
        mAsyncTabParamsManager = AsyncTabParamsManagerSingleton.getInstance();
        mTabWindowManager = TabWindowManagerSingleton.getInstance();
        // TODO(crbug.com/359875260): This is a temporary solution to get the
        // ArchivedTabModelSelector from within the tabmodel package.
        ArchivedTabModelSelectorHolder.setInstanceFn(
                (profileQuery) -> {
                    ArchivedTabModelOrchestrator archivedTabModelOrchestrator =
                            getForProfile(profileQuery);
                    return archivedTabModelOrchestrator.mTabModelSelector;
                });
    }

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        if (mWindow != null) {
            mWindow.destroy();
            mWindow = null;
        }

        if (mTabArchiveSettings != null) {
            mTabArchiveSettings.removeObserver(mTabArchiveSettingsObserver);
            mTabArchiveSettings.destroy();
            mTabArchiveSettings = null;
        }

        if (mTabArchiver != null) {
            mTabArchiver.removeObserver(mTabArchiverObserver);
        }

        // Null out TabWindowManager's reference so TabState isn't cleared.
        mTabWindowManager.setArchivedTabModelSelector(null);

        if (mHistoricalTabModelObserver != null) {
            mHistoricalTabModelObserver.destroy();
            mHistoricalTabModelObserver = null;
        }

        if (mTabArchiver != null) {
            mTabArchiver.destroy();
            mTabArchiver = null;
        }

        if (mArchivedTabCountSupplier != null) {
            mArchivedTabCountSupplier.destroy();
            mArchivedTabCountSupplier = null;
        }

        super.destroy();
    }

    /** Adds an observer. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Removes an observer. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Registers an orchestrator as active for declutter. Runs a declutter pass, and schedules a
     * recurring pass for long-running chrome instances (e.g. chrome is open but in the background
     * for weeks).
     */
    public void registerTabModelOrchestrator(TabbedModeTabModelOrchestrator orchestrator) {
        mActivityTabModelOrchestrators.add(orchestrator);
        assertNativeReady();
        if (mTabArchiveSettings.getArchiveEnabled()) {
            // There is some delay while the local tab group sync databases synchronizes with the
            // sync service on startup. Archiving is done on startup, although it's loaded as a
            // deferred task which is only started after the regular tab model is already
            // initialized. This is done to prevent any noticeable lag when archiving tabs. There
            // is the chance that tab groups are archived while in the midst of being deleted. This
            // is much more of an edge case than adding a set delay at startup, and is already
            // handled by observer events in the relevant UI which mirror the behavior in the tab
            // groups pane.
            doDeclutterPassAndScheduleNext(new WeakReference<>(orchestrator));
        } else {
            rescueArchivedTabs(orchestrator);
        }

        // If the flag is turned off, clear all {@link SavedTabGroup}s of possible archived status.
        if (!ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()) {
            rescueArchivedTabGroups();
        }
    }

    /** Unregisters an orchestrator when it's destroyed. */
    public void unregisterTabModelOrchestrator(TabbedModeTabModelOrchestrator orchestrator) {
        mActivityTabModelOrchestrators.remove(orchestrator);
    }

    /** Returns a supplier for the archive tab count. */
    public ObservableSupplier<Integer> getTabCountSupplier() {
        return mArchivedTabCountSupplier;
    }

    public @Nullable TabModel getTabModel() {
        // If the tab model selector isn't ready yet, then return a placeholder supplier.
        if (getTabModelSelector() == null) return null;
        return getTabModelSelector().getModel(/* incognito= */ false);
    }

    /** Returns whether the archived tab model has been initialized. */
    public boolean isTabModelInitialized() {
        return mInitCalled;
    }

    /**
     * Creates and initializes the class and fields, this must be called in the UI thread and can be
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
    public void maybeCreateAndInitTabModels(
            TabContentManager tabContentManager, CipherFactory cipherFactory) {
        // NullAway complains about the early return. Split the method in two so the inner method
        // is the initializer.
        if (mInitCalled) return;
        maybeCreateAndInitTabModelsInternal(tabContentManager, cipherFactory);
    }

    @EnsuresNonNull({
        "mArchivedTabCreator",
        "mTabModelSelector",
        "mTabPersistencePolicy",
        "mTabPersistentStore",
        "mWindow",
    })
    private void assertCreated() {
        assert mArchivedTabCreator != null;
        assert mTabModelSelector != null;
        assert mTabPersistencePolicy != null;
        assert mTabPersistentStore != null;
        assert mWindow != null;
    }

    @EnsuresNonNull({
        "mArchivedTabCreator",
        "mTabModelSelector",
        "mTabPersistencePolicy",
        "mTabPersistentStore",
        "mWindow",
        "mTabArchiveSettings",
        "mTabArchiver",
        "mTabGroupSyncService"
    })
    private void assertNativeReady() {
        assertCreated();
        assert mTabArchiveSettings != null;
        assert mTabArchiver != null;
        assert mTabGroupSyncService != null;
    }

    private void maybeCreateAndInitTabModelsInternal(
            TabContentManager tabContentManager, CipherFactory cipherFactory) {
        ThreadUtils.assertOnUiThread();
        assert tabContentManager != null;

        Context context = ContextUtils.getApplicationContext();
        mWindow = new WindowAndroid(context, /* trackOcclusion= */ false);
        mArchivedTabCreator = new ArchivedTabCreator(mWindow);

        mTabModelSelector =
                new ArchivedTabModelSelectorImpl(
                        mProfile,
                        mArchivedTabCreatorManager,
                        () -> NextTabPolicy.LOCATIONAL,
                        mAsyncTabParamsManager);
        mTabWindowManager.setArchivedTabModelSelector(mTabModelSelector);

        mTabPersistencePolicy =
                new TabbedModeTabPersistencePolicy(
                        TabMetadataFileManager.getMetadataFileName(
                                ARCHIVED_TAB_SELECTOR_UNIQUE_TAG),
                        /* otherMetadataFileName= */ null,
                        /* mergeTabsOnStartup= */ false,
                        /* tabMergingEnabled= */ false) {

                    @Override
                    public void notifyStateLoaded(int tabCountAtStartup) {
                        // Intentional no-op.
                    }
                };
        mTabPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_ARCHIVED,
                        mTabPersistencePolicy,
                        mTabModelSelector,
                        mArchivedTabCreatorManager,
                        mTabWindowManager,
                        cipherFactory) {
                    @Override
                    protected void recordLegacyTabCountMetrics() {
                        // Intentional no-op.
                    }
                };

        wireSelectorAndStore();
        markTabModelsInitialized();

        // This will be called from a deferred task which sets up the entire class, so therefore all
        // of the methods required for proper initialization need to be called here.
        onNativeLibraryReady(tabContentManager);
        loadState(/* ignoreIncognitoFiles= */ true, /* onStandardActiveIndexRead= */ null);
        restoreTabs(/* setActiveTab= */ false);

        mInitCalled = true;

        TabModel model = mTabModelSelector.getModel(/* incognito= */ false);
        for (Observer observer : mObservers) {
            observer.onTabModelCreated(model);
        }

        mArchivedTabCountSupplier.setupInternalObservers(model, mTabGroupSyncService);

        TabGroupModelFilter regularFilter =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(/* isIncognito= */ false);
        assumeNonNull(regularFilter);
        mHistoricalTabModelObserver = new HistoricalTabModelObserver(regularFilter);
    }

    private void doDeclutterPassAndScheduleNext(
            WeakReference<TabbedModeTabModelOrchestrator> orchestratorRef) {
        // If the orcehstrator has been unregistered since the last scheduled declutter pass, return
        // immediately.
        TabbedModeTabModelOrchestrator orchestrator = orchestratorRef.get();
        if (orchestrator == null || !mActivityTabModelOrchestrators.contains(orchestrator)) {
            return;
        }
        doDeclutterPass(orchestrator);
        assertNativeReady();
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                mCallbackController.makeCancelable(
                        () -> doDeclutterPassAndScheduleNext(orchestratorRef)),
                TimeUnit.HOURS.toMillis(mTabArchiveSettings.getDeclutterIntervalTimeDeltaHours()));
    }

    /**
     * Begins the process of decluttering tabs if it hasn't been started already.
     *
     * @param regularSelector The regular {@link TabModelSelecltor} to do a declutter pass for.
     */
    public void doDeclutterPass(TabbedModeTabModelOrchestrator orchestrator) {
        ThreadUtils.assertOnUiThread();
        assertCreated();
        TabModelUtils.runOnTabStateInitialized(
                mCallbackController.makeCancelable(() -> doDeclutterPassImpl(orchestrator)),
                mTabModelSelector,
                assertNonNull(orchestrator.getTabModelSelector()));
    }

    private void doDeclutterPassImpl(TabbedModeTabModelOrchestrator orchestrator) {
        assertNativeReady();
        if (!mTabArchiveSettings.getArchiveEnabled()) return;
        pauseSaveTabList(orchestrator);

        int archiveTimeHours = mTabArchiveSettings.getArchiveTimeDeltaHours();
        if (ChromeFeatureList.sAndroidTabDeclutterArchiveAllButActiveTab.isEnabled()) {
            mTabArchiveSettings.setArchiveTimeDeltaHours(0);
        }

        mTabArchiver.addObserver(
                new TabArchiver.Observer() {
                    @Override
                    public void onDeclutterPassCompleted() {
                        if (ChromeFeatureList.sAndroidTabDeclutterArchiveAllButActiveTab
                                .isEnabled()) {
                            mTabArchiveSettings.setArchiveTimeDeltaHours(archiveTimeHours);
                        }
                        resumeSaveTabList(orchestrator);
                        mTabArchiver.removeObserver(this);
                    }
                });

        mTriggerAutodeleteAfterDataCreated = true;
        mTabArchiver.doArchivePass(assertNonNull(orchestrator.getTabModelSelector()));
    }

    /**
     * Begins the process of rescuing archived tabs if it hasn't been started already. Rescuing tabs
     * will move them from the archived tab model into the normal tab model of the context this is
     * called from.
     *
     * @param orchestrator The orchestrator to save the rescued archived tabs.
     */
    public void rescueArchivedTabs(TabbedModeTabModelOrchestrator orchestrator) {
        ThreadUtils.assertOnUiThread();
        assertCreated();
        if (mRescueTabsCalled) return;
        mRescueTabsCalled = true;
        TabModelUtils.runOnTabStateInitialized(
                mCallbackController.makeCancelable(() -> rescueArchivedTabsImpl(orchestrator)),
                mTabModelSelector,
                assertNonNull(orchestrator.getTabModelSelector()));
        rescueArchivedTabGroups();
    }

    private void rescueArchivedTabsImpl(TabbedModeTabModelOrchestrator orchestrator) {
        assertNativeReady();
        assert ChromeFeatureList.sAndroidTabDeclutterRescueKillSwitch.isEnabled();
        pauseSaveTabList(orchestrator);
        mTabArchiver.rescueArchivedTabs(
                assertNonNull(orchestrator.getTabModelSelector())
                        .getTabCreatorManager()
                        .getTabCreator(/* incognito= */ false));
        resumeSaveTabList(orchestrator);
    }

    private void rescueArchivedTabGroups() {
        if (mTabGroupSyncService == null) return;

        if (mRescueTabGroupsCalled) return; // prevents calling when already called once
        mRescueTabGroupsCalled = true;

        // Clear all {@link SavedTabGroup}s of possible archived status as the rescue operation.
        for (String syncGroupId : mTabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(syncGroupId);

            if (savedTabGroup != null && savedTabGroup.archivalTimeMs != null) {
                mTabGroupSyncService.updateArchivalStatus(syncGroupId, false);
            }
        }
    }

    public void initializeHistoricalTabModelObserver(Supplier<TabModel> regularTabModelSupplier) {
        if (mHistoricalTabModelObserver != null) {
            mHistoricalTabModelObserver.addSecondaryTabModelSupplier(regularTabModelSupplier);
        }
    }

    public void removeHistoricalTabModelObserver(Supplier<TabModel> regularTabModelSupplier) {
        if (mHistoricalTabModelObserver != null) {
            mHistoricalTabModelObserver.removeSecondaryTabModelSupplier(regularTabModelSupplier);
        }
    }

    // TabModelOrchestrator lifecycle methods.

    @Override
    public void onNativeLibraryReady(TabContentManager tabContentManager) {
        if (mNativeLibraryReadyCalled) return;
        mNativeLibraryReadyCalled = true;
        super.onNativeLibraryReady(tabContentManager);
        onNativeLibraryReadyInternal();
    }

    private void onNativeLibraryReadyInternal() {
        assertCreated();
        mTabArchiveSettings = new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        mTabArchiveSettings.addObserver(mTabArchiveSettingsObserver);
        mTabGroupSyncService = assertNonNull(TabGroupSyncServiceFactory.getForProfile(mProfile));
        TabGroupModelFilter regularFilter =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(/* isIncognito= */ false);
        assumeNonNull(regularFilter);
        mTabArchiver =
                new TabArchiverImpl(
                        regularFilter,
                        mArchivedTabCreator,
                        mTabArchiveSettings,
                        System::currentTimeMillis,
                        mTabGroupSyncService);
        mTabArchiver.addObserver(mTabArchiverObserver);
    }

    @Override
    public void loadState(
            boolean ignoreIncognitoFiles, @Nullable Callback<String> onStandardActiveIndexRead) {
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

    // Getter methods

    public TabArchiveSettings getTabArchiveSettings() {
        assertNativeReady();
        return mTabArchiveSettings;
    }

    public TabArchiver getTabArchiver() {
        assert mTabArchiver != null;
        return mTabArchiver;
    }

    // Private methods

    @RequiresNonNull("mTabPersistentStore")
    private void pauseSaveTabList(TabbedModeTabModelOrchestrator orchestrator) {
        // Temporarily disable #saveTabListAsynchronously while running a bulk operation.
        orchestrator.getTabPersistentStore().pauseSaveTabList();
        mTabPersistentStore.pauseSaveTabList();
    }

    @RequiresNonNull("mTabPersistentStore")
    private void resumeSaveTabList(TabbedModeTabModelOrchestrator orchestrator) {
        // Re-enable #saveTabListAsynchronously after running a bulk operation.
        if (ChromeFeatureList.sTabModelInitFixes.isEnabled()) {
            // This triggers saves to the backing stores. It's possible we crash/are shutdown after
            // the first and before the second. For this reason, it's critical that we resume the
            // archived side before the tabbed side. This will cause tab duplication instead of data
            // loss. Duplication will be cleaned up and handled on the next restart. While data loss
            // would not be recoverable.
            mTabPersistentStore.resumeSaveTabList();
            orchestrator.getTabPersistentStore().resumeSaveTabList();
        } else {
            orchestrator.getTabPersistentStore().resumeSaveTabList();
            mTabPersistentStore.resumeSaveTabList();
        }
    }

    // Testing-specific methods

    /** Returns the {@link TabCreator} for archived tabs. */
    public TabCreator getArchivedTabCreatorForTesting() {
        assertCreated();
        return mArchivedTabCreatorManager.getTabCreator(false);
    }

    public void resetRescueArchivedTabsForTesting() {
        mRescueTabsCalled = false;
    }

    public void resetRescueArchivedTabGroupsForTesting() {
        mRescueTabGroupsCalled = false;
    }

    public void setTabModelSelectorForTesting(TabModelSelectorBase tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    public static void setInstanceForTesting(ArchivedTabModelOrchestrator instance) {
        sInstanceForTesting = instance;
    }
}
