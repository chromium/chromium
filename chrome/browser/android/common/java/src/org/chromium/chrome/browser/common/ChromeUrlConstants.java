// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.common;

import org.chromium.url.GURL;

/**
 * TODO(crbug.com/40281619) Move NTP related constants from UrlConstants.java to here.
 *
 * <p>Java side version of NTP related constants in chrome/common/url_constants.cc
 */
public class ChromeUrlConstants {
    private static class Holder {
        private static final String SERIALIZED_NATIVE_NTP_URL =
                "82,1,true,0,13,0,-1,0,-1,16,6,0,-1,22,1,0,-1,0,-1,false,false,chrome-native://newtab/";
        private static GURL sNativeNtpGurl =
                GURL.deserializeLatestVersionOnly(SERIALIZED_NATIVE_NTP_URL.replace(',', '\0'));
    }

    /**
     * Returns a cached GURL representation of {@link UrlConstants.NTP_URL}. It is safe
     * to call this method before native is loaded and doing so will not block on native loading
     * completion since a hardcoded, serialized string is used.
     */
    public static GURL nativeNtpGurl() {
        return Holder.sNativeNtpGurl;
    }
}
