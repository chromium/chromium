// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateFrozenTabArguments;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.Toast;

import java.util.function.Supplier;

/**
 * Glue-level class that manages lifetime of root .tabmodel objects: {@link TabPersistentStore} and
 * {@link TabModelSelectorImpl} for tabbed mode.
 */
@NullMarked
public class TabbedModeTabModelOrchestrator extends TabModelOrchestrator {
    private static final String TAG = "TMTMOrchestrator";

    /**
     * Allows for an easy conversion from {@link TabPersistentStore} into something @{link
     * SupplierUtils.waitForAll} can consume.
     */
    private static class OneshotStateLoadedObserver extends OneshotSupplierImpl<Boolean>
            implements TabPersistentStoreObserver {
        private final TabPersistentStore mTabPersistentStore;

        private OneshotStateLoadedObserver(TabPersistentStore tabPersistentStore) {
            mTabPersistentStore = tabPersistentStore;
            tabPersistentStore.addObserver(this);
        }

        @Override
        public void onStateLoaded() {
            set(true);
            mTabPersistentStore.removeObserver(this);
        }
    }

    private final boolean mTabMergingEnabled;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final CipherFactory mCipherFactory;
    // Effectively final after createTabModels().
    private @MonotonicNonNull String mWindowTag;

    private @MonotonicNonNull OneshotSupplier<ProfileProvider> mProfileProviderSupplier;

    // This class is driven by TabbedModeTabModelOrchestrator to prevent duplicate glue code in
    // ChromeTabbedActivity.
    private @MonotonicNonNull ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private @Nullable Supplier<TabModel> mArchivedHistoricalObserverSupplier;

    // Currently used to perform shadow operations for an alternative storage. Not always enabled.
    private @Nullable TabPersistentStore mShadowTabPersistentStore;
    private @Nullable Boolean mTabStateStoreIsAuthoritative;
    private final AccumulatingTabCreator mRegularShadowTabCreator = new AccumulatingTabCreator();
    private final AccumulatingTabCreator mIncognitoShadowTabCreator = new AccumulatingTabCreator();

    /**
     * Constructor.
     *
     * @param tabMergingEnabled Whether we are on the platform where tab merging is enabled.
     * @param activityLifecycleDispatcher Used to determine if the current activity context is still
     *     valid when running deferred tasks.
     * @param cipherFactory The {@link CipherFactory} used for encrypting and decrypting files.
     */
    public TabbedModeTabModelOrchestrator(
            boolean tabMergingEnabled,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            CipherFactory cipherFactory) {
        mTabMergingEnabled = tabMergingEnabled;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mCipherFactory = cipherFactory;
    }

    @Override
    public void destroy() {
        if (mArchivedTabModelOrchestrator != null) {
            mArchivedTabModelOrchestrator.removeHistoricalTabModelObserver(
                    assumeNonNull(mArchivedHistoricalObserverSupplier));
            mArchivedTabModelOrchestrator.unregisterTabModelOrchestrator(this);
        }
        if (mShadowTabPersistentStore != null) {
            mShadowTabPersistentStore.destroy();
            mShadowTabPersistentStore = null;
        }
        super.destroy();
    }

    @EnsuresNonNull({
        "mTabPersistentStore",
        "mTabPersistencePolicy",
        "mWindowTag",
        "mTabModelSelector",
        "mProfileProviderSupplier",
    })
    private void assertCreated() {
        assert mTabPersistentStore != null;
        assert mTabPersistencePolicy != null;
        assert mWindowTag != null;
        assert mTabModelSelector != null;
        assert mProfileProviderSupplier != null;
    }

    /**
     * Creates the TabModelSelector and the TabPersistentStore.
     *
     * @param activity The activity that hosts this TabModelOrchestrator.
     * @param modalDialogManager The {@link ModalDialogManager}.
     * @param profileProviderSupplier Supplies the {@link ProfileProvider} for the activity.
     * @param tabCreatorManager Manager for the {@link TabCreator} for the {@link TabModelSelector}.
     * @param nextTabPolicySupplier Policy for what to do when a tab is closed.
     * @param mismatchedIndicesHandler Handles when indices are mismatched.
     * @param selectorIndex Which index to use when requesting a selector.
     * @return Whether the creation was successful. It may fail is we reached the limit of number of
     *     windows.
     */
    public boolean createTabModels(
            Activity activity,
            ModalDialogManager modalDialogManager,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            MultiInstanceManager multiInstanceManager,
            MismatchedIndicesHandler mismatchedIndicesHandler,
            int selectorIndex) {
        mProfileProviderSupplier = profileProviderSupplier;
        boolean mergeTabsOnStartup = shouldMergeTabs(activity);
        if (mergeTabsOnStartup) {
            MultiInstanceManager.mergedOnStartup();
        }

        // Instantiate TabModelSelectorImpl
        Pair<Integer, TabModelSelector> selectorAssignment =
                TabWindowManagerSingleton.getInstance()
                        .requestSelector(
                                activity,
                                modalDialogManager,
                                profileProviderSupplier,
                                tabCreatorManager,
                                nextTabPolicySupplier,
                                multiInstanceManager,
                                mismatchedIndicesHandler,
                                selectorIndex);
        if (selectorAssignment == null) {
            // We will early out and handle this case below.
            mTabModelSelector = assumeNonNull(null);
        } else {
            mTabModelSelector = (TabModelSelectorBase) selectorAssignment.second;
        }

        if (mTabModelSelector == null) {
            markTabModelsInitialized();
            Toast.makeText(
                            activity,
                            activity.getString(R.string.unsupported_number_of_windows),
                            Toast.LENGTH_LONG)
                    .show();
            mWindowTag = "";
            return false;
        }

        int assignedIndex = assumeNonNull(selectorAssignment).first;
        assert assignedIndex != TabWindowManager.INVALID_WINDOW_ID;
        mWindowTag = Integer.toString(assignedIndex);

        // Instantiate TabPersistentStore
        mTabPersistencePolicy =
                new TabbedModeTabPersistencePolicy(
                        assignedIndex, mergeTabsOnStartup, mTabMergingEnabled);
        mTabPersistentStore =
                new TabPersistentStoreImpl(
                        TabPersistentStoreImpl.CLIENT_TAG_REGULAR,
                        mTabPersistencePolicy,
                        mTabModelSelector,
                        tabCreatorManager,
                        TabWindowManagerSingleton.getInstance(),
                        mCipherFactory);

        wireSelectorAndStore();
        markTabModelsInitialized();
        return true;
    }

    private boolean shouldMergeTabs(Activity activity) {
        if (isMultiInstanceApi31Enabled()) {
            // For multi-instance on Android S, this is a restart after the upgrade or fresh
            // installation. Allow merging tabs from CTA/CTA2 used by the previous version
            // if present.
            return MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY) == 0;
        }

        // Merge tabs if this TabModelSelector is for a ChromeTabbedActivity created in
        // fullscreen mode and there are no TabModelSelector's currently alive. This indicates
        // that it is a cold start or process restart in fullscreen mode.
        boolean mergeTabs = mTabMergingEnabled && !activity.isInMultiWindowMode();
        if (MultiInstanceManager.shouldMergeOnStartup(activity)) {
            mergeTabs =
                    mergeTabs
                            && (!MultiWindowUtils.getInstance().isInMultiDisplayMode(activity)
                                    || TabWindowManagerSingleton.getInstance()
                                                    .getNumberOfAssignedTabModelSelectors()
                                            == 0);
        } else {
            mergeTabs =
                    mergeTabs
                            && TabWindowManagerSingleton.getInstance()
                                            .getNumberOfAssignedTabModelSelectors()
                                    == 0;
        }
        return mergeTabs;
    }

    @VisibleForTesting
    protected boolean isMultiInstanceApi31Enabled() {
        return MultiWindowUtils.isMultiInstanceApi31Enabled();
    }

    @Override
    public void cleanupInstance(int instanceId) {
        assertCreated();
        mTabPersistentStore.cleanupStateFile(instanceId);
    }

    @Override
    public void onNativeLibraryReady(TabContentManager tabContentManager) {
        super.onNativeLibraryReady(tabContentManager);

        if (!ChromeFeatureList.sAndroidTabDeclutterRescueKillSwitch.isEnabled()) {
            return;
        }
        assertCreated();

        if (ChromeFeatureList.sAndroidTabDeclutterPerformanceImprovements.isEnabled()) {
            TabModelUtils.runOnTabStateInitialized(
                    mTabModelSelector,
                    (selector) -> {
                        createArchivedTabModelInDeferredTask(tabContentManager);
                    });
        } else {
            createArchivedTabModelInDeferredTask(tabContentManager);
        }

        if (TabStateStorageFlagHelper.isTabStorageEnabled()) {
            mTabStateStoreIsAuthoritative = TabStateStorageFlagHelper.isStorageAuthoritative();
            // Temporary variable usage to avoid unused variable warning.
            Log.i(TAG, "mTabStateStoreIsAuthoritative: " + mTabStateStoreIsAuthoritative);

            assert mProfileProviderSupplier.get() != null;
            ProfileProvider profileProvider = mProfileProviderSupplier.get();
            Profile profile = profileProvider.getOriginalProfile();
            assert profile != null;

            TabCreatorManager shadowTabCreatorManager =
                    incognito -> incognito ? mIncognitoShadowTabCreator : mRegularShadowTabCreator;
            assert !mWindowTag.isEmpty();
            mShadowTabPersistentStore =
                    new TabStateStore(
                            TabStateStorageServiceFactory.getForProfile(profile),
                            mTabModelSelector,
                            mWindowTag,
                            shadowTabCreatorManager);

            SupplierUtils.waitForAll(
                    this::onBothStateLoaded,
                    new OneshotStateLoadedObserver(mTabPersistentStore),
                    new OneshotStateLoadedObserver(mShadowTabPersistentStore));
        }
    }

    private void onBothStateLoaded() {
        assertCreated();
        // Unless mTabStateStoreIsAuthoritative is true, createNewTabArgumentsList should be empty.
        assert Boolean.FALSE.equals(mTabStateStoreIsAuthoritative)
                || mRegularShadowTabCreator.createNewTabArgumentsList.isEmpty();

        TabModel tabModel = mTabModelSelector.getModel(/* incognito= */ false);
        int tabCountDelta =
                tabModel.getCount() - mRegularShadowTabCreator.createFrozenTabArgumentsList.size();
        if (tabCountDelta > 0) {
            RecordHistogram.recordCount1000Histogram(
                    "Tabs.TabStateStore.TabCountDelta.AuthoritativeHigher", tabCountDelta);
        } else if (tabCountDelta < 0) {
            RecordHistogram.recordCount1000Histogram(
                    "Tabs.TabStateStore.TabCountDelta.ShadowHigher", -tabCountDelta);
        }

        for (CreateFrozenTabArguments arguments :
                mRegularShadowTabCreator.createFrozenTabArgumentsList) {
            Tab tab = tabModel.getTabById(arguments.id);
            if (tab == null || arguments.state.contentsState == null) continue;

            String authUrl = tab.getUrl().getSpec();
            String shadowUrl = arguments.state.contentsState.getVirtualUrlFromState();

            if (!TextUtils.equals(authUrl, shadowUrl)) {
                long timeDelta = tab.getTimestampMillis() - arguments.state.timestampMillis;
                if (timeDelta > 0) {
                    RecordHistogram.recordTimesHistogram(
                            "Tabs.TabStateStore.TimeDeltaOnMismatch.AuthoritativeNewer", timeDelta);
                } else if (timeDelta < 0) {
                    RecordHistogram.recordTimesHistogram(
                            "Tabs.TabStateStore.TimeDeltaOnMismatch.ShadowNewer", -timeDelta);
                }
            }
        }

        for (CreateFrozenTabArguments arguments :
                mRegularShadowTabCreator.createFrozenTabArgumentsList) {
            WebContentsState webContentsState = arguments.state.contentsState;
            if (webContentsState != null) {
                webContentsState.destroy();
            }
        }
        mRegularShadowTabCreator.createNewTabArgumentsList.clear();
        mRegularShadowTabCreator.createFrozenTabArgumentsList.clear();
    }

    private void createArchivedTabModelInDeferredTask(TabContentManager tabContentManager) {
        DeferredStartupHandler.getInstance()
                .addDeferredTask(
                        () -> createAndInitArchivedTabModelOrchestrator(tabContentManager));
        DeferredStartupHandler.getInstance().queueDeferredTasksOnIdleHandler();
    }

    @Override
    public void saveState() {
        super.saveState();
        if (mArchivedTabModelOrchestrator != null
                && mArchivedTabModelOrchestrator.areTabModelsInitialized()) {
            mArchivedTabModelOrchestrator.saveState();
        }
    }

    @Override
    public void loadState(
            boolean ignoreIncognitoFiles, @Nullable Callback<String> onStandardActiveIndexRead) {
        super.loadState(ignoreIncognitoFiles, onStandardActiveIndexRead);
        if (mShadowTabPersistentStore != null) {
            mShadowTabPersistentStore.loadState(ignoreIncognitoFiles);
        }
    }

    @Override
    public void restoreTabs(boolean setActiveTab) {
        super.restoreTabs(setActiveTab);
        if (mShadowTabPersistentStore != null) {
            mShadowTabPersistentStore.restoreTabs(setActiveTab);
        }
    }

    private void createAndInitArchivedTabModelOrchestrator(TabContentManager tabContentManager) {
        if (mActivityLifecycleDispatcher.isActivityFinishingOrDestroyed()) return;
        ThreadUtils.assertOnUiThread();
        assertCreated();
        // The profile will be available because native is initialized.
        assert mProfileProviderSupplier.get() != null;
        assert tabContentManager != null;

        Profile profile = mProfileProviderSupplier.get().getOriginalProfile();
        assert profile != null;

        mArchivedTabModelOrchestrator = ArchivedTabModelOrchestrator.getForProfile(profile);
        mArchivedTabModelOrchestrator.maybeCreateAndInitTabModels(
                tabContentManager, mCipherFactory);
        mArchivedHistoricalObserverSupplier =
                () -> mTabModelSelector.getModel(/* incognito= */ false);
        mArchivedTabModelOrchestrator.initializeHistoricalTabModelObserver(
                mArchivedHistoricalObserverSupplier);
        // Registering will automatically do an archive pass, and schedule recrurring passes for
        // long-running instances of Chrome.
        mArchivedTabModelOrchestrator.registerTabModelOrchestrator(this);
    }

    public TabPersistentStoreImpl getTabPersistentStoreForTesting() {
        assertCreated();
        return (TabPersistentStoreImpl) mTabPersistentStore;
    }
}
