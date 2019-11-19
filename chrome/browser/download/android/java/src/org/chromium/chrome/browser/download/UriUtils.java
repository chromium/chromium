// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.net.Uri;
import android.text.TextUtils;

import org.chromium.chrome.browser.util.UrlConstants;

/**
 * Utility class for Uri related operations.
 */
public class UriUtils {
    /**
     * Parses an originating URL string and returns a valid Uri that can be inserted into
     * DownloadManager. The returned Uri has to be null or non-empty http(s) scheme.
     * @param originalUrl String representation of the originating URL.
     * @return A valid Uri that can be accepted by DownloadManager.
     */
    public static Uri parseOriginalUrl(String originalUrl) {
        Uri originalUri = TextUtils.isEmpty(originalUrl) ? null : Uri.parse(originalUrl);
        if (originalUri != null) {
            String scheme = originalUri.normalizeScheme().getScheme();
            if (scheme == null
                    || (!scheme.equals(UrlConstants.HTTPS_SCHEME)
                            && !scheme.equals(UrlConstants.HTTP_SCHEME))) {
                originalUri = null;
            }
        }
        return originalUri;
    }
}
