// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.content.Context;
import android.os.Build;
import android.preference.PreferenceManager;
import android.text.TextUtils;

import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.base.BuildInfo;
import org.chromium.base.BundleUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.language.LocaleManagerDelegate;
import org.chromium.components.language.LocaleManagerDelegateImpl;
import org.chromium.ui.base.ResourceBundle;

import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

/**
 * Provides utility functions to assist with overriding the application language.
 * This class manages the AppLanguagePref.
 */
public class AppLocaleUtils {
    private AppLocaleUtils(){};

    // Value of AppLocale preference when the system language is used.
    public static final String APP_LOCALE_USE_SYSTEM_LANGUAGE = null;

    /**
     * Return true if languageName is the same as the current application override
     * language stored preference.
     * @return boolean
     */
    public static boolean isAppLanguagePref(String languageName) {
        return TextUtils.equals(getAppLanguagePref(), languageName);
    }

    /**
     * The |ApplocaleUtils.APP_LOCALE_USE_SYSTEM_LANGUAGE| constant acts as a signal that no app
     * override language is set and when this is the case the app UI language tracks the device
     * language.
     * @param overrideLanguage String to compare to the default system language value.
     * @return Whether or not |overrideLanguage| is the default system language.
     */
    public static boolean isFollowSystemLanguage(String overrideLanguage) {
        return TextUtils.equals(overrideLanguage, APP_LOCALE_USE_SYSTEM_LANGUAGE);
    }

    /**
     * Get the value of application language shared preference or null if there is none. On T+ this
     * method will use the {@link LocaleManager} service to get the App language.
     * @return String BCP-47 language tag (e.g. en-US).
     */
    public static String getAppLanguagePref() {
        if (shouldUseSystemManagedLocale()) {
            return getSystemManagedAppLanguage();
        }
        return ChromeSharedPreferences.getInstance().readString(
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, APP_LOCALE_USE_SYSTEM_LANGUAGE);
    }

    /**
     * Get the value of application language shared preference or null if there is none.
     * Used during {@link ChromeApplication#attachBaseContext} before
     * {@link ChromeSharedPreferences} is created.
     * @param base Context to use for getting the shared preference.
     * @return String BCP-47 language tag (e.g. en-US).
     */
    @SuppressWarnings("DefaultSharedPreferencesCheck")
    static String getAppLanguagePrefStartUp(Context base) {
        return PreferenceManager.getDefaultSharedPreferences(base).getString(
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, APP_LOCALE_USE_SYSTEM_LANGUAGE);
    }

    /**
     * Get the value of system App language using {@link LocaleManager}, Android ensures this
     * language is always supported by Chrome. If no override language is set
     * |APP_LOCALE_USE_SYSTEM_LANGUAGE| is returned. Only used on Android T (API level 33).
     * TODO(crbug.com/1333981) Move to Android T.
     */
    @RequiresApi(Build.VERSION_CODES.S)
    static @Nullable String getSystemManagedAppLanguage() {
        Locale locale = getAppLocaleManagerDelegate().getApplicationLocale();
        if (locale == null) {
            return APP_LOCALE_USE_SYSTEM_LANGUAGE;
        }
        return locale.toLanguageTag();
    }

    /**
     * Gets the first original system locale from {@link LocaleManager}. This is the language that
     * Chrome would use if there was no override set. If there are no possible UI languages en-US is
     * returned since that is the default UI language in that case. Only used on Android T (API
     * level 33).
     * TODO(crbug.com/1333981) Move to Android T.
     * @return The UI language of the system.
     */
    @RequiresApi(Build.VERSION_CODES.S)
    static Locale getSystemManagedOriginalLocale() {
        List<Locale> locales = getAppLocaleManagerDelegate().getSystemLocales();
        for (Locale locale : locales) {
            if (isSupportedUiLanguage(locale.toLanguageTag())) {
                return locale;
            }
        }
        return Locale.forLanguageTag("en-US");
    }

    /**
     * Download the language split. If successful set the application language shared preference.
     * If set to null the system language will be used.
     * @param languageName String BCP-47 code of language to download.
     */
    public static void setAppLanguagePref(String languageName) {
        setAppLanguagePref(languageName, success -> {});
    }

    /**
     * Download the language split using the provided listener for callbacks. If successful set the
     * application language shared preference. If called from an APK build where no bundle needs to
     * be downloaded the listener's on complete function is immediately called. If languageName is
     * null the system language will be used.
     * @param languageName String BCP-47 code of language to download.
     * @param listener LanguageSplitInstaller.InstallListener to use for callbacks.
     */
    public static void setAppLanguagePref(
            String languageName, LanguageSplitInstaller.InstallListener listener) {
        // Wrap the install listener so that on success the app override preference is set.
        LanguageSplitInstaller.InstallListener wrappedListener = (success) -> {
            if (success) {
                if (shouldUseSystemManagedLocale()) {
                    setSystemManagedAppLanguage(languageName);
                } else {
                    ChromeSharedPreferences.getInstance().writeString(
                            ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, languageName);
                }
            }
            listener.onComplete(success);
        };

        // If this is not a bundle build or the default system language is being used the language
        // split should not be installed. Instead indicate that the listener completed successfully
        // since the language resources will already be present.
        if (!BundleUtils.isBundle() || isFollowSystemLanguage(languageName)) {
            wrappedListener.onComplete(true);
        } else {
            LanguageSplitInstaller.getInstance().installLanguage(languageName, wrappedListener);
        }
    }

    /**
     * Sets the {@link LocaleManager} App language to |languageName|.
     * TODO(crbug.com/1333981) Move to Android T.
     */
    @RequiresApi(Build.VERSION_CODES.S)
    private static void setSystemManagedAppLanguage(String languageName) {
        getAppLocaleManagerDelegate().setApplicationLocale(languageName);
    }

    /**
     * Get the LocaleManagerDelegate for {@link LocaleManager}.
     * Only used on Android T+ (API level 33).
     * TODO(crbug.com/1333981) Move to Android T.
     */
    @RequiresApi(Build.VERSION_CODES.S)
    static LocaleManagerDelegate getAppLocaleManagerDelegate() {
        return new LocaleManagerDelegateImpl();
    }

    /**
     * Migrate the App override language from Chrome SharedPreferences to the {@link LocaleManager}
     * service if needed.  A migration is only attempted once on Android T and done if there is a
     * Chrome SharedPreferences override language but no system App override language.
     * TODO(crbug.com/1333981) Move to Android T.
     * TODO(crbug.com/1334729) Remove migration after Oct 2023.
     */
    @RequiresApi(Build.VERSION_CODES.S)
    public static void maybeMigrateOverrideLanguage() {
        // Don't migrate if there is no SharedPreference for the override language.
        // Since null is saved in the SharedPreference if following the system language, a custom
        // token is used for when the preference is not present.
        String unsetToken = "__UNSET__";
        String sharedPrefAppLanguage = ChromeSharedPreferences.getInstance().readString(
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, unsetToken);
        if (TextUtils.equals(sharedPrefAppLanguage, unsetToken)) return;

        // Removed the old shared preference so a migration will not occur again.
        removeSharedPrefAppLanguage();

        // Don't migrate if the old override language was set to follow the system.
        if (isFollowSystemLanguage(sharedPrefAppLanguage)) return;

        // Don't migrate if the Android system already has an App override language. This means that
        // before the migration occurred a user set an App over language in the Android Settings.
        if (!TextUtils.isEmpty(getAppLanguagePref())) return;

        // Set the existing override language as the system App override language.
        setSystemManagedAppLanguage(sharedPrefAppLanguage);
    }

    private static void removeSharedPrefAppLanguage() {
        ChromeSharedPreferences.getInstance().removeKey(
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE);
    }

    /**
     * The LocaleManager API is only available on Android T. While using pre-release SDKs it is not
     * possible to use Build.VERSION_CODES.T. This method uses {@link BuildInfo.isAtLeastT} to check
     * that the current SDK is T (API level 33).
     * TODO(crbug.com/1333981) Remove when on released versions of the SDK.
     * @return True if the current Android SDK supports {@link LocaleManager}
     */
    @ChecksSdkIntAtLeast(api = 33)
    public static boolean shouldUseSystemManagedLocale() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU;
    }

    /**
     * Return true if the locale is an exact match for an available UI language.
     * Note: "en" and "en-AU" will return false since the available locales are "en-GB" and "en-US".
     * @param potentialUiLanguage BCP-47 language tag representing a locale (e.g. "en-US")
     */
    public static boolean isAvailableExactUiLanguage(String potentialUiLanguage) {
        return isAvailableUiLanguage(potentialUiLanguage, null);
    }

    /**
     * Return true if this locale is available or has a reasonable fallback language that can be
     * used for UI. For example we do not have language packs for "en" or "pt" but fallback to the
     * reasonable alternatives "en-US" and "pt-BR". Similarly, we have no language pack for "es-MX"
     * or "es-AR" but will use "es-419" for both. However, for languages with no translations
     * (e.g. "yo", "cy", ect.) the fallback is "en-US" which is not reasonable.
     * @param potentialUiLanguage BCP-47 language tag representing a locale (e.g. "en-US")
     */
    public static boolean isSupportedUiLanguage(String potentialUiLanguage) {
        return isAvailableUiLanguage(potentialUiLanguage, BASE_LANGUAGE_COMPARATOR);
    }

    private static boolean isAvailableUiLanguage(
            String potentialUiLanguage, Comparator<String> comparator) {
        // The default system language is always an available UI language.
        if (isFollowSystemLanguage(potentialUiLanguage)) return true;
        return Arrays.binarySearch(
                       ResourceBundle.getAvailableLocales(), potentialUiLanguage, comparator)
                >= 0;
    }

    /**
     * Comparator that removes any country or script information from either language tag
     * since they are not needed for locale availability checks.
     * Example: "es-MX" and "es-ES" will evaluate as equal.
     */
    private static final Comparator<String> BASE_LANGUAGE_COMPARATOR = new Comparator<String>() {
        @Override
        public int compare(String a, String b) {
            String langA = LocaleUtils.toBaseLanguage(a);
            String langB = LocaleUtils.toBaseLanguage(b);
            return langA.compareTo(langB);
        }
    };
}
