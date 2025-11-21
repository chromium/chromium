// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.about_settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.text.format.DateUtils;

import androidx.preference.Preference;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.BaseSearchIndexProvider;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.util.date.CalendarFactory;
import org.chromium.ui.widget.Toast;

import java.util.Calendar;

/** Settings fragment that displays information about Chrome. */
@NullMarked
public class AboutChromeSettings extends ChromeBaseSettingsFragment
        implements EmbeddableSettingsPage, Preference.OnPreferenceClickListener {
    static {
        CalendarFactory.warmUp();
    }

    private static final int TAPS_FOR_DEVELOPER_SETTINGS = 7;

    private static final String PREF_APPLICATION_VERSION = "application_version";
    private static final String PREF_OS_VERSION = "os_version";
    private static final String PREF_LEGAL_INFORMATION = "legal_information";

    // Non-translated strings:
    @SuppressWarnings("InlineFormatString")
    private static final String MSG_DEVELOPER_ENABLE_COUNTDOWN =
            "%s more taps to enable Developer options.";

    private static final String MSG_DEVELOPER_ENABLE_COUNTDOWN_LAST_TAP =
            "1 more tap to enable Developer options.";
    private static final String MSG_DEVELOPER_ENABLED = "Developer options are now enabled.";
    private static final String MSG_DEVELOPER_ALREADY_ENABLED =
            "Developer options are already enabled.";

    private int mDeveloperHitCountdown =
            DeveloperSettings.shouldShowDeveloperSettings() ? -1 : TAPS_FOR_DEVELOPER_SETTINGS;
    private @Nullable Toast mToast;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        mPageTitle.set(getString(R.string.prefs_about_chrome));
        SettingsUtils.addPreferencesFromResource(this, R.xml.about_chrome_preferences);

        Preference p = findPreference(PREF_APPLICATION_VERSION);
        assumeNonNull(p);
        p.setSummary(
                getApplicationVersion(getActivity(), AboutSettingsBridge.getApplicationVersion()));
        p.setOnPreferenceClickListener(this);
        p = findPreference(PREF_OS_VERSION);
        assumeNonNull(p);
        p.setSummary(AboutSettingsBridge.getOSVersion());
        p = findPreference(PREF_LEGAL_INFORMATION);
        assumeNonNull(p);
        int currentYear = CalendarFactory.get().get(Calendar.YEAR);
        p.setSummary(getString(R.string.legal_information_summary, currentYear));
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    /**
     * Build the application version to be shown. In particular, this ensures the debug build
     * versions are more useful.
     */
    public static String getApplicationVersion(Context context, String version) {
        if (VersionInfo.isOfficialBuild()) {
            return version;
        }

        // For developer builds, show how recently the app was installed/updated.
        PackageInfo info;
        try {
            info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
        } catch (NameNotFoundException e) {
            return version;
        }
        CharSequence updateTimeString =
                DateUtils.getRelativeTimeSpanString(
                        info.lastUpdateTime, System.currentTimeMillis(), 0);
        return context.getString(R.string.version_with_update_time, version, updateTimeString);
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (mDeveloperHitCountdown > 0) {
            mDeveloperHitCountdown--;

            if (mDeveloperHitCountdown == 0) {
                DeveloperSettings.setDeveloperSettingsEnabled();

                // Show a toast that the developer settings were enabled.
                if (mToast != null) {
                    mToast.cancel();
                }
                mToast = Toast.makeText(getActivity(), MSG_DEVELOPER_ENABLED, Toast.LENGTH_LONG);
                mToast.show();
            } else if (mDeveloperHitCountdown > 0
                    && mDeveloperHitCountdown < (TAPS_FOR_DEVELOPER_SETTINGS - 2)) {
                // Show a countdown toast.
                if (mToast != null) {
                    mToast.cancel();
                }
                String title;
                if (mDeveloperHitCountdown == 1) {
                    title = MSG_DEVELOPER_ENABLE_COUNTDOWN_LAST_TAP;
                } else {
                    title = String.format(MSG_DEVELOPER_ENABLE_COUNTDOWN, mDeveloperHitCountdown);
                }
                mToast = Toast.makeText(getActivity(), title, Toast.LENGTH_SHORT);
                mToast.show();
            }
        } else if (mDeveloperHitCountdown < 0) {
            // Show a toast that the developer settings are already enabled.
            if (mToast != null) {
                mToast.cancel();
            }
            mToast =
                    Toast.makeText(getActivity(), MSG_DEVELOPER_ALREADY_ENABLED, Toast.LENGTH_LONG);
            mToast.show();
        }
        return true;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "about_chrome";
    }

    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider(
                    AboutChromeSettings.class.getName(), R.xml.about_chrome_preferences);
}
