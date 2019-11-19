// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.util.UrlConstants;

import java.net.URI;
import java.net.URISyntaxException;

/** URI utilities. */
public class UriUtils {
    /**
     * Checks whether the given <code>method</code> string has the correct format to be a URI
     * payment method name. Does not perform complete URI validation.
     *
     * @param method The payment method name to check.
     * @return Whether the method name has the correct format to be a URI payment method name.
     */
    public static boolean looksLikeUriMethod(String method) {
        return method.startsWith(UrlConstants.HTTPS_URL_PREFIX)
                || method.startsWith(UrlConstants.HTTP_URL_PREFIX);
    }

    /**
     * Parses the input <code>method</code> into a URI payment method name. Returns null for
     * invalid URI format or a relative URI.
     *
     * @param method The payment method name to parse.
     * @return The parsed URI payment method name or null if not valid.
     */
    @Nullable
    public static URI parseUriFromString(String method) {
        URI uri;
        try {
            // Don't use java.net.URL, because it performs a synchronous DNS lookup in the
            // constructor.
            uri = new URI(method);
        } catch (URISyntaxException e) {
            return null;
        }

        if (!uri.isAbsolute()) return null;

        assert UrlConstants.HTTPS_SCHEME.equals(uri.getScheme())
                || UrlConstants.HTTP_SCHEME.equals(uri.getScheme());

        return uri;
    }

    /**
     * Returns the origin part of the given URI.
     *
     * @param uri The input URI for which the origin needs to be returned. Should not be null.
     * @return The origin of the input URI. Never null.
     */
    public static URI getOrigin(URI uri) {
        assert uri != null;

        String originString = uri.resolve("/").toString();

        // Strip the trailing slash.
        if (!originString.isEmpty() && originString.charAt(originString.length() - 1) == '/') {
            originString = originString.substring(0, originString.length() - 1);
        }

        URI origin = parseUriFromString(originString);
        assert origin != null;

        return origin;
    }

    private UriUtils() {}
}