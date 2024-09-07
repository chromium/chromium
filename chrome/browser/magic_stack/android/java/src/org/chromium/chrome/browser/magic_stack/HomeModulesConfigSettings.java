// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.EDUCATIONAL_TIP;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SAFETY_HUB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.TAB_RESUMPTION;

import android.content.Context;
import android.content.res.Resources;
import android.os.Bundle;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

import java.util.List;

/** Fragment that allows the user to configure chrome home modules related preferences. */
public class HomeModulesConfigSettings extends ChromeBaseSettingsFragment {
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mPageTitle.set(getString(R.string.home_modules_configuration));
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getStyledContext()));
        HomeModulesConfigManager homeModulesConfigManager = HomeModulesConfigManager.getInstance();

        List<Integer> moduleTypeShownInSettings =
                homeModulesConfigManager.getModuleListShownInSettings();

        boolean isTabModuleAdded = false;
        for (@ModuleType int moduleType : moduleTypeShownInSettings) {
            if (moduleType == SINGLE_TAB || moduleType == TAB_RESUMPTION) {
                // The SINGLE_TAB and TAB_RESUMPTION modules are controlled by the same preference.
                if (isTabModuleAdded) continue;

                isTabModuleAdded = true;
            }

            ChromeSwitchPreference currentSwitch =
                    new ChromeSwitchPreference(getStyledContext(), null);
            currentSwitch.setKey(homeModulesConfigManager.getSettingsPreferenceKey(moduleType));
            currentSwitch.setTitle(getTitleForModuleType(moduleType));

            // Set up listeners and update the page.
            boolean isModuleTypeEnabled =
                    homeModulesConfigManager.getPrefModuleTypeEnabled(moduleType);
            currentSwitch.setChecked(isModuleTypeEnabled);
            currentSwitch.setOnPreferenceChangeListener(
                    (preference, newValue) -> {
                        homeModulesConfigManager.setPrefModuleTypeEnabled(
                                moduleType, (boolean) newValue);
                        HomeModulesMetricsUtils.recordModuleToggledInConfiguration(
                                moduleType, (boolean) newValue);

                        return true;
                    });
            getPreferenceScreen().addPreference(currentSwitch);
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    /** Returns the string of switch title for the module type. */
    private String getTitleForModuleType(@ModuleType int moduleType) {
        Resources resources = getResources();
        switch (moduleType) {
            case SINGLE_TAB:
            case TAB_RESUMPTION:
                return resources.getQuantityString(R.plurals.home_modules_tab_resumption_title, 1);
            case PRICE_CHANGE:
                return resources.getString(R.string.price_change_module_name);
            case SAFETY_HUB:
                return resources.getString(R.string.safety_hub_magic_stack_module_name);
            case EDUCATIONAL_TIP:
                return resources.getString(R.string.educational_tip_module_name);
            default:
                assert false : "Module type not supported!";
                return null;
        }
    }

    boolean isHomeModulesConfigSettingsEmptyForTesting() {
        return getPreferenceScreen().getPreferenceCount() == 0;
    }
}
