// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;
import org.chromium.chrome.browser.language.R;
import org.chromium.ui.base.ResourceBundle;

import java.util.Arrays;
import java.util.Comparator;
import java.util.Locale;

/**
 * Simple object representing the language item.
 */
public class LanguageItem {
    /**
     * Comparator for sorting LanguageItems alphabetically by display name.
     */
    public static final Comparator<LanguageItem> COMPARE_BY_DISPLAY_NAME = (l1, l2) -> {
        return l1.getDisplayName().compareTo(l2.getDisplayName());
    };

    private final String mCode;

    private final String mDisplayName;

    private final String mNativeDisplayName;

    private final boolean mSupportTranslate;

    private boolean mSupportAppUI;

    /**
     * Creates a new {@link LanguageItem}.
     * @param code The BCP-47 language tag for this language item.
     * @param displayName The display name of the language in the current app locale.
     * @param nativeDisplayName The display name of the language in the language's locale.
     * @param supportTranslate Whether Chrome supports translate for this language.
     */
    public LanguageItem(
            String code, String displayName, String nativeDisplayName, boolean supportTranslate) {
        mCode = code;
        mDisplayName = displayName;
        mNativeDisplayName = nativeDisplayName;
        mSupportTranslate = supportTranslate;
        if (TextUtils.equals(code, AppLocaleUtils.SYSTEM_LANGUAGE_VALUE)) {
            mSupportAppUI = true; // system language is a supported UI language
        } else {
            mSupportAppUI = isAvailableUiLanguage(mCode);
        }
    }

    /**
     * @return The BCP-47 language tag of the language item.
     */
    public String getCode() {
        return mCode;
    }

    /**
     * @return The display name of the language in the current app locale.
     */
    public String getDisplayName() {
        return mDisplayName;
    }

    /**
     * @return The display name of the language in the language's locale.
     */
    public String getNativeDisplayName() {
        return mNativeDisplayName;
    }

    /**
     * @return Whether Chrome supports translate for this language.
     */
    public boolean isSupported() {
        return mSupportTranslate;
    }

    /**
     * Return true if this LanguageItem is a base language that supports translate.
     * This filters out country variants that are not supported by Translate even if their base
     * language is (e.g. en-US, en-IN, or es-MX).
     * Todo(crbug.com/1180262): Make mSupportTranslate equivalent to this flag.
     * @return Whether or not this Language item is a base translatable language.
     */
    public boolean isSupportedBaseLanguage() {
        if (!mSupportTranslate) {
            return false;
        }

        // Currently the only two country variants that are translateable are "zh-CN" and "zh-TW".
        if (TextUtils.equals(mCode, "zh-CN") || TextUtils.equals(mCode, "zh-TW")) {
            return true;
        }

        // "no" is used by translate as the macrolanguage including "nb".
        if (TextUtils.equals(mCode, "nb")) return false;

        // If not a language with supported variants check that the code is a base language.
        return !mCode.contains("-");
    }

    /**
     * @return Whether this language supports the Chrome UI.
     */
    public boolean isUISupported() {
        return mSupportAppUI;
    }

    /**
     * Create a LanguageItem representing the system default language.
     * @return LanguageItem
     */
    public static LanguageItem makeSystemDefaultLanguageItem() {
        String displayName = ContextUtils.getApplicationContext().getResources().getString(
                R.string.default_lang_subtitle);
        String nativeName =
                GlobalAppLocaleController.getInstance().getOriginalSystemLocale().getDisplayName(
                        Locale.getDefault());
        return new LanguageItem(
                AppLocaleUtils.SYSTEM_LANGUAGE_VALUE, displayName, nativeName, true);
    }

    /**
     * Return true if the language is available as a UI language.
     * @param language BCP-47 language tag representing a locale (e.g. "en-US")
     */
    public static boolean isAvailableUiLanguage(String language) {
        return Arrays.binarySearch(ResourceBundle.getAvailableLocales(), language) >= 0;
    }
}
