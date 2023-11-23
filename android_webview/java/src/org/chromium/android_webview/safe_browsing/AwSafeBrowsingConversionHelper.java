// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.safe_browsing;

import android.os.Build;
import android.webkit.WebViewClient;

import org.chromium.base.ContextUtils;
import org.chromium.components.safe_browsing.SBThreatType;

/**
 * This is a helper class to map native SafeBrowsingActions and SAFE_BROWSING_THREATs to the
 * constants in WebViewClient.
 */
public final class AwSafeBrowsingConversionHelper {
    // These values are used for UMA. Entries should not be renumbered and
    // numeric values should never be reused. The BOUNDARY constant should be
    // updated when adding new constants.

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
    // TODO(ntfschr): replace this with the named constant when we roll the Q SDK
    // (http://crbug.com/887186).
    public static final int SAFE_BROWSING_THREAT_BILLING = 4;

    /** Boundary for Safe Browsing Threat values, used for UMA recording. */
    public static final int SAFE_BROWSING_THREAT_BOUNDARY = 5;

    /**
     * Converts the threat type value from SafeBrowsing code to the WebViewClient constant.
     *
     * <p class="note"><b>Note:</b> this output may depend upon the embedding application's {@code
     * targetSdk} value if {@code chromiumThreatType} refers to a threat type added after {@link
     * Build.VERSION_CODES#O_MR1} (when we added the original Safe Browisng threat type constants).
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
                return ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                                >= Build.VERSION_CODES.Q
                        ? SAFE_BROWSING_THREAT_BILLING
                        : SAFE_BROWSING_THREAT_UNKNOWN;
            default:
                return SAFE_BROWSING_THREAT_UNKNOWN;
        }
    }

    // Do not instantiate this class.
    private AwSafeBrowsingConversionHelper() {}
}
