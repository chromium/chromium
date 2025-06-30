// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

import java.util.List;

/**
 * Common utility class for {@link TabModelImpl} and {@link TabCollectionTabModelImpl}. Allows
 * extracting common logic out of the two models.
 */
@NullMarked
public class TabModelImplUtil {
    /**
     * Returns the next tab to select after closing the given tabs.
     *
     * @param model The {@link TabModel} to act on.
     * @param modelDelegate The {@link TabModelDelegate} to get the current tab from.
     * @param currentTabSupplier The {@link ObservableSupplier} that supplies the current tab.
     * @param nextTabPolicySupplier The {@link NextTabPolicySupplier} to get the next tab policy.
     * @param closingTabs The list of tabs that are closing.
     * @param uponExit Whether the app is closing as a result of this tab closure.
     * @param tabCloseType The type of tab closure.
     * @return The next tab to select after closing the given tabs or null if no tab could be found.
     */
    static @Nullable Tab getNextTabIfClosed(
            TabModel model,
            TabModelDelegate modelDelegate,
            ObservableSupplier<@Nullable Tab> currentTabSupplier,
            NextTabPolicySupplier nextTabPolicySupplier,
            List<Tab> closingTabs,
            boolean uponExit,
            @TabCloseType int tabCloseType) {
        // If closing a tab in the non-active model, select the current tab in the active model.
        if (!model.isActiveModel()) {
            Tab otherModelTab = TabModelUtils.getCurrentTab(modelDelegate.getCurrentModel());
            return otherModelTab != null && !otherModelTab.isClosing() ? otherModelTab : null;
        }

        // If the current tab is not closing, return it.
        Tab currentTab = currentTabSupplier.get();
        if (validNextTab(currentTab) && !closingTabs.contains(currentTab)) {
            return currentTab;
        }

        // If uponExit, select the next most recent tab.
        if (uponExit) {
            Tab nextMostRecentTab = TabModelUtils.getMostRecentTab(model, closingTabs);
            if (validNextTab(nextMostRecentTab)) {
                return nextMostRecentTab;
            }
        }

        // If not in overview mode, select the parent tab if it exists.
        if (closingTabs.size() == 1 && NextTabPolicy.HIERARCHICAL == nextTabPolicySupplier.get()) {
            Tab parentTab =
                    findTabInAllTabModels(
                            modelDelegate,
                            model.isIncognitoBranded(),
                            closingTabs.get(0).getParentId());
            if (validNextTab(parentTab)) {
                return parentTab;
            }
        }

        // Select a nearby tab if one exists.
        if (tabCloseType != TabCloseType.ALL) {
            Tab nearbyTab =
                    findNearbyNotClosingTab(model, model.indexOf(closingTabs.get(0)), closingTabs);
            if (validNextTab(nearbyTab)) {
                return nearbyTab;
            }
        }

        // If closing the last incognito tab, select the current normal tab.
        if (model.isIncognitoBranded()) {
            Tab regularCurrentTab = TabModelUtils.getCurrentTab(modelDelegate.getModel(false));
            if (validNextTab(regularCurrentTab)) {
                return regularCurrentTab;
            }
        }

        return null;
    }

    private static boolean validNextTab(@Nullable Tab tab) {
        return tab != null && !tab.isClosing();
    }

    /**
     * Returns the tab with the given ID checking both incognito and normal tab models.
     *
     * @param modelDelegate The {@link TabModelDelegate} to get the tab from.
     * @param isIncognito Whether to start with checking the incognito tab model.
     * @param tabId The ID of the tab to get.
     * @return The tab with the given ID or null if no tab could be found.
     */
    private static @Nullable Tab findTabInAllTabModels(
            TabModelDelegate modelDelegate, boolean isIncognito, int tabId) {
        Tab tab = modelDelegate.getModel(isIncognito).getTabById(tabId);
        if (tab != null) return tab;
        return modelDelegate.getModel(!isIncognito).getTabById(tabId);
    }

    /**
     * Returns the tab that is closest to the given index, if any.
     *
     * @param model The {@link TabModel} to act on.
     * @param closingIndex The index of the tab that is closing.
     * @param closingTabs The list of tabs that are closing. This is used to avoid returning a tab
     *     that is closing.
     * @return The closest tab or null if no tab could be found.
     */
    public static @Nullable Tab findNearbyNotClosingTab(
            TabModel model, int closingIndex, List<Tab> closingTabs) {
        Tab nearestTab = null;
        for (int i = 0; i < model.getCount(); i++) {
            if (i == closingIndex) {
                continue;
            } else if (i > closingIndex && nearestTab != null) {
                return nearestTab;
            }
            Tab tab = model.getTabAtChecked(i);
            if (!tab.isClosing() && !closingTabs.contains(tab)) {
                nearestTab = tab;
            }
        }
        return nearestTab;
    }
}
