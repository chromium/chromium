// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import static org.chromium.chrome.browser.firstrun.FirstRunStatus.isFirstRunTriggered;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Utilities for setup list modules. */
@NullMarked
public class SetupListModuleUtils {
    public static final long SETUP_LIST_ACTIVE_WINDOW_MILLIS = TimeUnit.DAYS.toMillis(14);

    /**
     * Returns a ranked list of module types supported by the setup list. The order of modules in
     * this list defines their ranking, with lower indices indicating higher priority (e.g., index 0
     * is rank 1, index 1 is rank 2, etc.).
     */
    public static List<Integer> getRankedModuleTypes() {
        List<Integer> modules = new ArrayList<>();
        // TODO(crbug.com/469425754): Add all the modules once they're ready, in the following order
        // 1. Default Browser
        // 2. Sign In/ Sync
        // 3. Enhanced Safe Browsing
        // 4. PW Management
        // 5. Omnibox Placement
        modules.add(ModuleType.ENHANCED_SAFE_BROWSING_PROMO);
        return modules;
    }

    /** Returns whether the setup list is active based on the 14-day window. */
    public static boolean isSetupListActive() {
        if (isFirstRunTriggered()) {
            // Only enabled from the second run onwards.
            return false;
        }
        long firstCtaStartTimestamp =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, -1L);
        if (firstCtaStartTimestamp == -1L) {
            // If timestamp is not set, setup list is not active.
            return false;
        }

        return (TimeUtils.currentTimeMillis() - firstCtaStartTimestamp)
                < SETUP_LIST_ACTIVE_WINDOW_MILLIS;
    }
}
