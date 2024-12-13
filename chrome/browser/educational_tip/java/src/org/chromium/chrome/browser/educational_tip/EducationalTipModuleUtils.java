// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

import java.util.HashSet;

/** Utilities for educational tip modules. */
public class EducationalTipModuleUtils {

    /** Returns a list of module types supported by EducationalTip builder and mediator. */
    public static HashSet<Integer> getModuleTypes() {
        HashSet<Integer> modules = new HashSet<>();
        modules.add(ModuleType.DEFAULT_BROWSER_PROMO);
        modules.add(ModuleType.TAB_GROUPS);
        modules.add(ModuleType.TAB_GROUP_SYNC);
        modules.add(ModuleType.QUICK_DELETE);
        return modules;
    }

    /** Returns the card type based on module type. */
    @EducationalTipCardType
    static Integer getCardType(@ModuleType int moduleType) {
        switch (moduleType) {
            case ModuleType.DEFAULT_BROWSER_PROMO:
                return EducationalTipCardType.DEFAULT_BROWSER_PROMO;
            case ModuleType.TAB_GROUPS:
                return EducationalTipCardType.TAB_GROUP;
            case ModuleType.TAB_GROUP_SYNC:
                return EducationalTipCardType.TAB_GROUP_SYNC;
            case ModuleType.QUICK_DELETE:
                return EducationalTipCardType.QUICK_DELETE;
            default:
                return null;
        }
    }
}
