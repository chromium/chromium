// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Fragment containing standard protection settings. */
public class StandardProtectionSettingsFragment extends SafeBrowsingSettingsFragmentBase
        implements Preference.OnPreferenceChangeListener {
    @VisibleForTesting static final String PREF_SUBTITLE = "subtitle";
    @VisibleForTesting static final String PREF_EXTENDED_REPORTING = "extended_reporting";
    @VisibleForTesting static final String PREF_PASSWORD_LEAK_DETECTION = "password_leak_detection";

    public ChromeSwitchPreference mExtendedReportingPreference;
    public ChromeSwitchPreference mPasswordLeakDetectionPreference;

    private ManagedPreferenceDelegate mManagedPreferenceDelegate;
    private PrefService mPrefService;

    @Override
    protected void onCreatePreferencesInternal(Bundle bundle, String rootKey) {
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();
        mPrefService = UserPrefs.get(getProfile());

        mExtendedReportingPreference = findPreference(PREF_EXTENDED_REPORTING);
        mExtendedReportingPreference.setOnPreferenceChangeListener(this);
        mExtendedReportingPreference.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mPasswordLeakDetectionPreference = findPreference(PREF_PASSWORD_LEAK_DETECTION);
        mPasswordLeakDetectionPreference.setOnPreferenceChangeListener(this);
        mPasswordLeakDetectionPreference.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        updateLeakDetectionAndExtendedReportingPreferences();
    }

    @Override
    protected int getPreferenceResource() {
        return R.xml.standard_protection_preferences;
    }

    /**
     * Update the appearance of the preferences under this fragment. The setEnabled function sets
     * whether the toggle is clickable. The setChecked function sets whether the toggle is currently
     * shown as checked. Note that the preferences under standard protection fragment are only
     * clickable if the current Safe Browsing state is STANDARD_PROTECTION, because they should be
     * forced enabled in ENHANCED_PROTECTION mode and forced disabled in NO_SAFE_BROWSING mode.
     */
    private void updateLeakDetectionAndExtendedReportingPreferences() {
        @SafeBrowsingState int safe_browsing_state = getSafeBrowsingBridge().getSafeBrowsingState();
        boolean is_enhanced_protection =
                safe_browsing_state == SafeBrowsingState.ENHANCED_PROTECTION;
        boolean is_standard_protection =
                safe_browsing_state == SafeBrowsingState.STANDARD_PROTECTION;

        boolean extended_reporting_checked =
                is_enhanced_protection
                        || (is_standard_protection
                                && getSafeBrowsingBridge()
                                        .isSafeBrowsingExtendedReportingEnabled());
        boolean extended_reporting_disabled_by_delegate =
                mManagedPreferenceDelegate.isPreferenceClickDisabled(mExtendedReportingPreference);
        mExtendedReportingPreference.setEnabled(
                is_standard_protection && !extended_reporting_disabled_by_delegate);
        mExtendedReportingPreference.setChecked(extended_reporting_checked);

        boolean leak_detection_enabled =
                mPrefService.getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED);
        boolean leak_detection_disabled_by_delegate =
                mManagedPreferenceDelegate.isPreferenceClickDisabled(
                        mPasswordLeakDetectionPreference);
        mPasswordLeakDetectionPreference.setEnabled(
                is_standard_protection && !leak_detection_disabled_by_delegate);
        mPasswordLeakDetectionPreference.setChecked(
                is_enhanced_protection || (is_standard_protection && leak_detection_enabled));
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_EXTENDED_REPORTING.equals(key)) {
            getSafeBrowsingBridge().setSafeBrowsingExtendedReportingEnabled((boolean) newValue);
        } else if (PREF_PASSWORD_LEAK_DETECTION.equals(key)) {
            mPrefService.setBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED, (boolean) newValue);
        } else {
            assert false : "Should not be reached";
        }
        return true;
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                String key = preference.getKey();
                if (PREF_EXTENDED_REPORTING.equals(key)) {
                    return getSafeBrowsingBridge().isSafeBrowsingExtendedReportingManaged();
                } else if (PREF_PASSWORD_LEAK_DETECTION.equals(key)) {
                    return mPrefService.isManagedPreference(Pref.PASSWORD_LEAK_DETECTION_ENABLED);
                } else {
                    assert false : "Should not be reached";
                }
                return false;
            }
        };
    }
}
