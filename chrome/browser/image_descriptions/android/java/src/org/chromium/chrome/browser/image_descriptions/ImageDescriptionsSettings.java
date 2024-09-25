// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment for the "Image Descriptions" settings sub-page under Settings > Accessibility. This page
 * allows a user to control whether or not the feature is on, and whether or not it is allowed to
 * run on mobile data or requires a Wi-Fi connection.
 */
public class ImageDescriptionsSettings extends PreferenceFragmentCompat
        implements Preference.OnPreferenceChangeListener,
                CustomDividerFragment,
                EmbeddableSettingsPage,
                ProfileDependentSetting {
    public static final String IMAGE_DESCRIPTIONS = "image_descriptions_switch";
    public static final String IMAGE_DESCRIPTIONS_DATA_POLICY = "image_descriptions_data_policy";

    private ChromeSwitchPreference mGetImageDescriptionsSwitch;
    private RadioButtonGroupAccessibilityPreference mRadioButtonGroupAccessibilityPreference;

    private ImageDescriptionsControllerDelegate mDelegate;
    private boolean mIsEnabled;
    private boolean mOnlyOnWifi;
    private Profile mProfile;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mPageTitle.set(getString(R.string.image_descriptions_settings_title));
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public boolean hasDivider() {
        return false;
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.image_descriptions_preference);

        Bundle extras = getArguments();
        if (extras != null) {
            mIsEnabled = extras.getBoolean(IMAGE_DESCRIPTIONS);
            mOnlyOnWifi = extras.getBoolean(IMAGE_DESCRIPTIONS_DATA_POLICY);
        }

        mGetImageDescriptionsSwitch = (ChromeSwitchPreference) findPreference(IMAGE_DESCRIPTIONS);
        mGetImageDescriptionsSwitch.setOnPreferenceChangeListener(this);
        mGetImageDescriptionsSwitch.setChecked(mIsEnabled);

        mRadioButtonGroupAccessibilityPreference =
                (RadioButtonGroupAccessibilityPreference)
                        findPreference(IMAGE_DESCRIPTIONS_DATA_POLICY);
        mRadioButtonGroupAccessibilityPreference.setOnPreferenceChangeListener(this);
        mRadioButtonGroupAccessibilityPreference.setEnabled(mIsEnabled);
        mRadioButtonGroupAccessibilityPreference.initialize(mOnlyOnWifi);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (preference.getKey().equals(IMAGE_DESCRIPTIONS)) {
            boolean userHasEnabled = (boolean) newValue;

            // If userChoice is true, user has turned on descriptions, enable and set Wi-Fi pref.
            if (userHasEnabled) {
                mDelegate.enableImageDescriptions(mProfile);
                mDelegate.setOnlyOnWifiRequirement(
                        mRadioButtonGroupAccessibilityPreference.getOnlyOnWifiValue(), mProfile);

                // We enable the radio button group if get image descriptions is enabled.
                mRadioButtonGroupAccessibilityPreference.setEnabled(true);
            } else {
                mDelegate.disableImageDescriptions(mProfile);
                mRadioButtonGroupAccessibilityPreference.setEnabled(false);
            }

        } else if (preference.getKey().equals(IMAGE_DESCRIPTIONS_DATA_POLICY)) {
            mDelegate.setOnlyOnWifiRequirement((boolean) newValue, mProfile);
        }
        return true;
    }

    public void setDelegate(ImageDescriptionsControllerDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }
}
