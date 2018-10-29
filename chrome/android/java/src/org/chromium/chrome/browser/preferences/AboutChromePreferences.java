// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.text.format.DateUtils;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.preferences.PrefServiceBridge.AboutVersionStrings;

import java.util.Calendar;

/**
 * Settings fragment that displays information about Chrome.
 */
public class AboutChromePreferences extends PreferenceFragment {

    private static final String PREF_APPLICATION_VERSION = "application_version";
    private static final String PREF_OS_VERSION = "os_version";
    private static final String PREF_LEGAL_INFORMATION = "legal_information";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getActivity().setTitle(R.string.prefs_about_chrome);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.about_chrome_preferences);

        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();
        AboutVersionStrings versionStrings = prefServiceBridge.getAboutVersionStrings();
        Preference p = findPreference(PREF_APPLICATION_VERSION);
        p.setSummary(getApplicationVersion(getActivity(), versionStrings.getApplicationVersion()));
        p = findPreference(PREF_OS_VERSION);
        p.setSummary(versionStrings.getOSVersion());
        p = findPreference(PREF_LEGAL_INFORMATION);
        int currentYear = Calendar.getInstance().get(Calendar.YEAR);
        p.setSummary(getString(R.string.legal_information_summary, currentYear));
    }

    /**
     * Build the application version to be shown.  In particular, this ensures the debug build
     * versions are more useful.
     */
    public static String getApplicationVersion(Context context, String version) {
        if (ChromeVersionInfo.isOfficialBuild()) {
            return version;
        }

        // For developer builds, show how recently the app was installed/updated.
        PackageInfo info;
        try {
            info = context.getPackageManager().getPackageInfo(
                    context.getPackageName(), 0);
        } catch (NameNotFoundException e) {
            return version;
        }
        CharSequence updateTimeString = DateUtils.getRelativeTimeSpanString(
                info.lastUpdateTime, System.currentTimeMillis(), 0);
        return context.getString(R.string.version_with_update_time, version,
                updateTimeString);
    }
}
