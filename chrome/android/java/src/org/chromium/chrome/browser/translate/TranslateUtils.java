// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.text.TextUtils;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;

/**
 * Utility classes related to the translate feature.
 */
public class TranslateUtils {
    /**
     * Returns true iff the content displayed in the current tab can be translated.
     * @param tab The tab in question.
     */
    public static boolean canTranslateCurrentTab(Tab tab) {
        String url = tab.getUrl();
        boolean isChromeScheme = url.startsWith(UrlConstants.CHROME_URL_PREFIX)
                || url.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX);
        boolean isFileScheme = url.startsWith(UrlConstants.FILE_URL_PREFIX);
        boolean isContentScheme = url.startsWith(UrlConstants.CONTENT_URL_PREFIX);
        return !isChromeScheme && !isFileScheme && !isContentScheme && !TextUtils.isEmpty(url)
                && tab.getWebContents() != null && TranslateBridge.canManuallyTranslate(tab);
    }
}
