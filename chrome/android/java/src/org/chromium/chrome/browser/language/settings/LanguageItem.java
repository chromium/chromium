// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

/**
 * Simple object representing the language item.
 */
public class LanguageItem {
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
        mSupportAppUI = AvailableUiLanguages.isAvailable(mCode);
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
     * @return Whether this language supports the Chrome UI.
     */
    public boolean isUISupported() {
        return mSupportAppUI;
    }
}
