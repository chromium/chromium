// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.net.Uri;

import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Collection of util methods for help launching a NewTabPage. */
public class NewTabPageUtils {
    private static final String ORIGIN_PARAMETER_KEY = "origin";
    private static final String WEB_FEED_PARAMETER = "web-feed";

    /**
     * @return The NTP url encoded with {@link NewTabPageLaunchOrigin} information.
     */
    public static String encodeNtpUrl(@NewTabPageLaunchOrigin int launchOrigin) {
        Uri.Builder uriBuilder = Uri.parse(UrlConstants.NTP_URL).buildUpon();
        switch (launchOrigin) {
            case NewTabPageLaunchOrigin.WEB_FEED:
                uriBuilder.appendQueryParameter(ORIGIN_PARAMETER_KEY, WEB_FEED_PARAMETER);
                break;
            case NewTabPageLaunchOrigin.UNKNOWN:
            default:
                break;
        }
        return uriBuilder.build().toString();
    }

    /**
     * @return The {@link NewTabPageLaunchOrigin} decoded from the NTP url.
     */
    public static @NewTabPageLaunchOrigin int decodeOriginFromNtpUrl(String url) {
        if (!UrlUtilities.isNtpUrl(url)) {
            return NewTabPageLaunchOrigin.UNKNOWN;
        }
        Uri uri = Uri.parse(url);
        String origin = uri.getQueryParameter(ORIGIN_PARAMETER_KEY);
        if (origin != null && origin.equals(WEB_FEED_PARAMETER)) {
            return NewTabPageLaunchOrigin.WEB_FEED;
        }
        return NewTabPageLaunchOrigin.UNKNOWN;
    }
}
