// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.magic_stack;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;

import android.content.Context;
import android.os.Bundle;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

import java.util.Set;

/** Fragment that allows the user to configure chrome home modules related preferences. */
public class ChromeHomeModulesConfigSettings extends ChromeBaseSettingsFragment {
    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.home_modules_context_menu_customize);
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getStyledContext()));
        ChromeHomeModulesConfigManager chromeHomeModulesConfigManager =
                ChromeHomeModulesConfigManager.getInstance();

        ModuleRegistry moduleRegistry = ModuleRegistry.getInstance();
        Set<Integer> moduleTypeRegistered = moduleRegistry.getRegisteredModuleTypes();
        for (@ModuleType int moduleType : moduleTypeRegistered) {
            if (!moduleRegistry.isModuleConfigurable(moduleType)
                    || !moduleRegistry.isModuleEligibleToBuild(moduleType)) continue;

            ChromeSwitchPreference currentSwitch =
                    new ChromeSwitchPreference(getStyledContext(), null);
            currentSwitch.setKey(chromeHomeModulesConfigManager.getPreferenceKey(moduleType));
            currentSwitch.setTitle(getTitleForModuleType(moduleType));

            // Set up listeners and update the page.
            boolean isModuleTypeEnabled =
                    chromeHomeModulesConfigManager.getPrefModuleTypeEnabled(moduleType);
            currentSwitch.setChecked(isModuleTypeEnabled);
            currentSwitch.setOnPreferenceChangeListener(
                    (preference, newValue) -> {
                        chromeHomeModulesConfigManager.setPrefModuleTypeEnabled(
                                moduleType, (boolean) newValue);
                        return true;
                    });
            getPreferenceScreen().addPreference(currentSwitch);
        }
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    /** Returns the switch title of the module type. */
    private Integer getTitleForModuleType(@ModuleType int moduleType) {
        switch (moduleType) {
            case PRICE_CHANGE:
                return R.string.price_change_module_context_menu_item;
            default:
                assert false : "Module type not supported!";
                return null;
        }
    }
}
