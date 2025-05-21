// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.content.Context;

import androidx.annotation.Nullable;
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
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Glue-level class that manages the lifetime of {@link TabPersistentStore} and {@link
 * TabModelSelectorImpl} for archived tabs. Uses the base logic from TabModelOrchestrator to wire
 * the store and selector. This class is tied to a profile, and will be cleaned up when the profile
 * goes away.
 */
public class ArchivedTabModelOrchestrator extends TabModelOrchestrator implements Destroyable {
    public static final String ARCHIVED_TAB_SELECTOR_UNIQUE_TAG = "archived";

    /** Observer for the ArchivedTabModelOrchestrator class. */
    public interface Observer {
        /**
         * Called when the archived {@link TabModel} is created.
         *
         * @param archivedTabModel The {@link TabModel} that was created.
         */
        public void onTabModelCreated(TabModel archivedTabModel);
    }

    private static ProfileKeyedMap<ArchivedTabModelOrchestrator> sProfileMap;
    private static ArchivedTabModelOrchestrator sInstanceForTesting;

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
                    if (!mTabArchiveSettings.getArchiveEnabled()
                            && mActivityTabModelOrchestrators.size() > 0) {
                        rescueArchivedTabs(mActivityTabModelOrchestrators.get(0));
                    }
                }
            };

    private final TabArchiver.Observer mTabArchiverObserver =
            new TabArchiver.Observer() {
                @Override
                public void onDeclutterPassCompleted() {
                    saveState();
                }

                @Override
                public void onArchivePersistedTabDataCreated() {
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

    private WindowAndroid mWindow;
    private TabArchiver mTabArchiver;
    private TabArchiveSettings mTabArchiveSettings;
    private TabCreator mArchivedTabCreator;
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
        if (mTabWindowManager != null) {
            mTabWindowManager.setArchivedTabModelSelector(null);
        }

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

    public TabModel getTabModel() {
        // If the tab model selector isn't ready yet, then return a placeholder supplier
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
        if (mInitCalled) return;
        ThreadUtils.assertOnUiThread();
        assert tabContentManager != null;

        Context context = ContextUtils.getApplicationContext();
        // TODO(crbug.com/331841977): Investigate removing the WindowAndroid requirement when
        // creating tabs.
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
                        TabPersistentStore.getMetadataFileName(ARCHIVED_TAB_SELECTOR_UNIQUE_TAG),
                        /* otherMetadataFileName= */ null,
                        /* mergeTabsOnStartup= */ false,
                        /* tabMergingEnabled= */ false) {

                    @Override
                    public void notifyStateLoaded(int tabCountAtStartup) {
                        // Intentional no-op.
                    }
                };
        mTabPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_ARCHIVED,
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

        mHistoricalTabModelObserver =
                new HistoricalTabModelObserver(
                        getTabModelSelector()
                                .getTabGroupModelFilterProvider()
                                .getTabGroupModelFilter(/* isIncognito= */ false));
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
        TabModelUtils.runOnTabStateInitialized(
                mCallbackController.makeCancelable(() -> doDeclutterPassImpl(orchestrator)),
                getTabModelSelector(),
                orchestrator.getTabModelSelector());
    }

    private void doDeclutterPassImpl(TabbedModeTabModelOrchestrator orchestrator) {
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
        mTabArchiver.doArchivePass(orchestrator.getTabModelSelector());
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
        if (mRescueTabsCalled) return;
        mRescueTabsCalled = true;
        TabModelUtils.runOnTabStateInitialized(
                mCallbackController.makeCancelable(() -> rescueArchivedTabsImpl(orchestrator)),
                getTabModelSelector(),
                orchestrator.getTabModelSelector());
        rescueArchivedTabGroups();
    }

    private void rescueArchivedTabsImpl(TabbedModeTabModelOrchestrator orchestrator) {
        assert ChromeFeatureList.sAndroidTabDeclutterRescueKillSwitch.isEnabled();
        pauseSaveTabList(orchestrator);
        mTabArchiver.rescueArchivedTabs(
                orchestrator
                        .getTabModelSelector()
                        .getTabCreatorManager()
                        .getTabCreator(/* incognito= */ false));
        resumeSaveTabList(orchestrator);
    }

    private void rescueArchivedTabGroups() {
        if (mTabGroupSyncService == null) return;

        if (mRescueTabGroupsCalled) return;
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

        mTabArchiveSettings = new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        mTabArchiveSettings.addObserver(mTabArchiveSettingsObserver);
        mTabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(mProfile);
        mTabArchiver =
                new TabArchiverImpl(
                        mTabModelSelector
                                .getTabGroupModelFilterProvider()
                                .getTabGroupModelFilter(/* isIncognito= */ false),
                        mArchivedTabCreator,
                        mTabArchiveSettings,
                        System::currentTimeMillis,
                        mTabGroupSyncService);
        mTabArchiver.addObserver(mTabArchiverObserver);
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

    // Getter methods

    public TabArchiveSettings getTabArchiveSettings() {
        return mTabArchiveSettings;
    }

    public TabArchiver getTabArchiver() {
        return mTabArchiver;
    }

    // Private methods

    private void pauseSaveTabList(TabbedModeTabModelOrchestrator orchestrator) {
        // Temporarily disable #saveTabListAsynchronously while running a bulk operation.
        orchestrator.getTabPersistentStore().pauseSaveTabList();
        mTabPersistentStore.pauseSaveTabList();
    }

    private void resumeSaveTabList(TabbedModeTabModelOrchestrator orchestrator) {
        // Re-enable #saveTabListAsynchronously after running a bulk operation.
        orchestrator.getTabPersistentStore().resumeSaveTabList();
        mTabPersistentStore.resumeSaveTabList();
    }

    // Testing-specific methods

    /** Returns the {@link TabCreator} for archived tabs. */
    public TabCreator getArchivedTabCreatorForTesting() {
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
