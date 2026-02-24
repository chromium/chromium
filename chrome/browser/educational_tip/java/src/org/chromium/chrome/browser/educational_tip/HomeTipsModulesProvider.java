// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellBuilder;
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
     * Registers module builders with the ModuleRegistry based on the Setup List state.
     *
     * <p>If the Setup List is active ({@link SetupListModuleUtils#isSetupListActive()}), this
     * method registers either {@link EducationalTipModuleTwoCellBuilder} or {@link
     * EducationalTipModuleBuilder} instances, depending on {@link
     * SetupListModuleUtils#shouldShowTwoCellLayout()}. Otherwise, it registers builders for the
     * default educational tips.
     *
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     * @param moduleRegistry The instance of {@link ModuleRegistry}.
     */
    public static void registerTipModules(
            EducationTipModuleActionDelegate actionDelegate, ModuleRegistry moduleRegistry) {
        boolean isSetupListActive = SetupListModuleUtils.isSetupListActive();
        boolean showTwoCell = SetupListModuleUtils.shouldShowTwoCellLayout();

        Collection<Integer> modulesToRegister =
                getModuleTypesToRegister(isSetupListActive, showTwoCell);
        for (@ModuleType int moduleType : modulesToRegister) {
            if (showTwoCell && moduleType != ModuleType.SETUP_LIST_CELEBRATORY_PROMO) {
                moduleRegistry.registerModule(
                        moduleType,
                        new EducationalTipModuleTwoCellBuilder(moduleType, actionDelegate));
            } else {
                moduleRegistry.registerModule(
                        moduleType, new EducationalTipModuleBuilder(moduleType, actionDelegate));
            }
        }
    }

    /**
     * Returns the collection of ModuleTypes to be registered based on whether the Setup List is
     * active and whether the two-cell layout should be shown.
     *
     * @param isSetupListActive Whether the Setup List feature is currently active.
     * @param showTwoCell Whether the two-cell layout should be used for the Setup List.
     * @return A collection of {@link ModuleType} integers.
     */
    @VisibleForTesting
    static Collection<Integer> getModuleTypesToRegister(
            boolean isSetupListActive, boolean showTwoCell) {
        if (isSetupListActive) {
            // Register both Setup List and standard Educational Tip modules. This allows the UI to
            // transition seamlessly from the Setup List (Celebration card) back to standard Tips
            // within the same session once all tasks are complete. Actual visibility is enforced
            // via each builder's isEligible() check.
            java.util.Set<Integer> modules = new java.util.HashSet<>();
            modules.addAll(SetupListModuleUtils.getModuleTypesForRegistration(showTwoCell));
            modules.addAll(EducationalTipModuleUtils.getModuleTypes());
            return modules;
        } else {
            // Fall back to returning the default Educational Tip modules.
            return EducationalTipModuleUtils.getModuleTypes();
        }
    }
}
