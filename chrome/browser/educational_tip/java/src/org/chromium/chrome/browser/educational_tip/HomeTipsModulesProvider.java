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
import java.util.List;

/**
 * Manages the registration of high-level module collections on the home surface, like the Setup
 * List or Educational Tips.
 */
@NullMarked
public class HomeTipsModulesProvider {
    /**
     * Registers the appropriate module builders with the ModuleRegistry based on feature flags and
     * the Setup List state.
     *
     * <p>If the Setup List is active and the two-cell layout should be shown, it registers the
     * {@link EducationalTipModuleTwoCellBuilder} for {@link
     * ModuleType#SETUP_LIST_TWO_CELL_CONTAINER}. If the Setup List is active but the single-cell
     * layout should be shown, it registers {@link EducationalTipModuleBuilder} for each individual
     * setup list item. Otherwise, it registers the standard Educational Tip modules using {@link
     * EducationalTipModuleBuilder}.
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
            if (showTwoCell) {
                moduleRegistry.registerModule(
                        moduleType, new EducationalTipModuleTwoCellBuilder(actionDelegate));
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
            // If the "Set Up List" feature is active, return its ranked modules.
            if (showTwoCell) {
                return List.of(ModuleType.SETUP_LIST_TWO_CELL_CONTAINER);
            }
            return SetupListModuleUtils.getRankedModuleTypes();
        } else {
            // Fall back to returning the default Educational Tip modules.
            return EducationalTipModuleUtils.getModuleTypes();
        }
    }
}
