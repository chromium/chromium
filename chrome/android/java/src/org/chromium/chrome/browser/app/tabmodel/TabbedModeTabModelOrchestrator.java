// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.app.Activity;
import android.os.Build;
import android.util.Pair;

import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.ui.widget.Toast;

/**
 * Glue-level class that manages lifetime of root .tabmodel objects: {@link TabPersistentStore} and
 * {@link TabModelSelectorImpl} for tabbed mode.
 */
public class TabbedModeTabModelOrchestrator extends TabModelOrchestrator {
    public TabbedModeTabModelOrchestrator() {}

    /**
     * Creates the TabModelSelector and the TabPersistentStore.
     *
     * @return Whether the creation was successful. It may fail is we reached the limit of number of
     *         windows.
     */
    public boolean createTabModels(Activity activity, TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex) {
        boolean mergeTabs = shouldMergeTabs(activity);
        if (mergeTabs) {
            MultiInstanceManager.mergedOnStartup();
        }

        // Instantiate TabModelSelectorImpl
        Pair<Integer, TabModelSelector> selectorAssignment =
                TabWindowManagerSingleton.getInstance().requestSelector(
                        activity, tabCreatorManager, nextTabPolicySupplier, selectorIndex);
        int assignedIndex = selectorAssignment.first;
        mTabModelSelector = (TabModelSelectorImpl) selectorAssignment.second;
        if (mTabModelSelector == null) {
            markTabModelsInitialized();
            Toast.makeText(activity,
                         activity.getString(
                                 org.chromium.chrome.R.string.unsupported_number_of_windows),
                         Toast.LENGTH_LONG)
                    .show();
            return false;
        }

        // Instantiate TabPersistentStore
        TabPersistencePolicy tabPersistencePolicy =
                new TabbedModeTabPersistencePolicy(assignedIndex, mergeTabs);
        mTabPersistentStore =
                new TabPersistentStore(tabPersistencePolicy, mTabModelSelector, tabCreatorManager);

        wireSelectorAndStore();
        markTabModelsInitialized();
        return true;
    }

    private boolean shouldMergeTabs(Activity activity) {
        // Merge tabs if this TabModelSelector is for a ChromeTabbedActivity created in
        // fullscreen mode and there are no TabModelSelector's currently alive. This indicates
        // that it is a cold start or process restart in fullscreen mode.
        boolean mergeTabs = Build.VERSION.SDK_INT > Build.VERSION_CODES.M
                && MultiInstanceManager.isTabModelMergingEnabled()
                && !activity.isInMultiWindowMode();
        if (MultiInstanceManager.shouldMergeOnStartup(activity)) {
            mergeTabs = mergeTabs
                    && (!MultiWindowUtils.getInstance().isInMultiDisplayMode(activity)
                            || TabWindowManagerSingleton.getInstance()
                                            .getNumberOfAssignedTabModelSelectors()
                                    == 0);
        } else {
            mergeTabs = mergeTabs
                    && TabWindowManagerSingleton.getInstance()
                                    .getNumberOfAssignedTabModelSelectors()
                            == 0;
        }
        return mergeTabs;
    }
}
