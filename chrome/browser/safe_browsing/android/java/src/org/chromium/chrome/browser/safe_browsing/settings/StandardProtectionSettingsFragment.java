// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Fragment containing standard protection settings.
 */
public class StandardProtectionSettingsFragment
        extends SafeBrowsingSettingsFragmentBase implements Preference.OnPreferenceChangeListener {
    @VisibleForTesting
    static final String PREF_EXTENDED_REPORTING = "extended_reporting";
    @VisibleForTesting
    static final String PREF_PASSWORD_LEAK_DETECTION = "password_leak_detection";

    public ChromeSwitchPreference mExtendedReportingPreference;
    public ChromeSwitchPreference mPasswordLeakDetectionPreference;

    private final ManagedPreferenceDelegate mManagedPreferenceDelegate =
            createManagedPreferenceDelegate();
    private final PrefService mPrefService = UserPrefs.get(Profile.getLastUsedRegularProfile());

    @Override
    protected void onCreatePreferencesInternal(Bundle bundle, String rootKey) {
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
        @SafeBrowsingState
        int safe_browsing_state = SafeBrowsingBridge.getSafeBrowsingState();
        boolean is_enhanced_protection =
                safe_browsing_state == SafeBrowsingState.ENHANCED_PROTECTION;
        boolean is_standard_protection =
                safe_browsing_state == SafeBrowsingState.STANDARD_PROTECTION;

        boolean extended_reporting_checked = is_enhanced_protection
                || (is_standard_protection
                        && SafeBrowsingBridge.isSafeBrowsingExtendedReportingEnabled());
        boolean extended_reporting_disabled_by_delegate =
                mManagedPreferenceDelegate.isPreferenceClickDisabledByPolicy(
                        mExtendedReportingPreference);
        mExtendedReportingPreference.setEnabled(
                is_standard_protection && !extended_reporting_disabled_by_delegate);
        mExtendedReportingPreference.setChecked(extended_reporting_checked);

        boolean has_token_for_leak_check = SafeBrowsingBridge.hasAccountForLeakCheckRequest()
                || SafeBrowsingBridge.isLeakDetectionUnauthenticatedEnabled();
        boolean leak_detection_enabled =
                mPrefService.getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED);
        boolean leak_detection_disabled_by_delegate =
                mManagedPreferenceDelegate.isPreferenceClickDisabledByPolicy(
                        mPasswordLeakDetectionPreference);
        boolean should_leak_detection_checked =
                is_enhanced_protection || (is_standard_protection && leak_detection_enabled);
        // Leak detection should not be checked if there is no available account (and leak detection
        // for signed out users is disabled), even if the feature is enabled.
        boolean leak_detection_checked = should_leak_detection_checked && has_token_for_leak_check;
        mPasswordLeakDetectionPreference.setEnabled(is_standard_protection
                && has_token_for_leak_check && !leak_detection_disabled_by_delegate);
        mPasswordLeakDetectionPreference.setChecked(leak_detection_checked);
        // If leak detection should be checked but not checked due to lack of account or
        // unauthenticated leak detection feature being disabled, show a message in the preference
        // summary.
        if (should_leak_detection_checked && !has_token_for_leak_check) {
            mPasswordLeakDetectionPreference.setSummary(
                    R.string.passwords_leak_detection_switch_signed_out_enable_description);
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_EXTENDED_REPORTING.equals(key)) {
            SafeBrowsingBridge.setSafeBrowsingExtendedReportingEnabled((boolean) newValue);
        } else if (PREF_PASSWORD_LEAK_DETECTION.equals(key)) {
            mPrefService.setBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED, (boolean) newValue);
        } else {
            assert false : "Should not be reached";
        }
        return true;
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            String key = preference.getKey();
            if (PREF_EXTENDED_REPORTING.equals(key)) {
                return SafeBrowsingBridge.isSafeBrowsingExtendedReportingManaged();
            } else if (PREF_PASSWORD_LEAK_DETECTION.equals(key)) {
                return mPrefService.isManagedPreference(Pref.PASSWORD_LEAK_DETECTION_ENABLED);
            } else {
                assert false : "Should not be reached";
            }
            return false;
        };
    }
}
