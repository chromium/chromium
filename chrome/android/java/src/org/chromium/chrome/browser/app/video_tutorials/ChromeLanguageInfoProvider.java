// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.video_tutorials;

import org.chromium.chrome.browser.language.settings.LanguageItem;
import org.chromium.chrome.browser.language.settings.LanguagesManager;
import org.chromium.chrome.browser.video_tutorials.Language;
import org.chromium.chrome.browser.video_tutorials.LanguageInfoProvider;

/**
 * See {@link LanguageInfoProvider}.
 */
public class ChromeLanguageInfoProvider implements LanguageInfoProvider {
    @Override
    public Language getLanguageInfo(String locale) {
        LanguageItem languageItem = LanguagesManager.getInstance().getLanguageMap().get(locale);
        if (languageItem == null) return null;

        return new Language(languageItem.getCode(), languageItem.getDisplayName(),
                languageItem.getNativeDisplayName());
    }
}
