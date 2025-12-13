// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

import java.util.HashSet;

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
}
