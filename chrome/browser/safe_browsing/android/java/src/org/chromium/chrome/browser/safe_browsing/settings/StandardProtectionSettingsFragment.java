// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;

/** Fragment containing standard protection settings. */
@NullMarked
public class StandardProtectionSettingsFragment extends SafeBrowsingSettingsFragmentBase
        implements Preference.OnPreferenceChangeListener {
    @VisibleForTesting static final String PREF_SUBTITLE = "subtitle";
    @VisibleForTesting static final String PREF_EXTENDED_REPORTING = "extended_reporting";

    public ChromeSwitchPreference mExtendedReportingPreference;

    private ManagedPreferenceDelegate mManagedPreferenceDelegate;

    @Initializer
    @Override
    protected void onCreatePreferencesInternal(@Nullable Bundle bundle, @Nullable String rootKey) {
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();

        mExtendedReportingPreference = findPreference(PREF_EXTENDED_REPORTING);
        mExtendedReportingPreference.setOnPreferenceChangeListener(this);
        mExtendedReportingPreference.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        updateExtendedReportingPreferences();
    }

    @Override
    protected int getPreferenceResource() {
        return R.xml.standard_protection_preferences;
    }

    /**
     * Update the appearance of the preference under this fragment. The setEnabled function sets
     * whether the toggle is clickable. The setChecked function sets whether the toggle is currently
     * shown as checked. Note that the preference under standard protection fragment are only
     * clickable if the current Safe Browsing state is STANDARD_PROTECTION, because they should be
     * forced enabled in ENHANCED_PROTECTION mode and forced disabled in NO_SAFE_BROWSING mode.
     */
    private void updateExtendedReportingPreferences() {
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
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_EXTENDED_REPORTING.equals(key)) {
            getSafeBrowsingBridge().setSafeBrowsingExtendedReportingEnabled((boolean) newValue);
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
                } else {
                    assert false : "Should not be reached";
                }
                return false;
            }
        };
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
