// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.LocaleUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.language.AndroidLanguageMetricsBridge;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * The global application language controller that uses the locale from
 * {@link AppLocaleUtils#getAppLanguagePref} to override the locales in
 * {@link ChromeApplication} and {@link ChromeActivity} and default Locale.
 */
public class GlobalAppLocaleController {
    private static final GlobalAppLocaleController INSTANCE = new GlobalAppLocaleController();

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static final String IS_SYSTEM_LANGUAGE_HISTOGRAM =
            "LanguageUsage.UI.Android.OverrideLanguage.IsSystemLanguage";

    /**
     * Annotation for the relationship between the override language and system language.
     * Do not reorder or remove items, only add new items before NUM_ENTRIES.
     * Keep in sync with LanguageUsage.UI.Android.OverrideLanguage.IsSystemLanguage from enums.xml.
     */
    @IntDef({
        OverrideLanguageStatus.DIFFERENT,
        OverrideLanguageStatus.SAME_BASE,
        OverrideLanguageStatus.EXACT_MATCH,
        OverrideLanguageStatus.NO_OVERRIDE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface OverrideLanguageStatus {
        int DIFFERENT = 0;
        int SAME_BASE = 1;
        int EXACT_MATCH = 2;
        int NO_OVERRIDE = 3;
        int NUM_ENTRIES = 4;
    }

    // Set the original system language before Locale.getDefault() is overridden.
    private Locale mOriginalSystemLocale = Locale.getDefault();
    private String mOverrideLanguage;
    private boolean mIsOverridden;

    private GlobalAppLocaleController() {}

    /**
     * Sets the global override language and override state based on the {@link AppLocaleUitls}
     * shared preference. Should be called very early in {@link ChromeActivity#attachBaseContext}.
     * @param context The Context to use to get the shared preference from.
     * @return boolean Whether or not an override language is set.
     */
    public boolean init(Context base) {
        if (AppLocaleUtils.shouldUseSystemManagedLocale()) {
            mIsOverridden = false;
        } else {
            mOverrideLanguage = AppLocaleUtils.getAppLanguagePrefStartUp(base);
            mIsOverridden =
                    shouldOverrideAppLocale(
                            mOverrideLanguage, LocaleUtils.toLanguageTag(mOriginalSystemLocale));
        }
        return mIsOverridden;
    }

    /**
     * Preform setup actions when using {@link LocaleManager} that need to be done after the
     * Application has started.
     */
    public void maybeSetupLocaleManager() {
        if (AppLocaleUtils.shouldUseSystemManagedLocale()) {
            mOverrideLanguage = AppLocaleUtils.getSystemManagedAppLanguage();
            mOriginalSystemLocale = AppLocaleUtils.getSystemManagedOriginalLocale();
        }
    }

    /**
     * If the application locale should be overridden returns an updated override Configuration.
     * Called early in {@link ChromeActivity#attachBaseContext}.
     * @param base The base Context for the application and has the system locales.
     * @return Configuration to override application context with or null.
     */
    public Configuration getOverrideConfig(Context base) {
        assert mIsOverridden : "Can only call GlobalAppLocaleController.getConfig if overridden";

        Configuration config = new Configuration();
        // Pre-Android O, fontScale gets initialized to 1 in the constructor. Set it to 0 so
        // that applyOverrideConfiguration() does not interpret it as an overridden value.
        // https://crbug.com/834191
        config.fontScale = 0;

        LocaleUtils.updateConfig(base, config, mOverrideLanguage);
        return config;
    }

    /**
     * If the locale should be overridden update the context configuration to use new locale.
     * @param base Context to update.
     */
    public void maybeOverrideContextConfig(Context base) {
        if (!mIsOverridden) {
            return;
        }

        Configuration config = getOverrideConfig(base);
        Resources resources = base.getResources();
        // Resources#updateConfiguration() seems to reset densityDpi if it's not specified by the
        // configuration, regardless of whether it's specified by the input DisplayMetrics.
        if (BuildInfo.getInstance().isAutomotive) {
            config.densityDpi = resources.getConfiguration().densityDpi;
        }
        // Because of an Android bug with {@link Context#createConfigurationContext} the deprecated
        // method {@link Resources#updateConfiguration} is used. (crbug.com/1075390#c20).
        // TODO(crbug.com/40152130): Use #createConfigurationContext once that method is fixed.
        resources.updateConfiguration(config, resources.getDisplayMetrics());
        // Update default locales so {@links LocaleList#getDefault} returns the correct value.
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
    }

    /**
     * Get the original application locale.  If there is no override language this is
     * the current application language.
     * @return Locale of the original system language.
     */
    public Locale getOriginalSystemLocale() {
        return mOriginalSystemLocale;
    }

    /**
     * Return the override state of the controller
     * @return boolean Whether the Application locale is overridden.
     */
    public boolean isOverridden() {
        return mIsOverridden;
    }

    /**
     * Record the override language and it's status compared to the system locale. If no override
     * language is set report it as the empty string.
     */
    public void recordOverrideLanguageMetrics() {
        // When following the system language there is no override so Chrome tracks the System UI.
        String histogramLanguage =
                AppLocaleUtils.isFollowSystemLanguage(mOverrideLanguage) ? "" : mOverrideLanguage;
        AndroidLanguageMetricsBridge.reportAppOverrideLanguage(histogramLanguage);

        int status =
                getOverrideVsSystemLanguageStatus(
                        mOverrideLanguage, LocaleUtils.toLanguageTag(mOriginalSystemLocale));
        RecordHistogram.recordEnumeratedHistogram(
                IS_SYSTEM_LANGUAGE_HISTOGRAM, status, OverrideLanguageStatus.NUM_ENTRIES);
    }

    /**
     * Get the status of the override language compared to the system language.
     * @return The {@link OverrideLanguageStatus} that describes the relationship between the system
     * language and override language.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static @OverrideLanguageStatus int getOverrideVsSystemLanguageStatus(
            String overrideLanguage, String systemLanguage) {
        // When following the system language there is no override so Chrome tracks the System UI.
        if (AppLocaleUtils.isFollowSystemLanguage(overrideLanguage)) {
            return OverrideLanguageStatus.NO_OVERRIDE;
        }
        if (TextUtils.equals(overrideLanguage, systemLanguage)) {
            return OverrideLanguageStatus.EXACT_MATCH;
        }
        if (LocaleUtils.isBaseLanguageEqual(overrideLanguage, systemLanguage)) {
            return OverrideLanguageStatus.SAME_BASE;
        }
        return OverrideLanguageStatus.DIFFERENT;
    }

    /**
     * Deterimine if the app locale should be overridden based on the override and system languages
     * provided.
     * @param overrideLanguage A BCP 47 tag representing which override language should be used.
     * @param overrideLanguage A BCP 47 tag representing the original system language.
     * @return Whether or not the app locale should be overridden.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static boolean shouldOverrideAppLocale(String overrideLanguage, String systemLanguage) {
        return !TextUtils.isEmpty(overrideLanguage)
                && !TextUtils.equals(systemLanguage, overrideLanguage);
    }

    /**
     * Return the GlobalAppLocaleController singleton instance.
     * @return GlobalAppLocaleController singleton.
     */
    public static GlobalAppLocaleController getInstance() {
        return INSTANCE;
    }
}
