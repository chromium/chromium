// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Fragment containing Safety hub. */
public class SafetyHubFragment extends ChromeBaseSettingsFragment
        implements SafetyHubModuleDelegate {
    private static final String PREF_PASSWORDS = "passwords_account";

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_preferences);
        getActivity().setTitle(R.string.prefs_safety_check);

        Preference passwordCheckPreference = findPreference(PREF_PASSWORDS);
        int compromisedPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);

        PropertyModel passwordCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.ICON, R.drawable.ic_vpn_key_grey)
                        .with(
                                SafetyHubModuleProperties.IS_VISIBLE,
                                shouldShowPasswordCheckModule(getProfile()))
                        .with(
                                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                                compromisedPasswordsCount)
                        .build();

        PropertyModelChangeProcessor.create(
                passwordCheckPropertyModel,
                passwordCheckPreference,
                SafetyHubModuleViewBinder::bindPasswordCheckProperties);
    }

    @Override
    public boolean shouldShowPasswordCheckModule(Profile profile) {
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(profile);
        return PasswordManagerHelper.hasChosenToSyncPasswords(syncService)
                && passwordManagerHelper.canUseUpm();
    }
}
