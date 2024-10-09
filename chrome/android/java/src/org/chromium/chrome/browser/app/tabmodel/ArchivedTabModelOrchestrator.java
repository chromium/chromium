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
import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tab.tab_restore.HistoricalTabModelObserver;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.ArchivedTabCreator;
import org.chromium.chrome.browser.tabmodel.ArchivedTabModelSelectorHolder;
import org.chromium.chrome.browser.tabmodel.ArchivedTabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
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
                    if (!mTabArchiveSettings.getArchiveEnabled() && mInitCalled) {
                        mTabArchiver.rescueArchivedTabs(mRegularTabCreator);
                    }
                }
            };

    private final TabArchiver.Observer mTabArchiverObserver =
            new TabArchiver.Observer() {
                @Override
                public void onDeclutterPassCompleted() {
                    saveState();
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
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>();
    private final Callback<Integer> mTabCountSupplierObserver = mTabCountSupplier::set;

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
    private CallbackController mCallbackController = new CallbackController();
    private ObservableSupplier<Integer> mUnderlyingTabCountSupplier;
    // Always refers to the tab creator of the first activity to create the
    // ArchivedTabModelOrchestrator. This should always be the create for the "primary" instance
    // of ChromeTabbedActivity.
    private TabCreator mRegularTabCreator;
    private HistoricalTabModelObserver mHistoricalTabModelObserver;

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
            mTabArchiveSettings.addObserver(mTabArchiveSettingsObserver);
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

        if (mUnderlyingTabCountSupplier != null) {
            mUnderlyingTabCountSupplier.removeObserver(mTabCountSupplierObserver);
        }

        if (mHistoricalTabModelObserver != null) {
            mHistoricalTabModelObserver.destroy();
            mHistoricalTabModelObserver = null;
        }

        if (mTabArchiver != null) {
            mTabArchiver.destroy();
            mTabArchiver = null;
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

    public ObservableSupplier<Integer> getTabCountSupplier() {
        return mTabCountSupplier;
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
            TabContentManager tabContentManager,
            TabCreator regularTabCreator,
            CipherFactory cipherFactory) {
        if (mInitCalled) return;
        ThreadUtils.assertOnUiThread();
        assert tabContentManager != null;

        Context context = ContextUtils.getApplicationContext();
        // TODO(crbug.com/331841977): Investigate removing the WindowAndroid requirement when
        // creating tabs.
        mWindow = new WindowAndroid(context);
        mArchivedTabCreator = new ArchivedTabCreator(mWindow);
        mRegularTabCreator = regularTabCreator;

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

        if (!mTabArchiveSettings.getArchiveEnabled()) {
            mTabArchiver.rescueArchivedTabs(mRegularTabCreator);
        }

        mInitCalled = true;

        TabModel model = mTabModelSelector.getModel(/* incognito= */ false);
        for (Observer observer : mObservers) {
            observer.onTabModelCreated(model);
        }

        mUnderlyingTabCountSupplier = model.getTabCountSupplier();
        mTabCountSupplier.set(mUnderlyingTabCountSupplier.get());
        mUnderlyingTabCountSupplier.addObserver(mTabCountSupplierObserver);

        mHistoricalTabModelObserver =
                new HistoricalTabModelObserver(
                        getTabModelSelector()
                                .getTabModelFilterProvider()
                                .getTabModelFilter(/* isIncognito= */ false));
    }

    /** Begins the process of decluttering tabs if it hasn't been started already. */
    public void maybeBeginDeclutter() {
        if (mDeclutterInitializationCalled) return;
        mDeclutterInitializationCalled = true;
        waitUntilSelectorInitializedAndPostTask(this::maybeBeginDeclutterImpl);
    }

    private void maybeBeginDeclutterImpl() {
        assert ChromeFeatureList.sAndroidTabDeclutter.isEnabled();
        assert mTabArchiver != null;
        mTabArchiver.initDeclutter();

        int archiveTimeHours = mTabArchiveSettings.getArchiveTimeDeltaHours();
        if (ChromeFeatureList.sAndroidTabDeclutterArchiveAllButActiveTab.isEnabled()) {
            mTabArchiveSettings.setArchiveTimeDeltaHours(0);
        }

        // TODO(crbug.com/361130234): Record timing metrics here.
        mTabArchiver.addObserver(
                new TabArchiver.Observer() {
                    @Override
                    public void onDeclutterPassCompleted() {
                        if (ChromeFeatureList.sAndroidTabDeclutterArchiveAllButActiveTab
                                .isEnabled()) {
                            mTabArchiveSettings.setArchiveTimeDeltaHours(archiveTimeHours);
                        }
                        mTabArchiver.removeObserver(this);
                    }
                });
        runDeclutterAndScheduleNext();
    }

    /**
     * Begins the process of rescuing archived tabs if it hasn't been started already. Rescuing tabs
     * will move them from the archived tab model into the normal tab model of the context this is
     * called from.
     */
    public void maybeRescueArchivedTabs() {
        if (mRescueTabsCalled) return;
        mRescueTabsCalled = true;
        waitUntilSelectorInitializedAndPostTask(this::maybeRescueArchivedTabsImpl);
    }

    private void maybeRescueArchivedTabsImpl() {
        assert ChromeFeatureList.sAndroidTabDeclutterRescueKillSwitch.isEnabled();
        mTabArchiver.rescueArchivedTabs(mRegularTabCreator);
    }

    public void initializeHistoricalTabModelObserver(Supplier<TabModel> regularTabModelSupplier) {
        mHistoricalTabModelObserver.addSecodaryTabModelSupplier(regularTabModelSupplier);
    }

    private void waitUntilSelectorInitializedAndPostTask(Runnable task) {
        TabModelUtils.runOnTabStateInitialized(
                getTabModelSelector(),
                (selector) -> ThreadUtils.postOnUiThread(mCallbackController.makeCancelable(task)));
    }

    // TabModelOrchestrator lifecycle methods.

    @Override
    public void onNativeLibraryReady(TabContentManager tabContentManager) {
        if (mNativeLibraryReadyCalled) return;
        mNativeLibraryReadyCalled = true;

        super.onNativeLibraryReady(tabContentManager);

        mTabArchiveSettings = new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        mTabArchiveSettings.addObserver(mTabArchiveSettingsObserver);
        mTabArchiver =
                new TabArchiver(
                        mTabModelSelector.getModel(false),
                        mArchivedTabCreator,
                        mAsyncTabParamsManager,
                        TabWindowManagerSingleton.getInstance(),
                        mTabArchiveSettings,
                        System::currentTimeMillis);
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

    /**
     * Schedules a declutter event to happen after a certain interval. See {@link
     * TabArchiveSettings#getDeclutterIntervalTimeDeltaHours} for details.
     */
    private void runDeclutterAndScheduleNext() {
        ThreadUtils.assertOnUiThread();
        mTabArchiver.triggerScheduledDeclutter();
        mTaskRunner.postDelayedTask(
                mCallbackController.makeCancelable(this::postDeclutterTaskToUiThread),
                TimeUnit.HOURS.toMillis(mTabArchiveSettings.getDeclutterIntervalTimeDeltaHours()));
    }

    private void postDeclutterTaskToUiThread() {
        ThreadUtils.postOnUiThread(this::runDeclutterAndScheduleNext);
    }

    // Testing-specific methods

    /** Returns the {@link TabCreator} for archived tabs. */
    public TabCreator getArchivedTabCreatorForTesting() {
        return mArchivedTabCreatorManager.getTabCreator(false);
    }

    public void resetBeginDeclutterForTesting() {
        mDeclutterInitializationCalled = false;
    }

    public void setTaskRunnerForTesting(TaskRunner taskRunner) {
        mTaskRunner = taskRunner;
    }
}
