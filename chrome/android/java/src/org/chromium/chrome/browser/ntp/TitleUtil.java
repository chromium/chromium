// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.Nullable;

/**
 * Provides functions for working with link titles.
 */
public final class TitleUtil {
    private TitleUtil() {}

    /**
     * Returns a title suitable for display for a link. If |title| is non-empty, this simply returns
     * it. Otherwise, returns a shortened form of the URL.
     */
    public static String getTitleForDisplay(@Nullable String title, @Nullable String url) {
        if (!TextUtils.isEmpty(title) || TextUtils.isEmpty(url)) {
            return title;
        }

        Uri uri = Uri.parse(url);
        String host = uri.getHost();
        if (host == null) host = "";
        String path = uri.getPath();
        if (path == null || path.equals("/")) path = "";
        title = host + path;
        return title;
    }
}
