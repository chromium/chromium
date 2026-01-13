// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

import java.util.ArrayList;
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
        List<Integer> modules = new ArrayList<>();
        // TODO(crbug.com/469425754): Add all the modules once they're ready, in the following order
        // 1. Default Browser
        // 2. Sign In/ Sync
        // 3. Enhanced Safe Browsing
        // 4. PW Management
        // 5. Address bar Placement
        modules.add(ModuleType.ENHANCED_SAFE_BROWSING_PROMO);
        modules.add(ModuleType.ADDRESS_BAR_PLACEMENT_PROMO);
        return modules;
    }

    /** Returns whether the setup list is active based on the 14-day window. */
    public static boolean isSetupListActive() {
        return SetupListManager.getInstance().isSetupListActive();
    }

    /** Returns whether the module type belongs to the setup list. */
    public static boolean isSetupListModule(@ModuleType int moduleType) {
        switch (moduleType) {
            case ModuleType.ENHANCED_SAFE_BROWSING_PROMO:
            case ModuleType.ADDRESS_BAR_PLACEMENT_PROMO:
                return isSetupListActive();
            default:
                return false;
        }
    }

    public static void setRankedModuleTypesForTesting(List<Integer> rankedModuleTypes) {
        sRankedModuleTypesForTesting = rankedModuleTypes;
    }
}
