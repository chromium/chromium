// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Build;
import android.os.LocaleList;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;
import org.chromium.ui.base.LocalizationUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * This class provides the locale related methods for Chrome.
 */
public class ChromeLocalizationUtils {
    // Constants used to log UI language availability. Must stay in sync with values in the
    // LanguageUsage.UI.Available enum. These values are persisted to logs. Entries should
    // not be renumbered and numeric values should never be reused.
    @IntDef({UiAvailableTypes.TOP_AVAILABLE, UiAvailableTypes.ONLY_DEFAULT_AVAILABLE,
            UiAvailableTypes.NONE_AVAILABLE, UiAvailableTypes.OVERRIDDEN})
    @Retention(RetentionPolicy.SOURCE)
    @interface UiAvailableTypes {
        int TOP_AVAILABLE = 0;
        int ONLY_DEFAULT_AVAILABLE = 1;
        int NONE_AVAILABLE = 2;
        int OVERRIDDEN = 3;
        int NUM_ENTRIES = 4;
    }

    // Constants used to log the UI language correctness. Must stay in sync with values in the
    // LanguageUsage.UI.Android.Correctness enum. These values are persisted to logs. Entries
    // should not be renumbered and numeric values should never be reused.
    @IntDef({UiCorrectTypes.CORRECT, UiCorrectTypes.INCORRECT, UiCorrectTypes.NOT_AVAILABLE,
            UiCorrectTypes.ONLY_JAVA_CORRECT})
    @Retention(RetentionPolicy.SOURCE)
    @interface UiCorrectTypes {
        int CORRECT = 0;
        int INCORRECT = 1;
        int NOT_AVAILABLE = 2;
        int ONLY_JAVA_CORRECT = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * @return the current Chromium locale used to display UI elements.
     *
     * This matches what the Android framework resolves localized string resources to, using the
     * system locale and the application's resources. For example, if the system uses a locale
     * that is not supported by Chromium resources (e.g. 'fur-rIT'), Android will likely fallback
     * to 'en-rUS' strings when Resources.getString() is called, and this method will return the
     * matching Chromium name (i.e. 'en-US').
     *
     * Using this value is necessary to ensure that the strings accessed from the locale .pak files
     * from C++ match the resources displayed by the Java-based UI views.
     */
    public static String getJavaUiLocale() {
        return ContextUtils.getApplicationContext().getResources().getString(
                R.string.current_detected_ui_locale_name);
    }

    /**
     * Records the status of the current UI language under "LanguageUsage.UI.Android.*". Tracks if
     * the Android system language is available and if the Chromium UI language is correct.
     *
     * On N+ both the top Android language and default Android language are checked for
     * availability. The default language is the one used by the JVM for localization. These will be
     * different if the top Android is not available for localization in Chromium. Otherwise they
     * are the same.
     *
     * For correctness both the Java and native UI languages are checked. These can be different if
     * an override language is set and Play Store hygiene has not run.
     */
    public static void recordUiLanguageStatus() {
        String defaultLanguage = LocaleUtils.toLanguage(Locale.getDefault().toLanguageTag());

        // The default locale is the first Android locale with translated Chromium resources. On N+
        // the top system language can be retrieved, even if it is not an option for Chromium's UI.
        String topAndroidLanguage = defaultLanguage;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            topAndroidLanguage =
                    LocaleUtils.toLanguage(LocaleList.getDefault().get(0).toLanguageTag());
        }
        boolean isDefaultLanguageAvailable = AppLocaleUtils.isSupportedUiLanguage(defaultLanguage);
        boolean isTopAndroidLanguageAvailable =
                AppLocaleUtils.isSupportedUiLanguage(topAndroidLanguage);

        // The java and native UI languages can be different if the native language pack is not
        // correctly installed through the Play Store.
        String javaUiLanguage = LocaleUtils.toLanguage(getJavaUiLocale());
        String nativeUiLanguage = LocaleUtils.toLanguage(LocalizationUtils.getNativeUiLocale());
        boolean isJavaUiCorrect = TextUtils.equals(defaultLanguage, javaUiLanguage);
        boolean isNativeUiCorrect = TextUtils.equals(defaultLanguage, nativeUiLanguage);

        // The Android Chromium UI language can be overridden at the device level by users.
        boolean isOverridden = GlobalAppLocaleController.getInstance().isOverridden();

        @UiAvailableTypes
        int availableStatus = getUiAvailabilityStatus(
                isOverridden, isTopAndroidLanguageAvailable, isDefaultLanguageAvailable);
        RecordHistogram.recordEnumeratedHistogram("LanguageUsage.UI.Android.Availability",
                availableStatus, UiAvailableTypes.NUM_ENTRIES);

        boolean noLanguageAvailable = !isTopAndroidLanguageAvailable && !isDefaultLanguageAvailable;
        @UiCorrectTypes
        int correctStatus =
                getUiCorrectnessStatus(noLanguageAvailable, isJavaUiCorrect, isNativeUiCorrect);
        RecordHistogram.recordEnumeratedHistogram(
                "LanguageUsage.UI.Android.Correctness", correctStatus, UiCorrectTypes.NUM_ENTRIES);

        if (isOverridden) {
            @UiCorrectTypes
            int overrideStatus = getOverrideUiCorrectStatus(isJavaUiCorrect, isNativeUiCorrect);
            RecordHistogram.recordEnumeratedHistogram(
                    "LanguageUsage.UI.Android.Correctness.Override", overrideStatus,
                    UiCorrectTypes.NUM_ENTRIES);
        } else {
            @UiCorrectTypes
            int noOverrideStatus =
                    getNoOverrideUiCorrectStatus(noLanguageAvailable, isJavaUiCorrect);
            RecordHistogram.recordEnumeratedHistogram(
                    "LanguageUsage.UI.Android.Correctness.NoOverride", noOverrideStatus,
                    UiCorrectTypes.NUM_ENTRIES);
        }
    }

    /**
     * Return the status of Android system languages for use as the Chromium UI language.
     * The default language is the one used by the JVM for localization. This will be different from
     * the top Android language if the top Android language is not available for localization in
     * Chromium. See {@link LocaleList#getDefault()}. If the Chromium UI is overridden the language
     * set will always be available.
     * @param isOverridden Boolean indicating if the Chromium UI is overridden.
     * @param isTopAndroidLanguageAvailable Boolean indicating if the top Android language can be
     *         the UI language.
     * @param isDefaultLanguageAvailable Boolean indicating if the default Android language can be
     *         the UI language.
     * @return The @UiAvailableTypes status of Android language settings.
     */
    @VisibleForTesting
    static @UiAvailableTypes int getUiAvailabilityStatus(boolean isOverridden,
            boolean isTopAndroidLanguageAvailable, boolean isDefaultLanguageAvailable) {
        if (isOverridden) {
            return UiAvailableTypes.OVERRIDDEN;
        }
        if (isTopAndroidLanguageAvailable) {
            return UiAvailableTypes.TOP_AVAILABLE;
        }
        if (isDefaultLanguageAvailable && !isTopAndroidLanguageAvailable) {
            return UiAvailableTypes.ONLY_DEFAULT_AVAILABLE;
        }
        return UiAvailableTypes.NONE_AVAILABLE;
    }

    /**
     * Return the correctness status of the current Chromium UI.  The Ui language is correct when it
     * matches the default Android system language. The native UI language can be different from the
     * Java UI language if Play Store daily hygiene has not run since setting an override language.
     * @param noLanguageAvailable Boolean indicating no Android system language is a Chromium
     *         language.
     * @param isJavaUiCorrect Boolean indicating if the Java UI language is correct.
     * @param isNativeUiCorrect Boolean indicating if the native UI language is correct.
     * @return The @UiCorrectnessTypes status of the UI.
     */
    @VisibleForTesting
    static @UiCorrectTypes int getUiCorrectnessStatus(
            boolean noLanguageAvailable, boolean isJavaUiCorrect, boolean isNativeUiCorrect) {
        if (noLanguageAvailable) {
            return UiCorrectTypes.NOT_AVAILABLE;
        }
        if (isJavaUiCorrect && isNativeUiCorrect) {
            return UiCorrectTypes.CORRECT;
        }
        if (isJavaUiCorrect && !isNativeUiCorrect) {
            return UiCorrectTypes.ONLY_JAVA_CORRECT;
        }
        return UiCorrectTypes.INCORRECT;
    }

    /**
     * Return the status of the current Chromium UI when the UI language is not overridden. The UI
     * language is correct when it matches the default Android system language. If no Android
     * language is available as the Chromium language then the UI is by definition incorrect.
     * @param isCorrect Boolean indicating the Chromium UI language matches the Android default.
     * @return The @UiCorrectnessTypes status of the UI.
     */
    @VisibleForTesting
    static @UiCorrectTypes int getNoOverrideUiCorrectStatus(
            boolean noLanguageAvailable, boolean isCorrect) {
        if (noLanguageAvailable) {
            return UiCorrectTypes.NOT_AVAILABLE;
        }
        if (isCorrect) {
            return UiCorrectTypes.CORRECT;
        }
        return UiCorrectTypes.INCORRECT;
    }

    /**
     * Return the status of the current Chromium UI when the UI language is overridden. The UI
     * language is correct when it matches the override UI value set as the default locale. The
     * native UI language can be different from the Java UI language if Play Store daily hygiene has
     * not run since setting an override language.
     * @param isJavaUiCorrect Boolean indicating if the Java UI language is correct.
     * @param isNativeUiCorrect Boolean indicating if the native UI language is correct.
     * @return The @UiCorrectnessTypes status of the UI.
     */
    @VisibleForTesting
    static @UiCorrectTypes int getOverrideUiCorrectStatus(
            boolean isJavaUiCorrect, boolean isNativeUiCorrect) {
        if (isJavaUiCorrect && isNativeUiCorrect) {
            return UiCorrectTypes.CORRECT;
        }
        if (isJavaUiCorrect && !isNativeUiCorrect) {
            return UiCorrectTypes.ONLY_JAVA_CORRECT;
        }
        return UiCorrectTypes.INCORRECT;
    }

    private ChromeLocalizationUtils() {
        /* cannot be instantiated */
    }
}
