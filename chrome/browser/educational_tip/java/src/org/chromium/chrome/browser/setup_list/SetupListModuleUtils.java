// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.List;

/** Utilities for setup list modules. */
@NullMarked
public class SetupListModuleUtils {
    @Nullable private static List<Integer> sRankedModuleTypesForTesting;

    /**
     * Returns a ranked list of module types supported by the setup list. The order of modules in
     * this list defines their ranking, with lower indices indicating higher priority (e.g., index 0
     * is rank 1, index 1 is rank 2, etc.).
     */
    public static List<Integer> getRankedModuleTypes() {
        if (sRankedModuleTypesForTesting != null) {
            return sRankedModuleTypesForTesting;
        }
        return SetupListManager.getInstance().getRankedModuleTypes();
    }

    /** Returns the module type list for the two-cell container. */
    public static List<Integer> getTwoCellContainerModuleTypes() {
        return SetupListManager.getInstance().getTwoCellContainerModuleTypes();
    }

    /** Returns whether the setup list is active based on the 14-day window. */
    public static boolean isSetupListActive() {
        return SetupListManager.getInstance().isSetupListActive();
    }

    /** Returns whether the two-cell layout should be shown based on the 3-day window. */
    public static boolean shouldShowTwoCellLayout() {
        return SetupListManager.getInstance().shouldShowTwoCellLayout();
    }

    /**
     * Returns the manual rank for the given module type, or null if it shouldn't be manually
     * ranked.
     */
    @Nullable
    public static Integer getManualRank(@ModuleType int moduleType) {
        return SetupListManager.getInstance().getManualRank(moduleType);
    }

    /** Returns whether the module type belongs to the currently active setup list view. */
    public static boolean isSetupListModule(@ModuleType int moduleType) {
        return SetupListManager.getInstance().isSetupListModule(moduleType);
    }

    /** Returns whether the given module is completed, using the optimized manager state. */
    public static boolean isModuleCompleted(@ModuleType int moduleType) {
        return SetupListManager.getInstance().isModuleCompleted(moduleType);
    }

    /**
     * Marks the given module type as completed by setting its individual boolean preference key.
     * The {@link SetupListManager} will observe this change and update the ranking automatically.
     */
    public static void setModuleCompleted(@ModuleType int moduleType) {
        String individualPrefKey = getCompletionKeyForModule(moduleType);
        if (individualPrefKey != null) {
            ChromeSharedPreferences.getInstance().writeBoolean(individualPrefKey, true);
        }
    }

    @Nullable
    public static String getCompletionKeyForModule(@ModuleType int type) {
        if (SetupListManager.isBaseSetupListModule(type)) {
            return ChromePreferenceKeys.SETUP_LIST_COMPLETED_KEY_PREFIX.createKey(
                    String.valueOf(type));
        }
        return null;
    }

    public static void setRankedModuleTypesForTesting(List<Integer> rankedModuleTypes) {
        sRankedModuleTypesForTesting = rankedModuleTypes;
    }
}
