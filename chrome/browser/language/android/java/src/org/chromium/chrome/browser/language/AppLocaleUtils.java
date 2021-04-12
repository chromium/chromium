// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.content.Context;
import android.preference.PreferenceManager;
import android.text.TextUtils;

import org.chromium.base.BundleUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Provides utility functions to assist with overriding the application language.
 * This class manages the AppLanguagePref.
 */
public class AppLocaleUtils {
    private AppLocaleUtils(){};

    // Value of AppLocale preference when the system language is used.
    public static final String SYSTEM_LANGUAGE_VALUE = null;

    /**
     * Return true if languageName is the same as the current application override
     * language stored preference.
     * @return boolean
     */
    public static boolean isAppLanguagePref(String languageName) {
        return TextUtils.equals(getAppLanguagePref(), languageName);
    }

    /**
     * Get the value of application language shared preference or null if there is none.
     * @return String BCP-47 language tag (e.g. en-US).
     */
    public static String getAppLanguagePref() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, SYSTEM_LANGUAGE_VALUE);
    }

    /**
     * Get the value of application language shared preference or null if there is none.
     * Used during {@link ChromeApplication#attachBaseContext} before
     * {@link SharedPreferencesManager} is created.
     * @param base Context to use for getting the shared preference.
     * @return String BCP-47 language tag (e.g. en-US).
     */
    @SuppressWarnings("DefaultSharedPreferencesCheck")
    protected static String getAppLanguagePrefStartUp(Context base) {
        return PreferenceManager.getDefaultSharedPreferences(base).getString(
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, SYSTEM_LANGUAGE_VALUE);
    }

    /**
     * Set the application language shared preference and download the language split if needed. If
     * set to null the system language will be used.
     * @param languageName String BCP-47 code of language to download.
     */
    public static void setAppLanguagePref(String languageName) {
        setAppLanguagePref(languageName, success -> {});
    }

    /**
     * Set the application language shared preference and download the language split using the
     * provided listener for callbacks. If called from an APK build where no bundle needs to be
     * downloaded the listener's on complete function is immediately called, triggering the success
     * UI. If languageName is null the system language will be used.
     * @param languageName String BCP-47 code of language to download.
     * @param listener LanguageSplitInstaller.InstallListener to use for callbacks.
     */
    public static void setAppLanguagePref(
            String languageName, LanguageSplitInstaller.InstallListener listener) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, languageName);
        if (BundleUtils.isBundle() && !TextUtils.equals(languageName, SYSTEM_LANGUAGE_VALUE)) {
            LanguageSplitInstaller.getInstance().installLanguage(languageName, listener);
        } else {
            listener.onComplete(true);
        }
    }
}
