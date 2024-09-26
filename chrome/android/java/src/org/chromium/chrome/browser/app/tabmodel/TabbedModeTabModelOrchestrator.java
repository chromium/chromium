// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.app.Activity;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.ui.widget.Toast;

/**
 * Glue-level class that manages lifetime of root .tabmodel objects: {@link TabPersistentStore} and
 * {@link TabModelSelectorImpl} for tabbed mode.
 */
public class TabbedModeTabModelOrchestrator extends TabModelOrchestrator {
    private final boolean mTabMergingEnabled;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final CipherFactory mCipherFactory;

    // This class is driven by TabbedModeTabModelOrchestrator to prevent duplicate glue code in
    //  ChromeTabbedActivity.
    private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private TabCreatorManager mTabCreatorManager;

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

    /**
     * Creates the TabModelSelector and the TabPersistentStore.
     *
     * @param activity The activity that hosts this TabModelOrchestrator.
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
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            MismatchedIndicesHandler mismatchedIndicesHandler,
            int selectorIndex) {
        mProfileProviderSupplier = profileProviderSupplier;
        mTabCreatorManager = tabCreatorManager;
        boolean mergeTabsOnStartup = shouldMergeTabs(activity);
        if (mergeTabsOnStartup) {
            MultiInstanceManager.mergedOnStartup();
        }

        // Instantiate TabModelSelectorImpl
        Pair<Integer, TabModelSelector> selectorAssignment =
                TabWindowManagerSingleton.getInstance()
                        .requestSelector(
                                activity,
                                profileProviderSupplier,
                                tabCreatorManager,
                                nextTabPolicySupplier,
                                mismatchedIndicesHandler,
                                selectorIndex);
        if (selectorAssignment == null) {
            mTabModelSelector = null;
        } else {
            mTabModelSelector = (TabModelSelectorBase) selectorAssignment.second;
        }

        if (mTabModelSelector == null) {
            markTabModelsInitialized();
            Toast.makeText(
                            activity,
                            activity.getString(
                                    org.chromium.chrome.R.string.unsupported_number_of_windows),
                            Toast.LENGTH_LONG)
                    .show();
            return false;
        }

        int assignedIndex = selectorAssignment.first;

        // Instantiate TabPersistentStore
        mTabPersistencePolicy =
                new TabbedModeTabPersistencePolicy(
                        assignedIndex, mergeTabsOnStartup, mTabMergingEnabled);
        mTabPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_REGULAR,
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
            return MultiWindowUtils.getInstanceCount() == 0;
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

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected boolean isMultiInstanceApi31Enabled() {
        return MultiWindowUtils.isMultiInstanceApi31Enabled();
    }

    @Override
    public void cleanupInstance(int instanceId) {
        mTabPersistentStore.cleanupStateFile(instanceId);
    }

    @Override
    public void onNativeLibraryReady(TabContentManager tabContentManager) {
        super.onNativeLibraryReady(tabContentManager);

        if (ChromeFeatureList.sAndroidTabDeclutterRescueKillSwitch.isEnabled()) {
            DeferredStartupHandler.getInstance()
                    .addDeferredTask(
                            () -> createAndInitArchivedTabModelOrchestrator(tabContentManager));
            DeferredStartupHandler.getInstance().queueDeferredTasksOnIdleHandler();
        }
    }

    @Override
    public void saveState() {
        super.saveState();
        if (mArchivedTabModelOrchestrator != null
                && mArchivedTabModelOrchestrator.areTabModelsInitialized()) {
            mArchivedTabModelOrchestrator.saveState();
        }
    }

    private void createAndInitArchivedTabModelOrchestrator(TabContentManager tabContentManager) {
        if (mActivityLifecycleDispatcher.isActivityFinishingOrDestroyed()) return;
        ThreadUtils.assertOnUiThread();
        // The profile will be available because native is initialized.
        assert mProfileProviderSupplier.hasValue();
        assert tabContentManager != null;

        Profile profile = mProfileProviderSupplier.get().getOriginalProfile();
        assert profile != null;

        TabCreator regularTabCreator = mTabCreatorManager.getTabCreator(/* incognito= */ false);
        mArchivedTabModelOrchestrator = ArchivedTabModelOrchestrator.getForProfile(profile);
        mArchivedTabModelOrchestrator.maybeCreateAndInitTabModels(
                tabContentManager, regularTabCreator, mCipherFactory);
        mArchivedTabModelOrchestrator.initializeHistoricalTabModelObserver(
                () -> getTabModelSelector().getModel(/* incognito= */ false));

        // If the feature flag is enabled, then start the declutter process. Otherwise, rescue
        // tabs that may have been archived previously.
        if (ChromeFeatureList.sAndroidTabDeclutter.isEnabled()) {
            mArchivedTabModelOrchestrator.maybeBeginDeclutter();
        } else {
            mArchivedTabModelOrchestrator.maybeRescueArchivedTabs();
        }
    }

    public TabPersistentStore getTabPersistentStoreForTesting() {
        return mTabPersistentStore;
    }
}
