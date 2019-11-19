// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.net.Uri;

import org.chromium.chrome.browser.util.UrlConstants;

import androidx.annotation.Nullable;

/**
 * A class to canonically represent a HTTP or HTTPS web origin in Java. In comparison to
 * {@link org.chromium.net.GURLUtils#getOrigin} it can be used before native is loaded and lets us
 * ensure conversion to an origin has been done with the type system.
 *
 * {@link #toString()} does <b>not</b> match {@link org.chromium.net.GURLUtils#getOrigin}. The
 * latter will return a String with a trailing "/". Not having a trailing slash matches RFC
 * behaviour (https://tools.ietf.org/html/rfc6454), it seems that
 * {@link org.chromium.net.GURLUtils#getOrigin} adds it as a bug, but as its result is saved to
 * user's Android Preferences, it is not trivial to change.
 */
public class Origin {
    private static final int HTTP_DEFAULT_PORT = 80;
    private static final int HTTPS_DEFAULT_PORT = 443;

    private final Uri mOrigin;

    private Origin(Uri origin) {
        mOrigin = origin;
    }

    /**
     * Constructs a canonical Origin from a String. Will return {@code null} for origins that are
     * not HTTP or HTTPS.
     */
    @Nullable
    public static Origin create(String uri) {
        return create(Uri.parse(uri));
    }

    /**
     * Constructs a canonical Origin from an Uri. Will return {@code null} for origins that are not
     * HTTP or HTTPS.
     */
    @Nullable
    public static Origin create(Uri uri) {
        if (uri == null || uri.getScheme() == null || uri.getAuthority() == null) {
            return null;
        }

        // This class can only correctly handle certain origins, see https://crbug.com/1019244.
        String scheme = uri.getScheme();
        if (!scheme.equals(UrlConstants.HTTP_SCHEME) && !scheme.equals(UrlConstants.HTTPS_SCHEME)) {
            return null;
        }

        // Make explicit ports implicit and remove any user:password.
        int port = uri.getPort();
        if (scheme.equals(UrlConstants.HTTP_SCHEME) && port == HTTP_DEFAULT_PORT) port = -1;
        if (scheme.equals(UrlConstants.HTTPS_SCHEME) && port == HTTPS_DEFAULT_PORT) port = -1;

        String authority = uri.getHost();
        if (port != -1) authority += ":" + port;

        try {
            return new Origin(uri.normalizeScheme()
                    .buildUpon()
                    .opaquePart("")
                    .fragment("")
                    .path("")
                    .encodedAuthority(authority)
                    .clearQuery()
                    .build());
        } catch (UnsupportedOperationException e) {
            return null;
        }
    }

    /**
     * Constructs a canonical Origin from a String, throwing an exception if parsing fails.
     */
    public static Origin createOrThrow(String uri) {
        return createOrThrow(Uri.parse(uri));
    }

    /**
     * Constructs a canonical Origin from an Uri, throwing an exception if parsing fails.
     */
    public static Origin createOrThrow(Uri uri) {
        Origin origin = Origin.create(uri);
        if (origin == null) throw new IllegalArgumentException("Could not parse: " + uri);
        return origin;
    }

    /*
     * Returns a Uri representing the Origin.
     */
    public Uri uri() {
        return mOrigin;
    }

    @Override
    public int hashCode() {
        return mOrigin.hashCode();
    }

    /**
     * Returns a String representing the Origin.
     */
    @Override
    public String toString() {
        return mOrigin.toString();
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) return true;
        if (other == null || getClass() != other.getClass()) return false;
        return mOrigin.equals(((Origin) other).mOrigin);
    }
}
