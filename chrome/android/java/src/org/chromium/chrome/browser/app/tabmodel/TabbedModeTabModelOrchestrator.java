// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.app.Activity;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
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

    // This class is driven by TabbedModeTabModelOrchestrator to prevent duplicate glue code in
    //  ChromeTabbedActivity.
    private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private OneshotSupplier<ProfileProvider> mProfileProviderSupplier;

    /**
     * Constructor.
     *
     * @param tabMergingEnabled Whether we are on the platform where tab merging is enabled.
     */
    public TabbedModeTabModelOrchestrator(boolean tabMergingEnabled) {
        mTabMergingEnabled = tabMergingEnabled;
    }

    /**
     * Creates the TabModelSelector and the TabPersistentStore.
     *
     * @param activity The activity that hosts this TabModelOrchestrator.
     * @param profileProviderSupplier Supplies the {@link ProfileProvider} for the activity.
     * @param tabCreatorManager Manager for the {@link TabCreator} for the {@link TabModelSelector}.
     * @param nextTabPolicyProvider Policy for what to do when a tab is closed.
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
                new TabPersistentStore(mTabPersistencePolicy, mTabModelSelector, tabCreatorManager);

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

        if (ChromeFeatureList.sAndroidTabDeclutter.isEnabled()) {
            // The profile will be available because native is initialized.
            assert mProfileProviderSupplier.hasValue();

            Profile profile = mProfileProviderSupplier.get().getOriginalProfile();
            mArchivedTabModelOrchestrator = ArchivedTabModelOrchestrator.getForProfile(profile);
            mArchivedTabModelOrchestrator.maybeCreateTabModels();
            mArchivedTabModelOrchestrator.onNativeLibraryReady(tabContentManager);
        }
    }

    @Override
    public void loadState(
            boolean ignoreIncognitoFiles, Callback<String> onStandardActiveIndexRead) {
        super.loadState(ignoreIncognitoFiles, onStandardActiveIndexRead);

        if (ChromeFeatureList.sAndroidTabDeclutter.isEnabled()) {
            assert mArchivedTabModelOrchestrator != null;
            mArchivedTabModelOrchestrator.loadState(
                    /* ignoreIncognitoFiles= */ true, /* onStandardActiveIndexRead= */ null);
        }
    }

    @Override
    public void restoreTabs(boolean setActiveTab) {
        super.restoreTabs(setActiveTab);

        if (ChromeFeatureList.sAndroidTabDeclutter.isEnabled()) {
            assert mArchivedTabModelOrchestrator != null;
            mArchivedTabModelOrchestrator.restoreTabs(/* setActiveTab= */ false);
        }
    }

    @Override
    public void saveState() {
        super.saveState();

        if (ChromeFeatureList.sAndroidTabDeclutter.isEnabled()) {
            assert mArchivedTabModelOrchestrator != null;
            mArchivedTabModelOrchestrator.saveState();
        }
    }

    public TabPersistentStore getTabPersistentStoreForTesting() {
        return mTabPersistentStore;
    }
}
