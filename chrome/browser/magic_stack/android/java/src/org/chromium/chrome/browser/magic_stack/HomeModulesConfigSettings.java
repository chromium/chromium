// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.getTitleForModuleType;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;

import java.util.List;

/** Fragment that allows the user to configure chrome home modules related preferences. */
@NullMarked
public class HomeModulesConfigSettings extends ChromeBaseSettingsFragment {
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.home_modules_configuration));
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getStyledContext()));
        HomeModulesConfigManager homeModulesConfigManager = HomeModulesConfigManager.getInstance();

        List<Integer> moduleTypeShownInSettings =
                homeModulesConfigManager.getModuleListShownInSettings();

        for (@ModuleType int moduleType : moduleTypeShownInSettings) {
            ChromeSwitchPreference currentSwitch =
                    new ChromeSwitchPreference(getStyledContext(), null);
            currentSwitch.setKey(homeModulesConfigManager.getSettingsPreferenceKey(moduleType));
            currentSwitch.setTitle(getTitleForModuleType(moduleType, getResources()));

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

    boolean isHomeModulesConfigSettingsEmptyForTesting() {
        return getPreferenceScreen().getPreferenceCount() == 0;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
