// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

import java.util.HashSet;

/** Utilities for educational tip modules. */
@NullMarked
public class EducationalTipModuleUtils {
    private static @Nullable Boolean sIsEducationalTipActiveForTesting;

    /** Returns a list of module types supported by EducationalTip builder and mediator. */
    public static HashSet<Integer> getModuleTypes() {
        HashSet<Integer> modules = new HashSet<>();
        modules.add(ModuleType.DEFAULT_BROWSER_PROMO);
        modules.add(ModuleType.TAB_GROUP_PROMO);
        modules.add(ModuleType.TAB_GROUP_SYNC_PROMO);
        modules.add(ModuleType.QUICK_DELETE_PROMO);
        modules.add(ModuleType.HISTORY_SYNC_PROMO);
        modules.add(ModuleType.NTP_THEME_PROMO);
        return modules;
    }

    /**
     * Creates a {@link BottomSheetObserver} that triggers an update callback when a bottom sheet is
     * dismissed.
     *
     * @param updateCallback The callback to run when the sheet is hidden.
     * @return A new BottomSheetObserver.
     */
    public static BottomSheetObserver createBottomSheetObserver(Runnable updateCallback) {
        return new BottomSheetObserver() {
            @Override
            public void onSheetStateChanged(int newState, int reason) {
                if (newState == BottomSheetController.SheetState.HIDDEN) {
                    updateCallback.run();
                }
            }
        };
    }

    /** Returns whether the educational tip module is active. */
    public static boolean isEducationalTipActive() {
        if (sIsEducationalTipActiveForTesting != null) {
            return sIsEducationalTipActiveForTesting;
        }
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER);
    }

    /** Sets whether the educational tip module is active for testing. */
    public static void setEducationalTipActiveForTesting(boolean active) {
        sIsEducationalTipActiveForTesting = active;
        ResettersForTesting.register(() -> sIsEducationalTipActiveForTesting = null);
    }
}
