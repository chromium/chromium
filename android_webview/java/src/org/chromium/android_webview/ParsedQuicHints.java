// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.InetAddresses;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.util.HashSet;
import java.util.Set;

/** Holds parsed and validated QUIC hint parameters for {@link AwBrowserContext#addQuicHints}. */
@Lifetime.Temporary
@NullMarked
public final class ParsedQuicHints {
    private static final String HTTPS_SCHEME_PREFIX = "https://";

    public final GURL[] exactOrigins;
    public final String[] wildcardSuffixes;
    public final boolean tryQuicByDefault;

    private ParsedQuicHints(
            GURL[] exactOrigins, String[] wildcardSuffixes, boolean tryQuicByDefault) {
        this.exactOrigins = exactOrigins;
        this.wildcardSuffixes = wildcardSuffixes;
        this.tryQuicByDefault = tryQuicByDefault;
    }

    // Parses and validates a set of QUIC hint strings into exact origins, wildcard domain suffixes,
    // and the catch-all tryQuicByDefault flag.
    public static ParsedQuicHints parse(@Nullable Set<@Nullable String> origins) {
        if (origins == null) {
            throw new IllegalArgumentException("origins cannot be null");
        }

        Set<GURL> exactOrigins = new HashSet<>();
        Set<String> wildcardSuffixes = new HashSet<>();
        boolean tryQuicByDefault = false;

        for (String origin : origins) {
            if (origin == null || origin.trim().isEmpty()) {
                throw new IllegalArgumentException("Origin cannot be null or empty");
            }
            String trimmed = origin.trim();

            // Example: "*" or "https://*" -> tryQuicByDefault = true
            if (trimmed.equals("*") || trimmed.equalsIgnoreCase("https://*")) {
                tryQuicByDefault = true;
                continue;
            }

            // Mandate explicit "https://" scheme prefix (aligning with OriginMatcher).
            // Non-HTTPS schemes (e.g., "http://") and bare patterns without scheme are rejected.
            if (!trimmed.regionMatches(
                    /* ignoreCase= */ true,
                    0,
                    HTTPS_SCHEME_PREFIX,
                    0,
                    HTTPS_SCHEME_PREFIX.length())) {
                throw new IllegalArgumentException(
                        "Invalid origin (must use https:// scheme): " + origin);
            }

            // Strip the "https://" prefix to check for a leading "*." wildcard pattern.
            // Example: "https://*.example.com:8443/path" -> "*.example.com:8443/path"
            String afterScheme = trimmed.substring(HTTPS_SCHEME_PREFIX.length());
            String wildcardSuffix = parseWildcard(afterScheme, origin);
            if (wildcardSuffix != null) {
                if (!tryQuicByDefault) {
                    wildcardSuffixes.add(wildcardSuffix);
                }
                // For strict parity with OriginMatcher, wildcards match
                // subdomains only. The apex origin is not automatically added.
                continue;
            }

            // Non-wildcard input (including IPv6 literals like "https://[::1]:8443"):
            // validate and store as a full GURL.
            GURL gurl = new GURL(trimmed);
            if (GURL.isEmptyOrInvalid(gurl) || "0".equals(gurl.getPort())) {
                throw new IllegalArgumentException("Invalid origin: " + origin);
            }

            if (!tryQuicByDefault) {
                exactOrigins.add(gurl);
            }
        }

        // If the catch-all wildcard ("*" or "https://*") was provided, native only needs
        // tryQuicByDefault = true; avoid passing redundant arrays over JNI.
        if (tryQuicByDefault) {
            return new ParsedQuicHints(new GURL[0], new String[0], true);
        }

        return new ParsedQuicHints(
                exactOrigins.toArray(new GURL[0]), wildcardSuffixes.toArray(new String[0]), false);
    }

    private static @Nullable String parseWildcard(String afterScheme, String origin) {
        if (!afterScheme.startsWith("*.")) {
            return null;
        }

        // Replace the leading "*." with "https://" so GURL parses and canonicalizes the
        // host, port, path, query, and fragment.
        // Example: "*.EXAMPLE.COM:8443/path?q=1#frag" -> "https://EXAMPLE.COM:8443/path?q=1#frag"
        String afterWildcard = afterScheme.substring(2);
        if (afterWildcard.startsWith("/") || afterWildcard.startsWith("\\")) {
            throw new IllegalArgumentException("Invalid origin: " + origin);
        }

        GURL gurl = new GURL(HTTPS_SCHEME_PREFIX + afterWildcard);
        String host = gurl.getHost();
        String port = gurl.getPort();
        if (GURL.isEmptyOrInvalid(gurl)
                || !gurl.getUsername().isEmpty()
                || !gurl.getPassword().isEmpty()
                || "0".equals(port)
                || host.isEmpty()
                || host.startsWith(".")
                || host.contains("..")
                || host.startsWith("[")
                || InetAddresses.isNumericAddress(host)) {
            throw new IllegalArgumentException("Invalid origin: " + origin);
        }

        // Form the canonical domain suffix (and non-default port if present) passed to native.
        // Example: "*.example.com:8443" -> ".example.com:8443"
        return port.isEmpty() ? "." + host : "." + host + ":" + port;
    }
}
