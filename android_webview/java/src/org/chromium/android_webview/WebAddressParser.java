// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.NonNull;

import java.net.URISyntaxException;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Copied from Android frameworks/base/core/java/android/net/WebAddress.java, with not needed
 * methods removed and formatting, so we don't depend on the class in Android. We should eventually
 * remove its usage in Chromium, because using regex to parse Url isn't generally working.
 *
 * Renamed to WebAddressParser to be able to use with the WebAddress class in the same place.
 *
 * Web Address Parser
 *
 * This is called WebAddress, rather than URL or URI, because it
 * attempts to parse the stuff that a user will actually type into a
 * browser address widget.
 *
 * Unlike java.net.uri, this parser will not choke on URIs missing
 * schemes.  It will only throw a URISyntaxException if the input is
 * really hosed.
 *
 * If given an https scheme but no port, fills in port
 *
 */
public class WebAddressParser {
    private String mScheme;
    private String mHost;
    private int mPort;
    private String mPath;
    private String mAuthInfo;

    // See android.util.Patterns.GOOD_IRI_CHAR.
    private static final String GOOD_IRI_CHAR = "a-zA-Z0-9\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF";
    private static final String SCHEME = "(?:(http|https|file)\\:\\/\\/)?";
    // We replace the regex of AUTHORITY to fix crbug.com/1247395, this is the only functional
    // change in this file comparing to the original WebAddress from Android framework.
    private static final String AUTHORITY = "(?:([^/?#:]+(?::[^/?#]+)?)@)?";
    private static final String HOST =
            "([" + GOOD_IRI_CHAR + "%_-][" + GOOD_IRI_CHAR + "%_\\.-]*|\\[[0-9a-fA-F:\\.]+\\])?";
    private static final String PORT = "(?:\\:([0-9]*))?";
    private static final String PATH = "(\\/?[^#]*)?";
    private static final String ANCHOR = ".*";

    static final int MATCH_GROUP_SCHEME = 1;
    static final int MATCH_GROUP_AUTHORITY = 2;
    static final int MATCH_GROUP_HOST = 3;
    static final int MATCH_GROUP_PORT = 4;
    static final int MATCH_GROUP_PATH = 5;

    static Pattern sAddressPattern =
            Pattern.compile(
                    SCHEME + AUTHORITY + HOST + PORT + PATH + ANCHOR, Pattern.CASE_INSENSITIVE);

    /** parses given uriString. */
    public WebAddressParser(String address) throws URISyntaxException {
        if (address == null) {
            throw new NullPointerException();
        }

        mScheme = "";
        mHost = "";
        mPort = -1;
        mPath = "/";
        mAuthInfo = "";

        Matcher m = sAddressPattern.matcher(address);
        String t;
        if (m.matches()) {
            t = m.group(MATCH_GROUP_SCHEME);
            if (t != null) mScheme = t.toLowerCase(Locale.ROOT);
            t = m.group(MATCH_GROUP_AUTHORITY);
            if (t != null) mAuthInfo = t;
            t = m.group(MATCH_GROUP_HOST);
            if (t != null) mHost = t;
            t = m.group(MATCH_GROUP_PORT);
            if (t != null && t.length() > 0) {
                // The ':' character is not returned by the regex.
                try {
                    mPort = Integer.parseInt(t);
                } catch (NumberFormatException ex) {
                    throw new URISyntaxException(address, "Bad port");
                }
            }
            t = m.group(MATCH_GROUP_PATH);
            if (t != null && t.length() > 0) {
                /* handle busted myspace frontpage redirect with
                missing initial "/" */
                if (t.charAt(0) == '/') {
                    mPath = t;
                } else {
                    mPath = "/" + t;
                }
            }
        } else {
            // nothing found... outa here
            throw new URISyntaxException(address, "Bad address");
        }

        /* Get port from scheme or scheme from port, if necessary and
        possible */
        if (mPort == 443 && mScheme.equals("")) {
            mScheme = "https";
        } else if (mPort == -1) {
            if (mScheme.equals("https")) {
                mPort = 443;
            } else {
                mPort = 80; // default
            }
        }
        if (mScheme.equals("")) mScheme = "http";
    }

    @NonNull
    @Override
    public String toString() {
        String port = "";
        if ((mPort != 443 && mScheme.equals("https")) || (mPort != 80 && mScheme.equals("http"))) {
            port = ":" + Integer.toString(mPort);
        }
        String authInfo = "";
        if (mAuthInfo.length() > 0) {
            authInfo = mAuthInfo + "@";
        }

        return mScheme + "://" + authInfo + mHost + port + mPath;
    }
}
