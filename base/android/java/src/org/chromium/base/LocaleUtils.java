// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.os.LocaleList;
import android.text.TextUtils;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import java.util.ArrayList;
import java.util.Locale;

/** This class provides the locale related methods. */
public class LocaleUtils {
    /** Guards this class from being instantiated. */
    private LocaleUtils() {}

    /**
     * Java keeps deprecated language codes for Hebrew, Yiddish and Indonesian but Chromium uses
     * updated ones. Similarly, Android uses "tl" while Chromium uses "fil" for Tagalog/Filipino.
     * The Translate settings use "gom", but Chrome uses "kok". Apply a mapping here. See
     * http://developer.android.com/reference/java/util/Locale.html
     * @return a updated language code for Chromium with given language string.
     */
    public static String getUpdatedLanguageForChromium(String language) {
        // IMPORTANT: If adding a new Chrome UI language, update the mapping found in:
        // build/android/gyp/util/resource_utils.py (Languages that are accept languages, but not
        // Chrome Android UI languages do not need to be kept in sync).
        switch (language) {
            case "gom":
                return "kok"; // Konkani
            case "in":
                return "id"; // Indonesian
            case "iw":
                return "he"; // Hebrew
            case "ji":
                return "yi"; // Yiddish
            case "jw":
                return "jv"; // Javanese
            case "tl":
                return "fil"; // Filipino
            default:
                return language;
        }
    }

    /**
     * @return a locale with updated language codes for Chromium, with translated modern language
     *         codes used by Chromium.
     */
    @VisibleForTesting
    public static Locale getUpdatedLocaleForChromium(Locale locale) {
        String language = locale.getLanguage();
        String languageForChrome = getUpdatedLanguageForChromium(language);
        if (languageForChrome.equals(language)) {
            return locale;
        }
        return new Locale.Builder().setLocale(locale).setLanguage(languageForChrome).build();
    }

    /**
     * Android uses "tl" while Chromium uses "fil" for Tagalog/Filipino.
     * So apply a mapping here.
     * See http://developer.android.com/reference/java/util/Locale.html
     * @return a updated language code for Android with given language string.
     */
    public static String getUpdatedLanguageForAndroid(String language) {
        // IMPORTANT: Keep in sync with the mapping found in:
        // build/android/gyp/util/resource_utils.py
        switch (language) {
            case "und":
                return ""; // Undefined
            case "fil":
                return "tl"; // Filipino
            default:
                return language;
        }
    }

    /**
     * @return a locale with updated language codes for Android, from translated modern language
     *         codes used by Chromium.
     */
    @VisibleForTesting
    public static Locale getUpdatedLocaleForAndroid(Locale locale) {
        String language = locale.getLanguage();
        String languageForAndroid = getUpdatedLanguageForAndroid(language);
        if (languageForAndroid.equals(language)) {
            return locale;
        }
        return new Locale.Builder().setLocale(locale).setLanguage(languageForAndroid).build();
    }

    /**
     * This function creates a Locale object from xx-XX style string where xx is language code
     * and XX is a country code.
     * @return the locale that best represents the language tag.
     */
    public static Locale forLanguageTag(String languageTag) {
        Locale locale = Locale.forLanguageTag(languageTag);
        return getUpdatedLocaleForAndroid(locale);
    }

    /**
     * Converts Locale object to the BCP 47 compliant string format.
     * This works for API level lower than 24.
     *
     * Note that for Android M or before, we cannot use Locale.getLanguage() and
     * Locale.toLanguageTag() for this purpose. Since Locale.getLanguage() returns deprecated
     * language code even if the Locale object is constructed with updated language code. As for
     * Locale.toLanguageTag(), it does a special conversion from deprecated language code to updated
     * one, but it is only usable for Android N or after.
     * @return a well-formed IETF BCP 47 language tag with language and country code that
     *         represents this locale.
     */
    public static String toLanguageTag(Locale locale) {
        String language = getUpdatedLanguageForChromium(locale.getLanguage());
        String country = locale.getCountry();
        if (language.equals("no") && country.equals("NO") && locale.getVariant().equals("NY")) {
            return "nn-NO";
        }
        return country.isEmpty() ? language : language + "-" + country;
    }

    /**
     * Converts LocaleList object to the comma separated BCP 47 compliant string format.
     *
     * @return a well-formed IETF BCP 47 language tag with language and country code that
     *         represents this locale list.
     */
    @RequiresApi(Build.VERSION_CODES.N)
    public static String toLanguageTags(LocaleList localeList) {
        ArrayList<String> newLocaleList = new ArrayList<>();
        for (int i = 0; i < localeList.size(); i++) {
            Locale locale = getUpdatedLocaleForChromium(localeList.get(i));
            newLocaleList.add(toLanguageTag(locale));
        }
        return TextUtils.join(",", newLocaleList);
    }

    /**
     * Extracts the base language from a BCP 47 language tag.
     * @param languageTag language tag of the form xx-XX or xx.
     * @return the xx part of the language tag.
     */
    public static String toBaseLanguage(String languageTag) {
        int pos = languageTag.indexOf('-');
        if (pos < 0) {
            return languageTag;
        }
        return languageTag.substring(0, pos);
    }

    /**
     * @param first A BCP 47 formated language tag.
     * @param second A BCP 47 formated language tag.
     * @return True if the base language (e.g. "en" for "en-AU") is the same for each tag.
     */
    public static boolean isBaseLanguageEqual(String first, String second) {
        return TextUtils.equals(toBaseLanguage(first), toBaseLanguage(second));
    }

    /**
     * @return a language tag string that represents the default locale.
     *         The language tag is well-formed IETF BCP 47 language tag with language and country
     *         code.
     */
    @CalledByNative
    public static String getDefaultLocaleString() {
        return toLanguageTag(Locale.getDefault());
    }

    /**
     * @return a comma separated language tags string that represents a default locale or locales.
     *         Each language tag is well-formed IETF BCP 47 language tag with language and country
     *         code.
     */
    @CalledByNative
    public static String getDefaultLocaleListString() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return toLanguageTags(LocaleList.getDefault());
        }
        return getDefaultLocaleString();
    }

    /**
     * @return The default country code set during install.
     */
    @CalledByNative
    public static String getDefaultCountryCode() {
        CommandLine commandLine = CommandLine.getInstance();
        return commandLine.hasSwitch(BaseSwitches.DEFAULT_COUNTRY_CODE_AT_INSTALL)
                ? commandLine.getSwitchValue(BaseSwitches.DEFAULT_COUNTRY_CODE_AT_INSTALL)
                : Locale.getDefault().getCountry();
    }

    /**
     * Return the language tag of the first language in Configuration.
     * @param config Configuration to get language for.
     * @return The BCP 47 tag representation of the configuration's first locale.
     * Configuration.locale is deprecated on N+. However, read only is equivalent to
     * Configuration.getLocales()[0]. Change when minSdkVersion >= 24.
     */
    @SuppressWarnings("deprecation")
    public static String getConfigurationLanguage(Configuration config) {
        Locale locale = config.locale;
        return (locale != null) ? locale.toLanguageTag() : "";
    }

    /**
     * Return the language tag of the first language in the configuration
     * @param context Context to get language for.
     * @return The BCP 47 tag representation of the context's first locale.
     */
    public static String getContextLanguage(Context context) {
        return getConfigurationLanguage(context.getResources().getConfiguration());
    }

    /**
     * Prepend languageTag to the default locales on config.
     * @param base The Context to use for the base configuration.
     * @param config The Configuration to update.
     * @param languageTag The language to prepend to default locales.
     */
    public static void updateConfig(Context base, Configuration config, String languageTag) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            ApisN.setConfigLocales(base, config, languageTag);
        } else {
            config.setLocale(Locale.forLanguageTag(languageTag));
        }
    }

    /** Updates the default Locale/LocaleList to those of config. */
    public static void setDefaultLocalesFromConfiguration(Configuration config) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            ApisN.setLocaleList(config);
        } else {
            Locale.setDefault(config.locale);
        }
    }

    /** Helper class for N only code that is not validated on pre-N devices. */
    @RequiresApi(Build.VERSION_CODES.N)
    @VisibleForTesting
    static class ApisN {
        static void setConfigLocales(Context base, Configuration config, String language) {
            LocaleList updatedLocales =
                    prependToLocaleList(
                            language, base.getResources().getConfiguration().getLocales());
            config.setLocales(updatedLocales);
        }

        static void setLocaleList(Configuration config) {
            LocaleList.setDefault(config.getLocales());
        }

        /**
         * Create a new LocaleList with languageTag added to the front.
         * If languageTag is already in the list the existing tag is moved to the front.
         * @param languageTag String of language tag to prepend
         * @param localeList LocaleList to prepend to.
         * @return LocaleList
         */
        static LocaleList prependToLocaleList(String languageTag, LocaleList localeList) {
            String languageList = localeList.toLanguageTags();

            // Remove the first instance of languageTag with associated comma if present.
            // Pattern example: "(^|,)en-US$|en-US,"
            String pattern = String.format("(^|,)%1$s$|%1$s,", languageTag);
            languageList = languageList.replaceFirst(pattern, "");

            return LocaleList.forLanguageTags(
                    String.format("%1$s,%2$s", languageTag, languageList));
        }
    }
}
