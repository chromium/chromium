// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.secure_dns;

import android.content.Context;
import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.net.SecureDnsManagementMode;
import org.chromium.chrome.browser.privacy.secure_dns.SecureDnsProviderPreference.State;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.net.SecureDnsMode;

import java.util.List;

/**
 * Fragment to manage Secure DNS preference. It consists of a toggle switch and, if the switch is
 * enabled, a SecureDnsControl.
 */
public class SecureDnsSettings extends ChromeBaseSettingsFragment {
    // Must match keys in secure_dns_settings.xml.
    private static final String PREF_SECURE_DNS_SWITCH = "secure_dns_switch";
    private static final String PREF_SECURE_DNS_PROVIDER = "secure_dns_provider";

    private ChromeSwitchPreference mSecureDnsSwitch;
    private SecureDnsProviderPreference mSecureDnsProviderPreference;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    /**
     * @return A summary for use in the Preference that opens this fragment.
     */
    public static String getSummary(Context context) {
        @SecureDnsMode int mode = SecureDnsBridge.getMode();
        if (mode == SecureDnsMode.OFF) {
            return context.getString(R.string.text_off);
        } else if (mode == SecureDnsMode.AUTOMATIC) {
            return context.getString(R.string.settings_automatic_mode_summary);
        } else {
            String config = SecureDnsBridge.getConfig();
            List<SecureDnsBridge.Entry> providers = SecureDnsBridge.getProviders();
            String serverName = config;
            for (int i = 0; i < providers.size(); i++) {
                SecureDnsBridge.Entry entry = providers.get(i);
                if (entry.config.equals(config)) {
                    serverName = entry.name;
                    break;
                }
            }
            return String.format("%s - %s", context.getString(R.string.text_on), serverName);
        }
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mPageTitle.set(getString(R.string.settings_secure_dns_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.secure_dns_settings);

        // Set up preferences inside the activity.
        mSecureDnsSwitch = (ChromeSwitchPreference) findPreference(PREF_SECURE_DNS_SWITCH);
        mSecureDnsSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return SecureDnsBridge.isModeManaged();
                    }
                });
        mSecureDnsSwitch.setOnPreferenceChangeListener(
                (preference, enabled) -> {
                    storePreferenceState(
                            (boolean) enabled, mSecureDnsProviderPreference.getState());
                    loadPreferenceState();
                    return true;
                });

        if (!SecureDnsBridge.isModeManaged()) {
            // If the mode isn't managed directly, we still need to disable the controls
            // if we detect a managed system configuration, or any parental control software.
            // However, we don't want to show the managed setting icon in this case, because the
            // setting is not directly controlled by a policy.
            @SecureDnsManagementMode int managementMode = SecureDnsBridge.getManagementMode();
            if (managementMode != SecureDnsManagementMode.NO_OVERRIDE) {
                mSecureDnsSwitch.setEnabled(false);
                boolean parentalControls =
                        managementMode == SecureDnsManagementMode.DISABLED_PARENTAL_CONTROLS;
                mSecureDnsSwitch.setSummaryOff(
                        parentalControls
                                ? R.string.settings_secure_dns_disabled_for_parental_control
                                : R.string.settings_secure_dns_disabled_for_managed_environment);
            }
        }

        mSecureDnsProviderPreference =
                (SecureDnsProviderPreference) findPreference(PREF_SECURE_DNS_PROVIDER);
        mSecureDnsProviderPreference.setOnPreferenceChangeListener(
                (preference, value) -> {
                    State controlState = (State) value;
                    boolean valid =
                            storePreferenceState(mSecureDnsSwitch.isChecked(), controlState);
                    if (valid != controlState.valid) {
                        mSecureDnsProviderPreference.setState(controlState.withValid(valid));
                        // Cancel the change to controlState.
                        return false;
                    }
                    return true;
                });

        // Update preference views and state.
        loadPreferenceState();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    /**
     * @param enabled Whether the toggle switch is enabled
     * @param controlState The state from SecureDnsControl.
     * @return True if the state was successfully stored.
     */
    private boolean storePreferenceState(boolean enabled, State controlState) {
        if (!enabled) {
            SecureDnsBridge.setMode(SecureDnsMode.OFF);
            SecureDnsBridge.setConfig("");
        } else if (!controlState.secure) {
            SecureDnsBridge.setMode(SecureDnsMode.AUTOMATIC);
            SecureDnsBridge.setConfig("");
        } else {
            if (controlState.config.isEmpty() || !SecureDnsBridge.setConfig(controlState.config)) {
                return false;
            }
            SecureDnsBridge.setMode(SecureDnsMode.SECURE);
        }
        return true;
    }

    private void loadPreferenceState() {
        @SecureDnsMode int mode = SecureDnsBridge.getMode();
        boolean enabled = mode != SecureDnsMode.OFF;
        boolean enforced =
                SecureDnsBridge.isModeManaged()
                        || SecureDnsBridge.getManagementMode()
                                != SecureDnsManagementMode.NO_OVERRIDE;
        mSecureDnsSwitch.setChecked(enabled);
        mSecureDnsProviderPreference.setEnabled(enabled && !enforced);

        boolean secure = mode == SecureDnsMode.SECURE;
        String config = SecureDnsBridge.getConfig();
        boolean valid = true; // States loaded from storage are presumed valid.
        mSecureDnsProviderPreference.setState(new State(secure, config, valid));
    }

    @Override
    public void onResume() {
        super.onResume();
        loadPreferenceState();
    }
}
