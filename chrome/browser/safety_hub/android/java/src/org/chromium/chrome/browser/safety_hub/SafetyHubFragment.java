// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Fragment containing Safety hub. */
public class SafetyHubFragment extends ChromeBaseSettingsFragment
        implements FragmentSettingsLauncher {
    private static final String PREF_PASSWORDS = "passwords_account";
    private static final String PREF_UPDATE = "update_check";
    private static final String PREF_UNUSED_PERMISSIONS = "permissions";
    private SafetyHubModuleDelegate mDelegate;
    private SettingsLauncher mSettingsLauncher;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_preferences);
        getActivity().setTitle(R.string.prefs_safety_check);

        // Set up account-level password check preference.
        Preference passwordCheckPreference = findPreference(PREF_PASSWORDS);
        int compromisedPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);

        PropertyModel passwordCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(
                                SafetyHubModuleProperties.IS_VISIBLE,
                                mDelegate.shouldShowPasswordCheckModule())
                        .with(
                                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                                compromisedPasswordsCount)
                        .with(
                                SafetyHubModuleProperties.ON_CLICK_LISTENER,
                                () -> mDelegate.showPasswordCheckUI(getContext()))
                        .build();

        PropertyModelChangeProcessor.create(
                passwordCheckPropertyModel,
                passwordCheckPreference,
                SafetyHubModuleViewBinder::bindPasswordCheckProperties);

        // Set up update check preference.
        Preference updateCheckPreference = findPreference(PREF_UPDATE);

        PropertyModel updateCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.UPDATE_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(SafetyHubModuleProperties.UPDATE_STATUS, mDelegate.getUpdateStatus())
                        .build();

        PropertyModelChangeProcessor.create(
                updateCheckPropertyModel,
                updateCheckPreference,
                SafetyHubModuleViewBinder::bindUpdateCheckProperties);

        // Set up permissions preference.
        Preference permissionsPreference = findPreference(PREF_UNUSED_PERMISSIONS);
        int sitesWithUnusedPermissionsCount =
                UnusedSitePermissionsBridge.getRevokedPermissions(getProfile()).length;

        PropertyModel permissionsPropertyModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.PERMISSIONS_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                                sitesWithUnusedPermissionsCount)
                        .with(
                                SafetyHubModuleProperties.ON_CLICK_LISTENER,
                                () ->
                                        mSettingsLauncher.launchSettingsActivity(
                                                getContext(), SafetyHubPermissionsFragment.class))
                        .build();

        PropertyModelChangeProcessor.create(
                permissionsPropertyModel,
                permissionsPreference,
                SafetyHubModuleViewBinder::bindPermissionsProperties);
    }

    public void setDelegate(SafetyHubModuleDelegate safetyHubModuleDelegate) {
        mDelegate = safetyHubModuleDelegate;
    }

    @Override
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }
}
