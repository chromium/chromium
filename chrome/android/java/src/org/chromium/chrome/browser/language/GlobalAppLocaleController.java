// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.content.Context;
import android.content.res.Configuration;
import android.text.TextUtils;

import org.chromium.base.LocaleUtils;

import java.util.Locale;

/**
 * The global application language controller that uses the locale from
 * {@link AppLocaleUtils#getAppLanguagePref} to override the locales in
 * {@link ChromeApplication} and {@link ChromeActivity} and default Locale.
 */
public class GlobalAppLocaleController {
    private static final GlobalAppLocaleController INSTANCE = new GlobalAppLocaleController();

    // Set the original system language before Locale.getDefault() is overridden.
    private final Locale mOriginalSystemLocal = Locale.getDefault();
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
        mOverrideLanguage = AppLocaleUtils.getAppLanguagePrefStartUp(base);

        mIsOverridden = !TextUtils.isEmpty(mOverrideLanguage)
                && !TextUtils.equals(mOriginalSystemLocal.toLanguageTag(), mOverrideLanguage);
        return mIsOverridden;
    }

    /**
     * If the application should be overridden returns the override Configuration and updates the
     * default LocaleList/Locale. Called early in {@link ChromeActivity#attachBaseContext}.
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
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        return config;
    }

    /**
     * Do the Activity level locale override if app locale preference is set.
     * Should be called from {@link Activity#attachBaseContext()}.
     * @param base The base Context for the Activity which has the system locales.
     * @param config The Configuration that will be used to update the Activity.
     * @return boolean Whether or not config was modified.
     */
    public boolean applyActivityOverrides(Context base, Configuration config) {
        if (!mIsOverridden) {
            return false;
        }

        LocaleUtils.updateConfig(base, config, mOverrideLanguage);
        return true;
    }

    /**
     * Get the original application locale.  If there is no override language this is
     * the current application language.
     * @return Locale of the original system language.
     */
    public Locale getOriginalSystemLocale() {
        return mOriginalSystemLocal;
    }

    /**
     * Return the override state of the controller
     * @return boolean Whether the Application locale is overridden.
     */
    public boolean isOverridden() {
        return mIsOverridden;
    }

    /**
     * Return the GlobalAppLocaleController singleton instance.
     * @return GlobalAppLocaleController singleton.
     */
    public static GlobalAppLocaleController getInstance() {
        return INSTANCE;
    }
}
