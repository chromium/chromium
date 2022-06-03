// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.content.Context;
import android.preference.PreferenceManager;
import android.text.TextUtils;

import org.chromium.base.BundleUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.ui.base.ResourceBundle;

import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;

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
     * Get the value of application language shared preference or null if there is none.
     * @return String BCP-47 language tag (e.g. en-US).
     */
    public static String getAppLanguagePref() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, APP_LOCALE_USE_SYSTEM_LANGUAGE);
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
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, APP_LOCALE_USE_SYSTEM_LANGUAGE);
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
                SharedPreferencesManager.getInstance().writeString(
                        ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, languageName);
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
     * Return true if the base language of |languageCode| has multiple UI language variants (e.g.
     * pt-BR and pt-PT).
     * @param languageCode Language tag to look up.
     * @return Whether or not |languageCode| has multiple UI language variants.
     */
    public static boolean hasMultipleUiLanguageVariants(String languageCode) {
        if (isFollowSystemLanguage(languageCode)) {
            return false;
        }
        String baseLanguage = LocaleUtils.toLanguage(languageCode);
        HashSet<String> baseLanguages = new HashSet<String>();
        for (String code : ResourceBundle.getAvailableLocales()) {
            String base = LocaleUtils.toLanguage(code);
            if (baseLanguages.contains(base) && TextUtils.equals(base, baseLanguage)) {
                return true;
            }
            baseLanguages.add(LocaleUtils.toLanguage(code));
        }
        return false;
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
            String langA = LocaleUtils.toLanguage(a);
            String langB = LocaleUtils.toLanguage(b);
            return langA.compareTo(langB);
        }
    };
}
