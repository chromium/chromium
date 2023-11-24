// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import org.chromium.ui.base.ResourceBundle;

/** Utils for initializing and cleaning up android ResourceBundle in robolectric tests */
public class LanguageTestUtils {
    public static final String[] TEST_LOCALES = {
        "af", "am", "ar", "ar-XB", "as", "az", "be", "bg", "bn", "bs", "ca", "cs", "da", "de", "el",
        "en-GB", "en-US", "en-XA", "es", "es-419", "et", "eu", "fa", "fi", "fil", "fr", "fr-CA",
        "gl", "gu", "he", "hi", "hr", "hu", "hy", "id", "is", "it", "ja", "ka", "kk", "km", "kn",
        "ko", "ky", "lo", "lt", "lv", "mk", "ml", "mn", "mr", "ms", "my", "nb", "ne", "nl", "or",
        "pa", "pl", "pt-BR", "pt-PT", "ro", "ru", "si", "sk", "sl", "sq", "sr", "sr-Latn", "sv",
        "sw", "ta", "te", "th", "tr", "uk", "ur", "uz", "vi", "zh-CN", "zh-HK", "zh-TW", "zu"
    };

    private LanguageTestUtils() {}

    public static void initializeResourceBundleForTesting() {
        clearResourceBundleForTesting();
        ResourceBundle.setAvailablePakLocales(TEST_LOCALES);
    }

    public static void clearResourceBundleForTesting() {
        ResourceBundle.clearAvailablePakLocalesForTesting();
    }
}
