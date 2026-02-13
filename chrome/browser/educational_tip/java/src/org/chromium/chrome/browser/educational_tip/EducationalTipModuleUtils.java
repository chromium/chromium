// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

import java.util.HashSet;
import java.util.function.Supplier;

/** Utilities for educational tip modules. */
@NullMarked
public class EducationalTipModuleUtils {

    /** Returns a list of module types supported by EducationalTip builder and mediator. */
    public static HashSet<Integer> getModuleTypes() {
        HashSet<Integer> modules = new HashSet<>();
        modules.add(ModuleType.DEFAULT_BROWSER_PROMO);
        modules.add(ModuleType.TAB_GROUP_PROMO);
        modules.add(ModuleType.TAB_GROUP_SYNC_PROMO);
        modules.add(ModuleType.QUICK_DELETE_PROMO);
        modules.add(ModuleType.HISTORY_SYNC_PROMO);
        modules.add(ModuleType.TIPS_NOTIFICATIONS_PROMO);
        return modules;
    }

    /**
     * Creates a {@link BottomSheetObserver} that triggers an update callback when a bottom sheet is
     * dismissed.
     *
     * @param shouldSkipUpdate A supplier that returns true if the update should be skipped (e.g.
     *     for Default Browser when navigating to settings).
     * @param updateCallback The callback to run when the sheet is hidden.
     * @return A new BottomSheetObserver.
     */
    public static BottomSheetObserver createBottomSheetObserver(
            Supplier<Boolean> shouldSkipUpdate, Runnable updateCallback) {
        return new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(int newState, int reason) {
                if (newState == BottomSheetController.SheetState.HIDDEN) {
                    if (shouldSkipUpdate.get()
                            && reason
                                    == BottomSheetController.StateChangeReason
                                            .INTERACTION_COMPLETE) {
                        return;
                    }
                    updateCallback.run();
                }
            }
        };
    }
}
