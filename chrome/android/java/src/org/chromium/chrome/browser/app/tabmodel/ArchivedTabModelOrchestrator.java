// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.app.tabmodel.TabPersistentStoreFactory.buildAuthoritativeStore;
import static org.chromium.chrome.browser.app.tabmodel.TabPersistentStoreFactory.buildShadowStore;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.ARCHIVED_WINDOW_TAG;

import android.content.Context;

import androidx.annotation.CheckResult;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tab.TabArchiverImpl;
import org.chromium.chrome.browser.tab.TabDestroyStatus;
import org.chromium.chrome.browser.tab.tab_restore.HistoricalTabModelObserver;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator;
import org.chromium.chrome.browser.tabmodel.ArchivedTabCountTracker;
import org.chromium.chrome.browser.tabmodel.ArchivedTabCreator;
import org.chromium.chrome.browser.tabmodel.ArchivedTabModelSelectorHolder;
import org.chromium.chrome.browser.tabmodel.ArchivedTabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabOrchestratorType;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.function.Supplier;

/**
 * Glue-level class that manages the lifetime of {@link TabPersistentStore} and {@link
 * TabModelSelectorImpl} for archived tabs. Uses the base logic from TabModelOrchestrator to wire
 * the store and selector. This class is tied to a profile, and will be cleaned up when the profile
 * goes away.
 */
@NullMarked
public class ArchivedTabModelOrchestrator extends TabModelOrchestrator {
    /** Observer for the ArchivedTabModelOrchestrator class. */
    public interface Observer {
        /**
         * Called when the archived {@link TabModel} is created.
         *
         * @param archivedTabModel The {@link TabModel} that was created.
         */
        void onTabModelCreated(TabModel archivedTabModel);
    }

    /** Reasons for acquiring a lease on {@link ArchivedTabModelOrchestrator}. */
    @IntDef({
        LeaseReason.STARTUP_DECLUTTER_PASS,
        LeaseReason.RECURRING_DECLUTTER_PASS,
        LeaseReason.RESCUE_ARCHIVED_TABS,
        LeaseReason.PERSISTENT_STORE_CLEANER,
        LeaseReason.ARCHIVED_TABS_DIALOG,
        LeaseReason.DRAG_TO_ARCHIVE,
        LeaseReason.SEARCH_OVERLAY,
        LeaseReason.FOR_TESTING
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LeaseReason {
        int STARTUP_DECLUTTER_PASS = 0;
        int RECURRING_DECLUTTER_PASS = 1;
        int RESCUE_ARCHIVED_TABS = 2;
        int PERSISTENT_STORE_CLEANER = 3;
        int ARCHIVED_TABS_DIALOG = 4;
        int DRAG_TO_ARCHIVE = 5;
        int SEARCH_OVERLAY = 6;
        int FOR_TESTING = 7;
    }

    private static final int GRACE_PERIOD_DELAY_MS = 5000;
    private static final List<TabbedModeTabModelOrchestrator> sSavedActivityOrchestrators =
            new ArrayList<>();
    private static @Nullable ProfileKeyedMap<ArchivedTabModelOrchestrator> sProfileMap;
    private static @Nullable ArchivedTabModelOrchestrator sInstanceForTesting;

    // TODO(crbug.com/333572160): Rely on PKM destroy infra when it's working.
    @VisibleForTesting
    static final ApplicationStatus.ApplicationStateListener sApplicationStateListener =
            _ -> {
                if (ApplicationStatus.isEveryActivityDestroyed()) {
                    destroyProfileKeyedMap();
                }
            };

    private final TabArchiveSettings.Observer mTabArchiveSettingsObserver =
            new TabArchiveSettings.Observer() {
                @Override
                public void onSettingChanged() {
                    // In the case where CTA was destroyed in the background, skip rescuing
                    // archived tabs. It will be picked up when CTA is re-created, and the tab
                    // model orchestrator is re-registered.
                    if (!getTabArchiveSettings().getArchiveEnabled()
                            && !sSavedActivityOrchestrators.isEmpty()) {
                        rescueArchivedTabs(sSavedActivityOrchestrators.get(0));
                    }
                }
            };

    private final TabArchiver.Observer mTabArchiverObserver =
            new TabArchiver.Observer() {
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
    private final RecordingTabCreatorManager mRecordingTabCreatorManager;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final ObserverList<Observer> mArchivedModelObservers = new ObserverList<>();
    private final TabWindowManager mTabWindowManager;

    // Currently used to perform shadow operations for an alternative storage. Not always enabled.
    private final AccumulatingTabCreator mRegularShadowTabCreator = new AccumulatingTabCreator();
    private final AccumulatingTabCreator mIncognitoShadowTabCreator = new AccumulatingTabCreator();
    private final SettableNonNullObservableSupplier<Boolean> mTabStateInitializedSupplier =
            ObservableSuppliers.createNonNull(false);

    private @MonotonicNonNull WindowAndroid mWindow;
    private @MonotonicNonNull TabArchiver mTabArchiver;
    private @MonotonicNonNull TabCreator mArchivedTabCreator;
    private @MonotonicNonNull CipherFactory mCipherFactory;
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
    private ArchivedTabCountTracker mArchivedTabCountTracker = new ArchivedTabCountTracker();
    private int mLeaseCount;
    private @Nullable Runnable mGracePeriodTeardownRunnable;
    private @Nullable CallbackController mTeardownCallbackController;
    private boolean mIsDestroyed;

    /** Returns whether the given profile is valid for accessing the orchestrator. */
    @Contract("null -> false")
    private static boolean isValidProfile(@Nullable Profile profile) {
        return profile != null
                && profile.isNativeInitialized()
                && !profile.shutdownStarted()
                && !profile.isOffTheRecord();
    }

    @Nullable TabContentManager getTabContentManager() {
        TabContentManager fallbackManager = null;
        for (TabbedModeTabModelOrchestrator orchestrator : sSavedActivityOrchestrators) {
            TabContentManager manager = orchestrator.getTabContentManager();
            if (manager == null || manager.isDestroyed()) continue;
            if (Objects.equals(orchestrator.getOriginalProfile(), mProfile)) {
                return manager;
            }
            if (fallbackManager == null) {
                fallbackManager = manager;
            }
        }
        return fallbackManager;
    }

    /**
     * Acquires a lease on the ArchivedTabModelOrchestrator for the given profile. If the
     * orchestrator is not currently instantiated, it will be created and initialized.
     *
     * @param profile The {@link Profile} to acquire a lease for.
     * @param reason The reason describing the lease consumer.
     * @return A {@link Destroyable} token that decrements the lease count upon destruction.
     */
    @CheckResult
    public static @Nullable Destroyable acquireLease(
            @Nullable Profile profile, @LeaseReason int reason) {
        ThreadUtils.assertOnUiThread();
        if (sInstanceForTesting != null) {
            return sInstanceForTesting.acquireLease(reason);
        }
        if (!isValidProfile(profile)) {
            return null;
        }
        ArchivedTabModelOrchestrator orchestrator = getForProfile(profile);
        if (!orchestrator.areTabModelsInitialized()) {
            TabContentManager tabContentManager = orchestrator.getTabContentManager();
            if (tabContentManager != null) {
                orchestrator.maybeCreateAndInitTabModels(tabContentManager, new CipherFactory());
            }
        }
        return orchestrator.acquireLease(reason);
    }

    /**
     * Synchronously returns whether the orchestrator is currently resident in memory.
     *
     * @param profile The {@link Profile} to check.
     * @return Whether the orchestrator is instantiated for the given profile.
     */
    public static boolean isInstantiatedForProfile(@Nullable Profile profile) {
        return getIfInstantiatedForProfile(profile) != null;
    }

    /**
     * Synchronously returns the orchestrator if resident in memory, or null otherwise.
     *
     * @param profile The {@link Profile} to check.
     * @return The orchestrator instance if instantiated, or null.
     */
    public static @Nullable ArchivedTabModelOrchestrator getIfInstantiatedForProfile(
            @Nullable Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        if (!isValidProfile(profile) || sProfileMap == null) {
            return null;
        }
        return sProfileMap.getForProfile(profile);
    }

    /**
     * Returns the ArchivedTabModelOrchestrator that corresponds to the given profile. Must be
     * called after native initialization
     *
     * @param profile The {@link Profile} to build the ArchivedTabModelOrchestrator with.
     * @return The corresponding {@link ArchivedTabModelOrchestrator}.
     */
    public static ArchivedTabModelOrchestrator getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        assert profile != null : "Profile cannot be null.";
        assert profile.isNativeInitialized() : "Profile is not native initialized.";
        assert !profile.shutdownStarted() : "Profile shutdown already started.";
        if (!isValidProfile(profile)) {
            throw new IllegalStateException(
                    "Attempting to access ArchivedTabModelOrchestrator with invalid profile");
        }

        if (sProfileMap == null) {
            sProfileMap =
                    new ProfileKeyedMap<>(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL,
                            ArchivedTabModelOrchestrator::destroy);
            ApplicationStatus.registerApplicationStateListener(sApplicationStateListener);
        }

        return sProfileMap.getForProfile(profile, ArchivedTabModelOrchestrator::new);
    }

    /** Destroys the singleton profile keyed map. */
    public static void destroyProfileKeyedMap() {
        sSavedActivityOrchestrators.clear();
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

    @VisibleForTesting
    ArchivedTabModelOrchestrator(Profile profile) {
        mProfile = profile;
        mArchivedTabCreatorManager =
                (boolean incognito) -> {
                    assert !incognito : "Archived tab model does not support incognito.";
                    assert mArchivedTabCreator != null;
                    return mArchivedTabCreator;
                };
        mRecordingTabCreatorManager = new RecordingTabCreatorManager(mArchivedTabCreatorManager);
        mAsyncTabParamsManager = AsyncTabParamsManagerSingleton.getInstance();
        mTabWindowManager = TabWindowManagerSingleton.getInstance();
        // TODO(crbug.com/359875260): This is a temporary solution to get the
        // ArchivedTabModelSelector from within the tabmodel package.
        ArchivedTabModelSelectorHolder.setInstanceFn(
                (profileQuery) -> {
                    ArchivedTabModelOrchestrator archivedTabModelOrchestrator =
                            getIfInstantiatedForProfile(profileQuery);
                    return archivedTabModelOrchestrator == null
                            ? null
                            : archivedTabModelOrchestrator.mTabModelSelector;
                });
    }

    @SuppressWarnings("NullAway")
    @Override
    public @TabDestroyStatus int destroy() {
        if (mIsDestroyed) return TabDestroyStatus.NO_SHUTDOWN;
        mIsDestroyed = true;

        mArchivedModelObservers.clear();

        if (mTeardownCallbackController != null) {
            mTeardownCallbackController.destroy();
            mTeardownCallbackController = null;
            mGracePeriodTeardownRunnable = null;
        }

        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        if (mWindow != null) {
            mWindow.destroy();
            mWindow = null;
        }

        if (mNativeLibraryReadyCalled) {
            getTabArchiveSettings().removeObserver(mTabArchiveSettingsObserver);
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

        if (mArchivedTabCountTracker != null) {
            mArchivedTabCountTracker.destroy();
            mArchivedTabCountTracker = null;
        }

        mTabStateInitializedSupplier.destroy();

        return super.destroy();
    }

    @Override
    public void saveState() {
        if (mIsDestroyed) return;
        super.saveState();
    }

    /** Returns whether the tab state has finished loading from disk. */
    public boolean isTabStateInitialized() {
        return mTabStateInitializedSupplier.get();
    }

    /** Returns a supplier for whether the tab state is initialized. */
    public NonNullObservableSupplier<Boolean> getTabStateInitializedSupplier() {
        return mTabStateInitializedSupplier;
    }

    /**
     * Runs a runnable once tab state is initialized, executing synchronously if already
     * initialized.
     *
     * @param runnable The runnable to execute.
     */
    public void runOnTabStateInitialized(Runnable runnable) {
        if (isTabStateInitialized()) {
            runnable.run();
            return;
        }
        Callback<Boolean> observer =
                new Callback<Boolean>() {
                    @Override
                    public void onResult(Boolean initialized) {
                        if (initialized) {
                            mTabStateInitializedSupplier.removeObserver(this);
                            runnable.run();
                        }
                    }
                };
        mTabStateInitializedSupplier.addSyncObserver(observer);
    }

    /** Acquires a lease on this orchestrator instance. */
    @CheckResult
    public Destroyable acquireLease(@LeaseReason int reason) {
        ThreadUtils.assertOnUiThread();
        assert !mIsDestroyed : "Cannot acquire lease on destroyed orchestrator.";
        if (mIsDestroyed || !ChromeFeatureList.sArchivedTabsTeardown.isEnabled()) {
            return () -> {};
        }

        if (mTeardownCallbackController != null) {
            mTeardownCallbackController.destroy();
            mTeardownCallbackController = null;
            mGracePeriodTeardownRunnable = null;
        }

        mLeaseCount++;
        boolean[] released = new boolean[] {false};
        return () -> {
            ThreadUtils.assertOnUiThread();
            if (!released[0]) {
                released[0] = true;
                releaseLeaseInternal(reason);
            }
        };
    }

    private void releaseLeaseInternal(@LeaseReason int reason) {
        ThreadUtils.assertOnUiThread();
        assert mLeaseCount > 0 : "Lease count underflow for reason: " + reason;
        mLeaseCount--;
        if (mLeaseCount == 0
                && !mIsDestroyed
                && ChromeFeatureList.sArchivedTabsTeardown.isEnabled()) {
            scheduleGracePeriodTeardown();
        }
    }

    private void scheduleGracePeriodTeardown() {
        if (mGracePeriodTeardownRunnable != null) return;
        CallbackController callbackController = new CallbackController();
        mTeardownCallbackController = callbackController;
        mGracePeriodTeardownRunnable =
                callbackController.makeCancelable(
                        () -> {
                            mGracePeriodTeardownRunnable = null;
                            mTeardownCallbackController = null;
                            if (mLeaseCount == 0 && !mIsDestroyed) {
                                performTeardown();
                            }
                        });
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT, mGracePeriodTeardownRunnable, GRACE_PERIOD_DELAY_MS);
    }

    private void performTeardown() {
        ThreadUtils.assertOnUiThread();
        if (mIsDestroyed || !ChromeFeatureList.sArchivedTabsTeardown.isEnabled()) return;

        // CRITICAL DATA LOSS PREVENTION:
        // Only save state if tab state finished loading completely from disk.
        // If teardown happens mid-restore, the on-disk state is already the authoritative full
        // state. Saving a partially restored state would overwrite and truncate the disk file!
        if (mTabPersistentStore != null && areTabModelsInitialized() && isTabStateInitialized()) {
            saveState();
        }

        // Evict from ProfileKeyedMap so subsequent acquisitions create a fresh instance,
        // and destroy all child components.
        if (sProfileMap != null) {
            ArchivedTabModelOrchestrator orchestrator = sProfileMap.removeForProfile(mProfile);
            if (orchestrator != null) {
                orchestrator.destroy();
            }
        }
        destroy();
    }

    /** Adds an observer. */
    public void addObserver(Observer observer) {
        mArchivedModelObservers.addObserver(observer);
    }

    /** Removes an observer. */
    public void removeObserver(Observer observer) {
        mArchivedModelObservers.removeObserver(observer);
    }

    /**
     * Registers an orchestrator as active for declutter. Runs a declutter pass, and schedules a
     * recurring pass for long-running chrome instances (e.g. chrome is open but in the background
     * for weeks).
     */
    public void registerTabModelOrchestrator(TabbedModeTabModelOrchestrator orchestrator) {
        if (!sSavedActivityOrchestrators.contains(orchestrator)) {
            sSavedActivityOrchestrators.add(orchestrator);
        }
        assertNativeReady();
        if (getTabArchiveSettings().getArchiveEnabled()) {
            doDeclutterPass(orchestrator);
        } else {
            rescueArchivedTabs(orchestrator);
        }
    }

    /** Unregisters an orchestrator when it's destroyed. */
    public static void unregisterTabModelOrchestrator(TabbedModeTabModelOrchestrator orchestrator) {
        sSavedActivityOrchestrators.remove(orchestrator);
    }

    public static boolean isOrchestratorRegistered(TabbedModeTabModelOrchestrator orchestrator) {
        return sSavedActivityOrchestrators.contains(orchestrator);
    }

    /** Returns a supplier for the archive tab count. */
    public NonNullObservableSupplier<Integer> getTabCountSupplier() {
        return getTabArchiveSettings().getArchivedTabCountSupplier();
    }

    public @Nullable TabModel getTabModel() {
        // If the tab model selector isn't ready yet, then return a placeholder supplier.
        if (getTabModelSelector() == null) return null;
        return getTabModelSelector().getModel(/* incognito= */ false);
    }

    /** Returns whether the orchestrator has been destroyed. */
    public boolean isDestroyed() {
        return mIsDestroyed;
    }

    /** Returns the {@link Profile} associated with this orchestrator. */
    public Profile getProfile() {
        return mProfile;
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
        "mCipherFactory",
    })
    private void assertCreated() {
        assert mArchivedTabCreator != null;
        assert mTabModelSelector != null;
        assert mTabPersistencePolicy != null;
        assert mTabPersistentStore != null;
        assert mWindow != null;
        assert mCipherFactory != null;
    }

    @EnsuresNonNull({
        "mArchivedTabCreator",
        "mTabModelSelector",
        "mTabPersistencePolicy",
        "mTabPersistentStore",
        "mWindow",
        "mTabArchiver",
        "mTabGroupSyncService"
    })
    private void assertNativeReady() {
        assertCreated();
        assert mTabArchiver != null;
        assert mTabGroupSyncService != null;
    }

    private void maybeCreateAndInitTabModelsInternal(
            TabContentManager tabContentManager, CipherFactory cipherFactory) {
        mInitCalled = true;
        ThreadUtils.assertOnUiThread();
        assert tabContentManager != null;

        Context context = ContextUtils.getApplicationContext();
        mWindow = new WindowAndroid(context, /* occlusionTrackingAllowed= */ false);
        mArchivedTabCreator = new ArchivedTabCreator(mWindow);
        mCipherFactory = cipherFactory;

        mTabModelSelector =
                new ArchivedTabModelSelectorImpl(
                        mProfile,
                        mArchivedTabCreatorManager,
                        () -> NextTabPolicy.LOCATIONAL,
                        mAsyncTabParamsManager);
        mTabWindowManager.setArchivedTabModelSelector(mTabModelSelector);

        mTabPersistencePolicy =
                new TabbedModeTabPersistencePolicy(
                        TabMetadataFileManager.getMetadataFileName(ARCHIVED_WINDOW_TAG),
                        /* otherWindowTag= */ null,
                        /* mergeTabsOnStartup= */ false,
                        /* tabMergingEnabled= */ false,
                        ObservableSuppliers.createNonNull(false)) {

                    @Override
                    public void notifyStateLoaded(int tabCountAtStartup) {
                        // Intentional no-op.
                    }
                };

        mMigrationManager = new PersistentStoreMigrationManagerImpl(ARCHIVED_WINDOW_TAG);
        mTabPersistentStore =
                buildAuthoritativeStore(
                        TabOrchestratorType.ARCHIVED,
                        mMigrationManager,
                        mTabPersistencePolicy,
                        mTabModelSelector,
                        mRecordingTabCreatorManager,
                        mTabWindowManager,
                        ARCHIVED_WINDOW_TAG,
                        cipherFactory,
                        /* recordLegacyTabCountMetrics= */ false,
                        /* isFromRecreating= */ false);

        wireSelectorAndStore();
        markTabModelsInitialized();

        // This will be called from a deferred task which sets up the entire class, so therefore all
        // of the methods required for proper initialization need to be called here.
        onNativeLibraryReady(tabContentManager);
        loadState(
                /* ignoreIncognitoFiles= */ true,
                /* ignoreRegularFiles= */ false,
                /* onStandardActiveIndexRead= */ null);
        restoreTabs(/* setActiveTab= */ false);

        TabModel model = mTabModelSelector.getModel(/* incognito= */ false);
        TabModelUtils.runOnTabStateInitialized(
                mTabModelSelector,
                mCallbackController.makeCancelable(
                        (selector) -> {
                            mTabStateInitializedSupplier.set(true);
                            mArchivedTabCountTracker.setupInternalObservers(
                                    model, mTabGroupSyncService);
                            // Resolve TabArchiveSettings at call time rather than binding a
                            // method reference; the singleton may be replaced (and the old
                            // instance destroyed) while this orchestrator is still alive.
                            mArchivedTabCountTracker
                                    .getSupplier()
                                    .addSyncObserverAndPostIfNonNull(
                                            (count) ->
                                                    getTabArchiveSettings()
                                                            .setArchivedTabCount(count));
                        }));

        for (Observer observer : mArchivedModelObservers) {
            observer.onTabModelCreated(model);
        }

        mHistoricalTabModelObserver = new HistoricalTabModelObserver(model);
        for (TabbedModeTabModelOrchestrator activityOrchestrator : sSavedActivityOrchestrators) {
            Profile activityProfile = activityOrchestrator.getOriginalProfile();
            Supplier<@Nullable TabModel> supplier =
                    activityOrchestrator.getArchivedHistoricalObserverSupplier();
            if ((activityProfile == null || Objects.equals(activityProfile, mProfile))
                    && supplier != null) {
                initializeHistoricalTabModelObserver(supplier);
            }
        }
    }

    /**
     * Begins the process of decluttering tabs if it hasn't been started already.
     *
     * @param orchestrator Contains the regular {@link TabModelSelector} to do a declutter pass for.
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
        if (!getTabArchiveSettings().getArchiveEnabled()) {
            orchestrator.onDeclutterPassCompleted();
            return;
        }
        pauseSaveTabList(orchestrator);

        mTabArchiver.addObserver(
                new TabArchiver.Observer() {
                    @Override
                    public void onDeclutterPassCompleted() {
                        resumeSaveTabList(orchestrator);
                        mTabArchiver.removeObserver(this);
                        orchestrator.onDeclutterPassCompleted();
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
        if (mRescueTabsCalled) {
            orchestrator.onRescueArchivedTabsCompleted();
            return;
        }
        assertCreated();
        mRescueTabsCalled = true;
        TabModelUtils.runOnTabStateInitialized(
                mCallbackController.makeCancelable(() -> rescueArchivedTabsImpl(orchestrator)),
                mTabModelSelector,
                assertNonNull(orchestrator.getTabModelSelector()));
        rescueArchivedTabGroups();
    }

    private void rescueArchivedTabsImpl(TabbedModeTabModelOrchestrator orchestrator) {
        assertNativeReady();
        pauseSaveTabList(orchestrator);
        mTabArchiver.rescueArchivedTabs(
                assertNonNull(orchestrator.getTabModelSelector())
                        .getTabCreatorManager()
                        .getTabCreator(/* incognito= */ false));
        resumeSaveTabList(orchestrator);
        orchestrator.onRescueArchivedTabsCompleted();
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

    public void initializeHistoricalTabModelObserver(
            Supplier<@Nullable TabModel> regularTabModelSupplier) {
        if (mHistoricalTabModelObserver != null) {
            mHistoricalTabModelObserver.addSecondaryTabModelSupplier(regularTabModelSupplier);
        }
    }

    public void removeHistoricalTabModelObserver(
            Supplier<@Nullable TabModel> regularTabModelSupplier) {
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

        assertCreated();
        if (!mTabPersistentStoreDestroyedEarly) {
            mShadowTabPersistentStore =
                    buildShadowStore(
                            mMigrationManager,
                            mRegularShadowTabCreator,
                            mIncognitoShadowTabCreator,
                            mTabModelSelector,
                            mRecordingTabCreatorManager,
                            mTabPersistencePolicy,
                            mTabPersistentStore,
                            ARCHIVED_WINDOW_TAG,
                            mCipherFactory,
                            TabOrchestratorType.ARCHIVED,
                            /* isNonOtrOnly= */ true,
                            /* isFromRecreating= */ false);
            if (mShadowTabPersistentStore != null) {
                mShadowTabPersistentStore.onNativeLibraryReady();
            }
            markStoresInitialized();
        }

        TabArchiveSettings archiveSettings = getTabArchiveSettings();
        archiveSettings.addObserver(mTabArchiveSettingsObserver);
        mTabGroupSyncService = assertNonNull(TabGroupSyncServiceFactory.getForProfile(mProfile));
        TabModel regularTabModel = mTabModelSelector.getModel(/* incognito= */ false);

        mTabArchiver =
                new TabArchiverImpl(
                        regularTabModel,
                        mArchivedTabCreator,
                        archiveSettings,
                        System::currentTimeMillis,
                        mTabGroupSyncService);
        mTabArchiver.addObserver(mTabArchiverObserver);
    }

    @Override
    public void loadState(
            boolean ignoreIncognitoFiles,
            boolean ignoreRegularFiles,
            @Nullable Callback<String> onStandardActiveIndexRead) {
        if (mLoadStateCalled) return;
        mLoadStateCalled = true;
        assert ignoreIncognitoFiles : "Must ignore incognito files for archived tabs.";
        super.loadState(ignoreIncognitoFiles, ignoreRegularFiles, onStandardActiveIndexRead);
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
        return TabArchiveSettings.getInstance();
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
        // This triggers saves to the backing stores. It's possible we crash/are shutdown after
        // the first and before the second. For this reason, it's critical that we resume the
        // archived side before the tabbed side. This will cause tab duplication instead of data
        // loss. Duplication will be cleaned up and handled on the next restart. While data loss
        // would not be recoverable.
        mTabPersistentStore.resumeSaveTabList();
        orchestrator.getTabPersistentStore().resumeSaveTabList();
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

    public void setRescueTabsCalledForTesting(boolean called) {
        mRescueTabsCalled = called;
    }

    public void resetRescueArchivedTabGroupsForTesting() {
        mRescueTabGroupsCalled = false;
    }

    public void setTabModelSelectorForTesting(TabModelSelectorBase tabModelSelector) {
        mTabModelSelector = tabModelSelector;
        mTabStateInitializedSupplier.set(tabModelSelector.isTabStateInitialized());
    }

    public int getLeaseCountForTesting() {
        return mLeaseCount;
    }

    public @Nullable Runnable getGracePeriodTeardownRunnableForTesting() {
        return mGracePeriodTeardownRunnable;
    }

    public void performTeardownForTesting() {
        performTeardown();
    }

    public static void setInstanceForTesting(@Nullable ArchivedTabModelOrchestrator instance) {
        var previous = sInstanceForTesting;
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = previous);
    }

    static void registerSavedActivityOrchestratorForTesting(
            TabbedModeTabModelOrchestrator orchestrator) {
        sSavedActivityOrchestrators.add(orchestrator);
        ResettersForTesting.register(() -> sSavedActivityOrchestrators.remove(orchestrator));
    }

    public static ArchivedTabModelOrchestrator createForTesting(Profile profile) {
        return new ArchivedTabModelOrchestrator(profile);
    }

    public void setTabStateInitializedForTesting(boolean initialized) {
        mTabStateInitializedSupplier.set(initialized);
    }
}
