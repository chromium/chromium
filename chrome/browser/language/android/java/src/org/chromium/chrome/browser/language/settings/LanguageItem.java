// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;
import org.chromium.chrome.browser.language.R;

import java.util.Comparator;
import java.util.Locale;
import java.util.Objects;

/** Simple object representing the language item. */
public class LanguageItem {
    /** Comparator for sorting LanguageItems alphabetically by display name. */
    public static final Comparator<LanguageItem> COMPARE_BY_DISPLAY_NAME =
            (l1, l2) -> {
                return l1.getDisplayName().compareTo(l2.getDisplayName());
            };

    private final String mCode;

    private final String mDisplayName;

    private final String mNativeDisplayName;

    private final boolean mSupportTranslate;

    private boolean mSupportAppUI;

    /**
     * Creates a new LanguageItem getting UI availability from ResourceBundle.
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
        mSupportAppUI = AppLocaleUtils.isAvailableExactUiLanguage(code);
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
    public boolean isTranslateSupported() {
        return mSupportTranslate;
    }

    /**
     * Return true if this LanguageItem is a base language that supports translate.
     * This filters out country variants that are not differentiated by Translate even if their base
     * language is (e.g. en-GB, en-IN, or es-MX).
     * Todo(crbug.com/1180262): Make mSupportTranslate equivalent to this flag.
     * @return Whether or not this Language item is a base translatable language.
     */
    public boolean isSupportedBaseTranslateLanguage() {
        if (!mSupportTranslate) {
            return false;
        }

        switch (mCode) {
            case "zh-CN":
            case "zh-TW":
            case "mni-Mtei":
                // Cases with a variant that support translate
                return true;
            case "nb":
                // Translate uses the macrolangauge code "no" instead of "nb".
                return false;
            default:
                // If not a language with supported variants check that the code is a base language.
                return !mCode.contains("-");
        }
    }

    /**
     * @return Whether this language supports the Chrome UI.
     */
    public boolean isUISupported() {
        return mSupportAppUI;
    }

    /**
     * @return True if this language item represents the system default.
     */
    public boolean isSystemDefault() {
        return AppLocaleUtils.isFollowSystemLanguage(mCode);
    }

    /**
     * Return the hashCode of the language code for this LanguageItem. The language code can be
     * used for the hash since two LanguageItems with equal language codes are equal.
     */
    @Override
    public int hashCode() {
        return Objects.hashCode(mCode);
    }

    /** return String representation of the BCP-47 code for this language. */
    @Override
    public String toString() {
        return getCode();
    }

    /** Two LanguageItems are equal if their language codes are equal. */
    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof LanguageItem)) return false;
        LanguageItem other = (LanguageItem) obj;
        return TextUtils.equals(mCode, other.mCode);
    }

    /**
     * Create a LanguageItem representing the system default language.
     * @return LanguageItem
     */
    public static LanguageItem makeFollowSystemLanguageItem() {
        String displayName =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getString(R.string.default_lang_subtitle);
        String nativeName =
                GlobalAppLocaleController.getInstance()
                        .getOriginalSystemLocale()
                        .getDisplayName(Locale.getDefault());
        return new LanguageItem(
                AppLocaleUtils.APP_LOCALE_USE_SYSTEM_LANGUAGE,
                displayName,
                nativeName,
                /* supportTranslate= */ true);
    }
}
