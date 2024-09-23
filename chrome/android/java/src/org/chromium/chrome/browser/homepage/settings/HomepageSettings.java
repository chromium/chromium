// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage.settings;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.homepage.settings.RadioButtonGroupHomepagePreference.HomepageOption;
import org.chromium.chrome.browser.homepage.settings.RadioButtonGroupHomepagePreference.PreferenceValues;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/** Fragment that allows the user to configure homepage related preferences. */
public class HomepageSettings extends ChromeBaseSettingsFragment {
    @VisibleForTesting public static final String PREF_HOMEPAGE_SWITCH = "homepage_switch";

    @VisibleForTesting
    public static final String PREF_HOMEPAGE_RADIO_GROUP = "homepage_radio_group";

    private HomepageManager mHomepageManager;
    private RadioButtonGroupHomepagePreference mRadioButtons;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mHomepageManager = HomepageManager.getInstance();

        mPageTitle.set(getString(R.string.options_homepage_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.homepage_preferences);

        // Set up preferences inside the activity.
        ChromeSwitchPreference homepageSwitch =
                (ChromeSwitchPreference) findPreference(PREF_HOMEPAGE_SWITCH);
        homepageSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return HomepagePolicyManager.isHomepageManagedByPolicy();
                    }
                });

        mRadioButtons =
                (RadioButtonGroupHomepagePreference) findPreference(PREF_HOMEPAGE_RADIO_GROUP);

        // Set up listeners and update the page.
        boolean isHomepageEnabled = mHomepageManager.isHomepageEnabled();
        homepageSwitch.setChecked(isHomepageEnabled);
        homepageSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    onSwitchPreferenceChange((boolean) newValue);
                    return true;
                });
        mRadioButtons.setupPreferenceValues(createPreferenceValuesForRadioGroup());

        RecordUserAction.record("Settings.Homepage.Opened");
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onResume() {
        super.onResume();
        // If view created, update the state for pref values or policy state changes.
        if (mRadioButtons != null) {
            mRadioButtons.setupPreferenceValues(createPreferenceValuesForRadioGroup());
        }
    }

    @Override
    public void onStop() {
        super.onStop();

        // Save the final shared preference data.
        updateHomepageFromRadioGroupPreference(mRadioButtons.getPreferenceValue());
    }

    /**
     * Handle the preference changes when we toggled the homepage switch.
     * @param isChecked Whether switch is turned on.
     */
    private void onSwitchPreferenceChange(boolean isChecked) {
        mHomepageManager.setPrefHomepageEnabled(isChecked);
        mRadioButtons.setupPreferenceValues(createPreferenceValuesForRadioGroup());
    }

    /**
     * Will be called when the status of {@link #mRadioButtons} is changed.
     * TODO(crbug.com/40148533): Record changes whenever user changes settings rather than homepage
     * settings is stopped.
     *
     * @param newValue The {@link PreferenceValues} that the {@link
     *     RadioButtonGroupHomepagePreference} is holding.
     */
    private void updateHomepageFromRadioGroupPreference(PreferenceValues newValue) {
        // When the preference is changed by code during initialization due to policy, ignore the
        // changes of the preference.
        if (HomepagePolicyManager.isHomepageManagedByPolicy()) return;

        boolean setToUseNtp = newValue.getCheckedOption() == HomepageOption.ENTRY_CHROME_NTP;
        GURL newHomepage = UrlFormatter.fixupUrl(newValue.getCustomURI());
        if (!newHomepage.isValid()) {
            newHomepage = GURL.emptyGURL();
        }
        boolean useDefaultUri = mHomepageManager.getDefaultHomepageGurl().equals(newHomepage);

        mHomepageManager.setHomepagePreferences(setToUseNtp, useDefaultUri, newHomepage);
    }

    /**
     * @return The user customized homepage setting.
     */
    private GURL getHomepageForEditText() {
        if (HomepagePolicyManager.isHomepageManagedByPolicy()) {
            return HomepagePolicyManager.getHomepageUrl();
        }

        GURL defaultGurl = mHomepageManager.getDefaultHomepageGurl();
        GURL customGurl = mHomepageManager.getPrefHomepageCustomGurl();
        if (mHomepageManager.getPrefHomepageUseDefaultUri()) {
            return UrlUtilities.isNtpUrl(defaultGurl) ? GURL.emptyGURL() : defaultGurl;
        }

        if (customGurl.isEmpty() && !UrlUtilities.isNtpUrl(defaultGurl)) {
            return defaultGurl;
        }

        return customGurl;
    }

    private PreferenceValues createPreferenceValuesForRadioGroup() {
        boolean isPolicyEnabled = HomepagePolicyManager.isHomepageManagedByPolicy();

        // Check if the NTP button should be checked.
        // Note it is not always checked when homepage is NTP. When user customized homepage is NTP
        // URL, we don't check Chrome's Homepage radio button.
        boolean shouldCheckNtp;
        if (isPolicyEnabled) {
            shouldCheckNtp = UrlUtilities.isNtpUrl(HomepagePolicyManager.getHomepageUrl());
        } else {
            shouldCheckNtp =
                    mHomepageManager.getPrefHomepageUseChromeNtp()
                            || (mHomepageManager.getPrefHomepageUseDefaultUri()
                                    && UrlUtilities.isNtpUrl(
                                            mHomepageManager.getDefaultHomepageGurl()));
        }

        @HomepageOption
        int checkedOption =
                shouldCheckNtp ? HomepageOption.ENTRY_CHROME_NTP : HomepageOption.ENTRY_CUSTOM_URI;

        boolean isRadioButtonPreferenceEnabled =
                !isPolicyEnabled && mHomepageManager.isHomepageEnabled();

        // NTP should be visible when policy is not enforced or the option is checked.
        boolean isNtpOptionVisible = !isPolicyEnabled || shouldCheckNtp;

        // Customized option should be visible when policy is not enforced or the option is checked.
        boolean isCustomizedOptionVisible = !isPolicyEnabled || !shouldCheckNtp;

        return new PreferenceValues(
                checkedOption,
                getHomepageForEditText().getSpec(),
                isRadioButtonPreferenceEnabled,
                isNtpOptionVisible,
                isCustomizedOptionVisible);
    }
}
