// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;

import java.util.Collection;

/**
 * Manages the registration of high-level module collections on the home surface, like the Setup
 * List or Educational Tips.
 */
@NullMarked
public class HomeTipsModulesProvider {
    /**
     * Registers the appropriate set of modules (either Setup List or Educational Tips) with the
     * ModuleRegistry.
     *
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     * @param moduleRegistry The instance of {@link ModuleRegistry}.
     */
    public static void registerTipModules(
            EducationTipModuleActionDelegate actionDelegate, ModuleRegistry moduleRegistry) {
        Collection<Integer> modulesToRegister = getModuleTypesToRegister();

        for (@ModuleType int moduleType : modulesToRegister) {
            EducationalTipModuleBuilder moduleBuilder =
                    new EducationalTipModuleBuilder(moduleType, actionDelegate);
            moduleRegistry.registerModule(moduleType, moduleBuilder);
        }
    }

    @VisibleForTesting
    static Collection<Integer> getModuleTypesToRegister() {
        if (ChromeFeatureList.sAndroidSetupList.isEnabled()
                && SetupListModuleUtils.isSetupListActive()) {
            // If the "Set Up List" feature is active, return its ranked modules.
            return SetupListModuleUtils.getRankedModuleTypes();
        } else {
            // Fall back to returning the default Educational Tip modules.
            return EducationalTipModuleUtils.getModuleTypes();
        }
    }
}
