// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.webkit.WebViewClient;

import org.chromium.components.safe_browsing.SBThreatType;

/**
 * This is a helper class to map native SafeBrowsingActions and SAFE_BROWSING_THREATs to the
 * constants in WebViewClient.
 */
public final class AwSafeBrowsingConversionHelper {
    /** The resource was blocked for an unknown reason. */
    public static final int SAFE_BROWSING_THREAT_UNKNOWN =
            WebViewClient.SAFE_BROWSING_THREAT_UNKNOWN;
    /** The resource was blocked because it contains malware. */
    public static final int SAFE_BROWSING_THREAT_MALWARE =
            WebViewClient.SAFE_BROWSING_THREAT_MALWARE;
    /** The resource was blocked because it contains deceptive content. */
    public static final int SAFE_BROWSING_THREAT_PHISHING =
            WebViewClient.SAFE_BROWSING_THREAT_PHISHING;
    /** The resource was blocked because it contains unwanted software. */
    public static final int SAFE_BROWSING_THREAT_UNWANTED_SOFTWARE =
            WebViewClient.SAFE_BROWSING_THREAT_UNWANTED_SOFTWARE;
    /** The resource was blocked because it may trick the user into a billing agreement. */
    // TODO(ntfschr): add a new int to the SDK.
    public static final int SAFE_BROWSING_THREAT_BILLING =
            WebViewClient.SAFE_BROWSING_THREAT_UNKNOWN;

    /**
     * Converts the threat type value from SafeBrowsing code to the WebViewClient constant.
     */
    public static int convertThreatType(int chromiumThreatType) {
        switch (chromiumThreatType) {
            case SBThreatType.URL_MALWARE:
                return SAFE_BROWSING_THREAT_MALWARE;
            case SBThreatType.URL_PHISHING:
                return SAFE_BROWSING_THREAT_PHISHING;
            case SBThreatType.URL_UNWANTED:
                return SAFE_BROWSING_THREAT_UNWANTED_SOFTWARE;
            case SBThreatType.BILLING:
                return SAFE_BROWSING_THREAT_BILLING;
            default:
                return SAFE_BROWSING_THREAT_UNKNOWN;
        }
    }

    // Do not instantiate this class.
    private AwSafeBrowsingConversionHelper() {}
}
