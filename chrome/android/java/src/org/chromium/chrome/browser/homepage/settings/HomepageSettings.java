// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage.settings;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.homepage.settings.RadioButtonGroupHomepagePreference.HomepageOption;
import org.chromium.chrome.browser.homepage.settings.RadioButtonGroupHomepagePreference.PreferenceValues;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.url_formatter.UrlFormatter;

/**
 * Fragment that allows the user to configure homepage related preferences.
 */
public class HomepageSettings extends PreferenceFragmentCompat {
    @VisibleForTesting
    public static final String PREF_HOMEPAGE_SWITCH = "homepage_switch";
    @VisibleForTesting
    public static final String PREF_HOMEPAGE_EDIT = "homepage_edit";
    @VisibleForTesting
    public static final String PREF_HOMEPAGE_RADIO_GROUP = "homepage_radio_group";
    @VisibleForTesting
    public static final String PREF_TEXT_MANAGED = "text_managed";

    /**
     * Delegate used to mark that the homepage is being managed.
     * Created for {@link org.chromium.chrome.browser.settings.HomepagePreferences}
     */
    private static class HomepageManagedPreferenceDelegate
            implements ChromeManagedPreferenceDelegate {
        @Override
        public boolean isPreferenceControlledByPolicy(Preference preference) {
            return HomepagePolicyManager.isHomepageManagedByPolicy();
        }
    }

    private HomepageManager mHomepageManager;
    private Preference mHomepageEdit;
    private RadioButtonGroupHomepagePreference mRadioButtons;
    private TextMessagePreference mTextManaged;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mHomepageManager = HomepageManager.getInstance();

        getActivity().setTitle(R.string.options_homepage_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.homepage_preferences);

        HomepageManagedPreferenceDelegate managedDelegate = new HomepageManagedPreferenceDelegate();
        // Set up preferences inside the activity.
        ChromeSwitchPreference homepageSwitch =
                (ChromeSwitchPreference) findPreference(PREF_HOMEPAGE_SWITCH);
        homepageSwitch.setManagedPreferenceDelegate(managedDelegate);

        mHomepageEdit = findPreference(PREF_HOMEPAGE_EDIT);
        mTextManaged = (TextMessagePreference) findPreference(PREF_TEXT_MANAGED);
        mRadioButtons =
                (RadioButtonGroupHomepagePreference) findPreference(PREF_HOMEPAGE_RADIO_GROUP);

        mTextManaged.setManagedPreferenceDelegate(managedDelegate);
        setupPreferenceVisibility();

        // Set up listeners and update the page.
        boolean isHomepageEnabled = HomepageManager.isHomepageEnabled();
        homepageSwitch.setChecked(isHomepageEnabled);
        homepageSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            onSwitchPreferenceChange((boolean) newValue);
            return true;
        });

        RecordUserAction.record("Settings.Homepage.Opened");

        // Update preference views and state.
        updatePreferenceState();
    }

    /**
     * Set up the visibility for HomepageSettings based on {@link
     * ChromeFeatureList#HOMEPAGE_SETTINGS_UI_CONVERSION}.
     * TODO(wenyufu): If the feature flag is removed in the future, remove the unused component and
     * delete this function.
     */
    private void setupPreferenceVisibility() {
        boolean useNewUI = isHomepageSettingsUIConversionEnabled();

        mHomepageEdit.setVisible(!useNewUI);

        mRadioButtons.setVisible(useNewUI);
    }

    /**
     * Update the visibility when pref values or policy state changes.
     */
    private void updatePreferenceState() {
        boolean isManagedByPolicy = HomepagePolicyManager.isHomepageManagedByPolicy();
        mTextManaged.setVisible(false);

        if (isHomepageSettingsUIConversionEnabled()) {
            if (mRadioButtons != null) {
                mRadioButtons.setupPreferenceValues(createPreferenceValuesForRadioGroup());
            }
        } else {
            mHomepageEdit.setEnabled(!isManagedByPolicy && HomepageManager.isHomepageEnabled());
            mHomepageEdit.setSummary(mHomepageManager.getHomepageUriIgnoringEnabledState());
        }
    }

    private boolean isHomepageSettingsUIConversionEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.HOMEPAGE_SETTINGS_UI_CONVERSION);
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferenceState();
    }

    @Override
    public void onStop() {
        super.onStop();

        // Save the final shared preference data.
        if (isHomepageSettingsUIConversionEnabled()) {
            updateHomepageFromRadioGroupPreference(mRadioButtons.getPreferenceValue());
        }
    }

    /**
     * Handle the preference changes when we toggled the homepage switch.
     * @param isChecked Whether switch is turned on.
     */
    private void onSwitchPreferenceChange(boolean isChecked) {
        mHomepageManager.setPrefHomepageEnabled(isChecked);
        updatePreferenceState();
    }

    /**
     * Will be called when the status of {@link #mRadioButtons} is changed.
     * @param newValue The {@link PreferenceValues} that the {@link
     *         RadioButtonGroupHomepagePreference} is holding.
     */
    private void updateHomepageFromRadioGroupPreference(PreferenceValues newValue) {
        // When the preference is changed by code during initialization due to policy, ignore the
        // changes of the preference.
        if (HomepagePolicyManager.isHomepageManagedByPolicy()) return;

        boolean setToUseNTP = newValue.getCheckedOption() == HomepageOption.ENTRY_CHROME_NTP;
        String newHomepage = UrlFormatter.fixupUrl(newValue.getCustomURI()).getValidSpecOrEmpty();
        boolean useDefaultUri = HomepageManager.getDefaultHomepageUri().equals(newHomepage);

        mHomepageManager.setHomepagePreferences(setToUseNTP, useDefaultUri, newHomepage);
    }

    /**
     * @return The user customized homepage setting.
     */
    private String getHomepageForEditText() {
        if (HomepagePolicyManager.isHomepageManagedByPolicy()) {
            return HomepagePolicyManager.getHomepageUrl();
        }

        String defaultUrl = HomepageManager.getDefaultHomepageUri();
        String customUrl = mHomepageManager.getPrefHomepageCustomUri();
        if (mHomepageManager.getPrefHomepageUseDefaultUri()) {
            return UrlUtilities.isNTPUrl(defaultUrl) ? "" : defaultUrl;
        }

        if (TextUtils.isEmpty(customUrl) && !UrlUtilities.isNTPUrl(defaultUrl)) {
            return defaultUrl;
        }

        return customUrl;
    }

    private PreferenceValues createPreferenceValuesForRadioGroup() {
        boolean isPolicyEnabled = HomepagePolicyManager.isHomepageManagedByPolicy();

        // Check if the NTP button should be checked.
        // Note it is not always checked when homepage is NTP. When user customized homepage is NTP
        // URL, we don't check Chrome's Homepage radio button.
        boolean shouldCheckNTP;
        if (isPolicyEnabled) {
            shouldCheckNTP = UrlUtilities.isNTPUrl(HomepagePolicyManager.getHomepageUrl());
        } else {
            shouldCheckNTP = mHomepageManager.getPrefHomepageUseChromeNTP()
                    || (mHomepageManager.getPrefHomepageUseDefaultUri()
                            && UrlUtilities.isNTPUrl(HomepageManager.getDefaultHomepageUri()));
        }

        @HomepageOption
        int checkedOption =
                shouldCheckNTP ? HomepageOption.ENTRY_CHROME_NTP : HomepageOption.ENTRY_CUSTOM_URI;

        boolean isRadioButtonPreferenceEnabled =
                !isPolicyEnabled && HomepageManager.isHomepageEnabled();

        // NTP should be visible when policy is not enforced or the option is checked.
        boolean isNTPOptionVisible = !isPolicyEnabled || shouldCheckNTP;

        // Customized option should be visible when policy is not enforced or the option is checked.
        boolean isCustomizedOptionVisible = !isPolicyEnabled || !shouldCheckNTP;

        return new PreferenceValues(checkedOption, getHomepageForEditText(),
                isRadioButtonPreferenceEnabled, isNTPOptionVisible, isCustomizedOptionVisible);
    }
}
