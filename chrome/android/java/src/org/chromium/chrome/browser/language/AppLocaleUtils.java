// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.content.Context;
import android.preference.PreferenceManager;
import android.text.TextUtils;

import com.google.android.play.core.splitcompat.SplitCompat;
import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.SplitInstallRequest;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.util.Locale;

/**
 * Provides utility functions to assist with overriding the application language.
 * This class manages the AppLanguagePref.
 */
public class AppLocaleUtils {
    private AppLocaleUtils(){};

    private static final String TAG = "AppLocale";

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
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, null);
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
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, null);
    }

    /**
     * Set the value of application language shared preference. If set to null
     * the system language will be used.
     */
    public static void setAppLanguagePref(String languageName) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, languageName);
        if (BundleUtils.isBundle()) {
            ensureLaguageSplitInstalled(languageName);
        }
    }

    /**
     * Enable access to language split for bundle builds with an override language set.
     * @param context Activity context to enable downloaded language splits on.
     */
    public static void maybeInstallActivitySplitCompat(Context context) {
        if (GlobalAppLocaleController.getInstance().isOverridden() && BundleUtils.isBundle()) {
            Log.i(TAG, "maybeInstallActivitySplit isOverridden: %s  isBundle: %s",
                    GlobalAppLocaleController.getInstance().isOverridden(), BundleUtils.isBundle());
            Log.i(TAG, "Override Locale: %s", getAppLanguagePref());
            logInstalledLanguages(context);
            SplitCompat.installActivity(context);
            logInstalledLanguages(context);
        }
    }

    /**
     * For bundle builds ensure that the language split for languageName is download.
     */
    private static void ensureLaguageSplitInstalled(String languageName) {
        SplitInstallManager splitInstallManager =
                SplitInstallManagerFactory.create(ContextUtils.getApplicationContext());

        // TODO(perrier): check if languageName is already installed. https://crbug.com/1103806
        if (languageName != null) {
            SplitInstallRequest installRequest =
                    SplitInstallRequest.newBuilder()
                            .addLanguage(Locale.forLanguageTag(languageName))
                            .build();
            splitInstallManager.startInstall(installRequest);
        }
    }

    /**
     * Log list of installed languages for context.
     * @param context Context to log installed languages on.
     */
    private static void logInstalledLanguages(Context context) {
        if (BundleUtils.isBundle()) {
            SplitInstallManager splitInstallManager = SplitInstallManagerFactory.create(context);
            Log.i(TAG, "Installed Languages: %s",
                    TextUtils.join(", ", splitInstallManager.getInstalledLanguages()));
        } else {
            Log.i(TAG, "Installed Languages: None - not a bundle");
        }
    }
}
