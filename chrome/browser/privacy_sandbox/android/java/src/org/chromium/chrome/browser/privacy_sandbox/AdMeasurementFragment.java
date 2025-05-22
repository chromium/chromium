// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.preference.Preference;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ClickableSpansTextMessagePreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.SpanApplier;

/** Fragment for the Privacy Sandbox -> Ad Measurement preferences. */
@NullMarked
public class AdMeasurementFragment extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceChangeListener {
    public static final String TOGGLE_PREFERENCE = "ad_measurement_toggle";
    public static final String DISCLAIMER_PREFERENCE = "ad_measurement_page_disclaimer";
    public static final String CONSIDER_BULLET_THREE = "ad_measurement_consider_bullet_three";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    static boolean isAdMeasurementPrefEnabled(Profile profile) {
        PrefService prefService = UserPrefs.get(profile);
        return prefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED);
    }

    static void setAdMeasurementPrefEnabled(Profile profile, boolean isEnabled) {
        PrefService prefService = UserPrefs.get(profile);
        prefService.setBoolean(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED, isEnabled);
    }

    static boolean isAdMeasurementPrefManaged(Profile profile) {
        PrefService prefService = UserPrefs.get(profile);
        return prefService.isManagedPreference(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED);
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        mPageTitle.set(getString(R.string.settings_ad_measurement_page_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_measurement_preference);

        ChromeSwitchPreference adMeasurementToggle = findPreference(TOGGLE_PREFERENCE);
        adMeasurementToggle.setChecked(isAdMeasurementPrefEnabled(getProfile()));
        adMeasurementToggle.setOnPreferenceChangeListener(this);
        adMeasurementToggle.setManagedPreferenceDelegate(createManagedPreferenceDelegate());
        maybeApplyAdsApiUxEnhancements();
    }

    private void maybeApplyAdsApiUxEnhancements() {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)) {
            return;
        }
        ClickableSpansTextMessagePreference disclaimerPreference =
                findPreference(DISCLAIMER_PREFERENCE);
        disclaimerPreference.setVisible(true);
        disclaimerPreference.setSummary(
                SpanApplier.applySpans(
                        getResources()
                                .getString(R.string.settings_ad_measurement_page_disclaimer_clank),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ClickableSpan() {
                                    @Override
                                    public void onClick(View view) {
                                        onPrivacyPolicyLinkClicked();
                                    }
                                })));
        TextMessagePreference adMeasurementConsiderBullet3 = findPreference(CONSIDER_BULLET_THREE);
        adMeasurementConsiderBullet3.setDividerAllowedBelow(true);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object value) {
        if (preference.getKey().equals(TOGGLE_PREFERENCE)) {
            boolean enabled = (boolean) value;
            RecordUserAction.record(
                    enabled
                            ? "Settings.PrivacySandbox.AdMeasurement.Enabled"
                            : "Settings.PrivacySandbox.AdMeasurement.Disabled");
            setAdMeasurementPrefEnabled(getProfile(), enabled);
            return true;
        }
        return false;
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (TOGGLE_PREFERENCE.equals(preference.getKey())) {
                    return isAdMeasurementPrefManaged(getProfile());
                }
                return false;
            }
        };
    }

    private void onPrivacyPolicyLinkClicked() {
        RecordUserAction.record("Settings.PrivacySandbox.AdMeasurement.PrivacyPolicyLinkClicked");
        getCustomTabLauncher()
                .openUrlInCct(
                        getContext(),
                        getPrivacySandboxBridge().shouldUsePrivacyPolicyChinaDomain()
                                ? UrlConstants.GOOGLE_PRIVACY_POLICY_CHINA
                                : UrlConstants.GOOGLE_PRIVACY_POLICY);
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
