// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.AdvancedProtectionStatusManagerAndroidBridge;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;

/**
 * Fragment to manage HTTPS-First Mode preference. It consists of a toggle switch and, if the switch
 * is enabled, an HttpsFirstModeVariantPreference.
 */
@NullMarked
public class HttpsFirstModeSettingsFragment extends ChromeBaseSettingsFragment {
    // Must match keys in https_first_mode_settings.xml.
    @VisibleForTesting static final String PREF_HTTPS_FIRST_MODE_SWITCH = "https_first_mode_switch";

    @VisibleForTesting
    static final String PREF_HTTPS_FIRST_MODE_VARIANT = "https_first_mode_variant";

    @VisibleForTesting
    static final String PREF_ENFORCED_BY_ADVANCED_PROTECTION =
            "enforced_by_advanced_protection_warning";

    private ChromeSwitchPreference mHttpsFirstModeSwitch;
    private HttpsFirstModeVariantPreference mHttpsFirstModeVariantPreference;
    private HttpsFirstModeBridge mHttpsFirstModeBridge;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Initializer
    @Override
    public void setProfile(Profile profile) {
        super.setProfile(profile);
        mHttpsFirstModeBridge = new HttpsFirstModeBridge(profile);
    }

    /**
     * @return A summary for use in the Preference that opens this fragment.
     */
    public static String getSummary(Context context, Profile profile) {
        @HttpsFirstModeSetting int setting = new HttpsFirstModeBridge(profile).getCurrentSetting();
        switch (setting) {
            case HttpsFirstModeSetting.ENABLED_BALANCED:
                return context.getString(R.string.settings_https_first_mode_enabled_balanced_label);
            case HttpsFirstModeSetting.ENABLED_FULL:
                return context.getString(R.string.settings_https_first_mode_enabled_strict_label);
            case HttpsFirstModeSetting.DISABLED:
                // fall through
            default:
                return context.getString(R.string.settings_https_first_mode_disabled_label);
        }
    }

    /**
     * @return Whether the HTTPS-First Mode setting is enforced and not modifiable by the user.
     */
    public boolean isSettingEnforced() {
        return mHttpsFirstModeBridge.isManaged()
                || AdvancedProtectionStatusManagerAndroidBridge.isUnderAdvancedProtection();
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.settings_https_first_mode_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.https_first_mode_settings);

        // Set up the main toggle preference.
        mHttpsFirstModeSwitch = findPreference(PREF_HTTPS_FIRST_MODE_SWITCH);
        mHttpsFirstModeSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return mHttpsFirstModeBridge.isManaged();
                    }

                    @Override
                    public boolean isPreferenceClickDisabled(Preference preference) {
                        return isSettingEnforced();
                    }
                });
        mHttpsFirstModeSwitch.setOnPreferenceChangeListener(
                (preference, enabled) -> {
                    // When the toggle is turned on, default to Balanced mode.
                    mHttpsFirstModeBridge.updatePrefs(
                            (boolean) enabled
                                    ? HttpsFirstModeSetting.ENABLED_BALANCED
                                    : HttpsFirstModeSetting.DISABLED);
                    loadPreferenceState();
                    return true;
                });
        // Override switch description if the preference is force-enabled by Advanced Protection.
        if (AdvancedProtectionStatusManagerAndroidBridge.isUnderAdvancedProtection()) {
            mHttpsFirstModeSwitch.setSummary(
                    getString(
                            R.string
                                    .settings_https_first_mode_with_advanced_protection_description));
        }

        // Setup the sub-preference for controlling which mode of HFM is enabled.
        mHttpsFirstModeVariantPreference = findPreference(PREF_HTTPS_FIRST_MODE_VARIANT);
        mHttpsFirstModeVariantPreference.init(mHttpsFirstModeBridge.getCurrentSetting());
        mHttpsFirstModeVariantPreference.setOnPreferenceChangeListener(
                (preference, value) -> {
                    mHttpsFirstModeBridge.updatePrefs((int) value);
                    loadPreferenceState();
                    return true;
                });

        // Update preference views and state.
        loadPreferenceState();
    }

    private void loadPreferenceState() {
        @HttpsFirstModeSetting int setting = mHttpsFirstModeBridge.getCurrentSetting();
        boolean enabled = setting != HttpsFirstModeSetting.DISABLED;
        mHttpsFirstModeSwitch.setChecked(enabled);
        mHttpsFirstModeVariantPreference.setEnabled(enabled && !isSettingEnforced());
        mHttpsFirstModeVariantPreference.setCheckedState(setting);

        // If the setting is force-disabled by policy, don't show the radio
        // group.
        if (isSettingEnforced() && !enabled) {
            mHttpsFirstModeVariantPreference.setVisible(false);
        }

        maybeAddEnforcedByAdvancedProtectionWarning();
    }

    private void maybeAddEnforcedByAdvancedProtectionWarning() {
        var existingWarning = findPreference(PREF_ENFORCED_BY_ADVANCED_PROTECTION);
        if (existingWarning != null) {
            getPreferenceScreen().removePreference(existingWarning);
        }

        var additionalSecurityProvider = OsAdditionalSecurityPermissionUtil.getProviderInstance();
        if (!AdvancedProtectionStatusManagerAndroidBridge.isUnderAdvancedProtection()
                || additionalSecurityProvider == null) {
            return;
        }

        Context context = getPreferenceManager().getContext();
        ChromeBasePreference preference = new ChromeBasePreference(context, null);
        preference.setKey(PREF_ENFORCED_BY_ADVANCED_PROTECTION);
        int titleId =
                additionalSecurityProvider
                        .getHttpsFirstModeEnforcedByAndroidAdvancedProtectionWarningResourceId();
        if (titleId != 0) {
            preference.setTitle(context.getString(titleId));
        }
        int iconId = additionalSecurityProvider.getAdvancedProtectionIconResourceId();
        if (iconId != 0) {
            Drawable icon = ApiCompatibilityUtils.getDrawable(context.getResources(), iconId);
            icon.mutate();
            int disabledColor = SemanticColorUtils.getDefaultControlColorActive(context);
            icon.setColorFilter(disabledColor, PorterDuff.Mode.SRC_IN);
            preference.setIcon(icon);
        }
        getPreferenceScreen().addPreference(preference);
    }

    @Override
    public void onStart() {
        super.onStart();
        loadPreferenceState();
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
