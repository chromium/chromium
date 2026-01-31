// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Utilities for setup list modules. */
@NullMarked
public class SetupListModuleUtils {
    @Nullable private static List<Integer> sRankedModuleTypesForTesting;

    // TODO(crbug.com/469425754): Add all the modules once they're ready, in the following order
    // 1. Default Browser
    // 2. Sign In/ Sync
    // 3. Enhanced Safe Browsing
    // 4. PW Management
    // 5. Address bar Placement
    private static final List<Integer> BASE_SETUP_LIST_ORDER =
            Arrays.asList(
                    ModuleType.SIGN_IN_PROMO,
                    ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                    ModuleType.SAVE_PASSWORDS_PROMO,
                    ModuleType.PASSWORD_CHECKUP_PROMO,
                    ModuleType.ADDRESS_BAR_PLACEMENT_PROMO);

    private static final Map<Integer, Integer> sModuleRankMap;

    static {
        sModuleRankMap = new HashMap<>();
        for (int i = 0; i < BASE_SETUP_LIST_ORDER.size(); i++) {
            sModuleRankMap.put(BASE_SETUP_LIST_ORDER.get(i), i);
        }
    }

    private static final @ModuleType int TWO_CELL_CONTAINER_MODULE_TYPE =
            ModuleType.SETUP_LIST_TWO_CELL_CONTAINER;

    /**
     * Returns a ranked list of module types supported by the setup list. The order of modules in
     * this list defines their ranking, with lower indices indicating higher priority (e.g., index 0
     * is rank 1, index 1 is rank 2, etc.).
     */
    public static List<Integer> getRankedModuleTypes() {
        if (sRankedModuleTypesForTesting != null) {
            return sRankedModuleTypesForTesting;
        }
        return new ArrayList<>(BASE_SETUP_LIST_ORDER);
    }

    /** Returns the module type list for the two-cell container. */
    public static List<Integer> getTwoCellContainerModuleTypes() {
        return List.of(TWO_CELL_CONTAINER_MODULE_TYPE);
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
        if (!isSetupListActive()) {
            return null;
        }

        if (shouldShowTwoCellLayout()) {
            return (moduleType == TWO_CELL_CONTAINER_MODULE_TYPE) ? 0 : null;
        }
        return sModuleRankMap.get(moduleType);
    }

    /** Returns whether the module type belongs to the currently active setup list view. */
    public static boolean isSetupListModule(@ModuleType int moduleType) {
        if (!isSetupListActive()) return false;
        if (shouldShowTwoCellLayout()) {
            return moduleType == TWO_CELL_CONTAINER_MODULE_TYPE;
        } else {
            return sModuleRankMap.containsKey(moduleType);
        }
    }

    public static void setRankedModuleTypesForTesting(List<Integer> rankedModuleTypes) {
        sRankedModuleTypesForTesting = rankedModuleTypes;
        // Rebuild the map for testing
        sModuleRankMap.clear();
        if (rankedModuleTypes != null) {
            for (int i = 0; i < rankedModuleTypes.size(); i++) {
                sModuleRankMap.put(rankedModuleTypes.get(i), i);
            }
        }
    }
}
