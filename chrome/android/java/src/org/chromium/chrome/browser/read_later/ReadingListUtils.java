// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import android.text.TextUtils;

import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Utility functions for reading list feature.
 */
public final class ReadingListUtils {
    /**
     * @return Whether the URL can be added as reading list article.
     */
    public static boolean isReadingListSupported(String url) {
        if (TextUtils.isEmpty(url)) return false;

        // This should match ReadingListModel::IsUrlSupported(), having a separate function since
        // the UI may not load native library.
        return url.startsWith(UrlConstants.HTTP_URL_PREFIX)
                || url.startsWith(UrlConstants.HTTPS_URL_PREFIX);
    }
}
