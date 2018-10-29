// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.net.Uri;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import org.chromium.base.CollectionUtil;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.content_public.common.ContentUrlConstants;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URLDecoder;
import java.util.HashSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Utilities for working with URIs (and URLs). These methods may be used in security-sensitive
 * contexts (after all, origins are the security boundary on the web), and so the correctness bar
 * must be high.
 *
 * Use ShadowUrlUtilities to mock out native-dependent methods in tests.
 * TODO(pshmakov): we probably should just make those methods non-static.
 */
public class UrlUtilities {
    private static final String TAG = "UrlUtilities";

    /**
     * URI schemes that are internal to Chrome.
     */
    private static final HashSet<String> INTERNAL_SCHEMES =
            CollectionUtil.newHashSet(UrlConstants.CHROME_SCHEME, UrlConstants.CHROME_NATIVE_SCHEME,
                    ContentUrlConstants.ABOUT_SCHEME);

    // Patterns used in validateIntentUrl.
    private static final Pattern DNS_HOSTNAME_PATTERN =
            Pattern.compile("^[\\w\\.-]*$");
    private static final Pattern JAVA_PACKAGE_NAME_PATTERN =
            Pattern.compile("^[\\w\\.-]*$");
    private static final Pattern ANDROID_COMPONENT_NAME_PATTERN =
            Pattern.compile("^[\\w\\./-]*$");
    private static final Pattern URL_SCHEME_PATTERN =
            Pattern.compile("^[a-zA-Z]+$");

    private static final String TEL_URL_PREFIX = "tel:";

    /**
     * @param uri A URI.
     *
     * @return True if the URI's scheme is phone number scheme.
     */
    public static boolean isTelScheme(String uri) {
        return uri != null && uri.startsWith(TEL_URL_PREFIX);
    }

    /**
     * @param uri A URI.
     *
     * @return The string after tel: scheme. Normally, it should be a phone number, but isn't
     *         guaranteed.
     */
    public static String getTelNumber(String uri) {
        if (uri == null || !uri.contains(":")) return "";
        return uri.split(":")[1];
    }

    /**
     * @param uri A URI.
     *
     * @return True if the URI's scheme is one that ContentView can handle.
     */
    public static boolean isAcceptedScheme(String uri) {
        return nativeIsAcceptedScheme(uri);
    }

    /**
     * @param uri A URI.
     *
     * @return True if the URI is valid for Intent fallback navigation.
     */
    public static boolean isValidForIntentFallbackNavigation(String uri) {
        return nativeIsValidForIntentFallbackNavigation(uri);
    }

    /**
     * @param uri A URI.
     *
     * @return True if the URI's scheme is one that Chrome can download.
     */
    public static boolean isDownloadableScheme(String uri) {
        return nativeIsDownloadable(uri);
    }

    /**
     * @param uri A URI.
     *
     * @return Whether the URI's scheme is for a internal chrome page.
     */
    public static boolean isInternalScheme(URI uri) {
        return INTERNAL_SCHEMES.contains(uri.getScheme());
    }

    /**
     * @param url A URL.
     *
     * @return Whether the URL's scheme is HTTP or HTTPS.
     */
    public static boolean isHttpOrHttps(@NonNull String url) {
        // URI#getScheme would throw URISyntaxException if the other parts contain invalid
        // characters. For example, "http://foo.bar/has[square].html" has [] in the path, which
        // is not valid in URI. Both Uri.parse().getScheme() and URL().getProtocol() work in
        // this case.
        //
        // URL().getProtocol() throws MalformedURLException if the scheme is "invalid",
        // including common ones like "about:", "javascript:", "data:", etc.
        String scheme = Uri.parse(url).getScheme();
        return UrlConstants.HTTP_SCHEME.equals(scheme) || UrlConstants.HTTPS_SCHEME.equals(scheme);
    }

    /**
     * Determines whether or not the given URLs belong to the same broad domain or host.
     * "Broad domain" is defined as the TLD + 1 or the host.
     *
     * For example, the TLD + 1 for http://news.google.com would be "google.com" and would be shared
     * with other Google properties like http://finance.google.com.
     *
     * If {@code includePrivateRegistries} is marked as true, then private domain registries (like
     * appspot.com) are considered "effective TLDs" -- all subdomains of appspot.com would be
     * considered distinct (effective TLD = ".appspot.com" + 1).
     * This means that http://chromiumreview.appspot.com and http://example.appspot.com would not
     * belong to the same host.
     * If {@code includePrivateRegistries} is false, all subdomains of appspot.com
     * would be considered to be the same domain (TLD = ".com" + 1).
     *
     * @param primaryUrl First URL
     * @param secondaryUrl Second URL
     * @param includePrivateRegistries Whether or not to consider private registries.
     * @return True iff the two URIs belong to the same domain or host.
     */
    public static boolean sameDomainOrHost(String primaryUrl, String secondaryUrl,
            boolean includePrivateRegistries) {
        return nativeSameDomainOrHost(primaryUrl, secondaryUrl, includePrivateRegistries);
    }

    /**
     * This function works by calling net::registry_controlled_domains::GetDomainAndRegistry
     *
     * @param uri A URI
     * @param includePrivateRegistries Whether or not to consider private registries.
     *
     * @return The registered, organization-identifying host and all its registry information, but
     * no subdomains, from the given URI. Returns an empty string if the URI is invalid, has no host
     * (e.g. a file: URI), has multiple trailing dots, is an IP address, has only one subcomponent
     * (i.e. no dots other than leading/trailing ones), or is itself a recognized registry
     * identifier.
     */
    public static String getDomainAndRegistry(String uri, boolean includePrivateRegistries) {
        if (TextUtils.isEmpty(uri)) return uri;
        return nativeGetDomainAndRegistry(uri, includePrivateRegistries);
    }

    /** Returns whether a URL is within another URL's scope. */
    @VisibleForTesting
    public static boolean isUrlWithinScope(String url, String scopeUrl) {
        return nativeIsUrlWithinScope(url, scopeUrl);
    }

    /** @return whether two URLs match, ignoring the #fragment. */
    @VisibleForTesting
    public static boolean urlsMatchIgnoringFragments(String url, String url2) {
        if (TextUtils.equals(url, url2)) return true;
        return nativeUrlsMatchIgnoringFragments(url, url2);
    }

    /** @return whether the #fragmant differs in two URLs. */
    @VisibleForTesting
    public static boolean urlsFragmentsDiffer(String url, String url2) {
        if (TextUtils.equals(url, url2)) return false;
        return nativeUrlsFragmentsDiffer(url, url2);
    }

    /**
     * @param url An Android intent:// URL to validate.
     *
     * @throws URISyntaxException if url is not a valid Android intent://
     * URL, as specified at
     * https://developer.chrome.com/multidevice/android/intents#syntax.
     */
    @VisibleForTesting
    public static boolean validateIntentUrl(String url) {
        if (url == null) {
            Log.d(TAG, "url was null");
            return false;
        }

        URI parsed;
        try {
            parsed = new URI(url);
        } catch (URISyntaxException e) {
            // It may be that we received a URI of the form "intent:#Intent...",
            // which e.g. Google Authenticator produces. Work around that
            // specific case.
            if (url.indexOf("intent:#Intent;") == 0) {
                return validateIntentUrl(url.replace("intent:#Intent;", "intent://foo/#Intent;"));
            }

            Log.d(TAG, "Could not parse url '%s': %s", url, e.toString());
            return false;
        }

        String scheme = parsed.getScheme();
        if (scheme == null || !scheme.equals("intent")) {
            Log.d(TAG, "scheme was not 'intent'");
            return false;
        }

        String hostname = parsed.getHost();
        if (hostname == null) {
            Log.d(TAG, "hostname was null for '%s'", url);
            return false;
        }
        Matcher m = DNS_HOSTNAME_PATTERN.matcher(hostname);
        if (!m.matches()) {
            Log.d(TAG, "hostname did not match DNS_HOSTNAME_PATTERN");
            return false;
        }

        String path = parsed.getPath();
        if (path == null || (!path.isEmpty() && !path.equals("/"))) {
            Log.d(TAG, "path was null or not \"/\"");
            return false;
        }

        // We need to get the raw, unparsed, un-URL-decoded fragment.
        // parsed.getFragment() returns a URL-decoded fragment, which can
        // interfere with lexing and parsing Intent extras correctly. Therefore,
        // we handle the fragment "manually", but first assert that it
        // URL-decodes correctly.
        int fragmentStart = url.indexOf('#');
        if (fragmentStart == -1 || fragmentStart == url.length() - 1) {
            Log.d(TAG, "Could not find '#'");
            return false;
        }
        String fragment = url.substring(url.indexOf('#') + 1);
        try {
            String f = parsed.getFragment();
            if (f == null) {
                Log.d(TAG, "Could not get fragment from parsed URL");
                return false;
            }
            if (!URLDecoder.decode(fragment, "UTF-8").equals(f)) {
                Log.d(TAG, "Parsed fragment does not equal lexed fragment");
                return false;
            }
        } catch (UnsupportedEncodingException e) {
            Log.d(TAG, e.toString());
            return false;
        }

        // Now lex and parse the correctly-encoded fragment.
        String[] parts = fragment.split(";");
        if (parts.length < 3
                || !parts[0].equals("Intent")
                || !parts[parts.length - 1].equals("end")) {
            Log.d(TAG, "Invalid fragment (not enough parts, lacking Intent, or lacking end)");
            return false;
        }

        boolean seenPackage = false;
        boolean seenAction = false;
        boolean seenCategory = false;
        boolean seenComponent = false;
        boolean seenScheme = false;

        for (int i = 1; i < parts.length - 1; ++i) {
            // This is OK *only* because no valid package, action, category,
            // component, or scheme contains (unencoded) "=".
            String[] pair = parts[i].split("=");
            if (2 != pair.length) {
                Log.d(TAG, "Invalid key=value pair '%s'", parts[i]);
                return false;
            }

            m = JAVA_PACKAGE_NAME_PATTERN.matcher(pair[1]);
            if (pair[0].equals("package")) {
                if (seenPackage || !m.matches()) {
                    Log.d(TAG, "Invalid package '%s'", pair[1]);
                    return false;
                }
                seenPackage = true;
            } else if (pair[0].equals("action")) {
                if (seenAction || !m.matches()) {
                    Log.d(TAG, "Invalid action '%s'", pair[1]);
                    return false;
                }
                seenAction = true;
            } else if (pair[0].equals("category")) {
                if (seenCategory || !m.matches()) {
                    Log.d(TAG, "Invalid category '%s'", pair[1]);
                    return false;
                }
                seenCategory = true;
            } else if (pair[0].equals("component")) {
                Matcher componentMatcher = ANDROID_COMPONENT_NAME_PATTERN.matcher(pair[1]);
                if (seenComponent || !componentMatcher.matches()) {
                    Log.d(TAG, "Invalid component '%s'", pair[1]);
                    return false;
                }
                seenComponent = true;
            } else if (pair[0].equals("scheme")) {
                if (seenScheme) return false;
                Matcher schemeMatcher = URL_SCHEME_PATTERN.matcher(pair[1]);
                if (!schemeMatcher.matches()) {
                    Log.d(TAG, "Invalid scheme '%s'", pair[1]);
                    return false;
                }
                seenScheme = true;
            } else {
                // Assume we are seeing an Intent Extra. Up above, we ensured
                // that the #Intent... fragment was correctly URL-encoded;
                // beyond that, there is no further validation we can do. Extras
                // are blobs to us.
                continue;
            }
        }

        return true;
    }

    /**
     * @param url An HTTP or HTTPS URL.
     * @return The URL without path and query.
     */
    public static String stripPath(String url) {
        assert isHttpOrHttps(url);
        Uri parsed = Uri.parse(url);

        return parsed.getScheme() + "://" + ((parsed.getHost() != null) ? parsed.getHost() : "")
                + ((parsed.getPort() != -1) ? (":" + parsed.getPort()) : "");
    }

    /**
     * @param url An HTTP or HTTPS URL.
     * @return The URL without the scheme.
     */
    public static String stripScheme(String url) {
        String noScheme = url.trim();
        if (noScheme.startsWith(UrlConstants.HTTPS_URL_PREFIX)) {
            noScheme = noScheme.substring(8);
        } else if (noScheme.startsWith(UrlConstants.HTTP_URL_PREFIX)) {
            noScheme = noScheme.substring(7);
        }
        return noScheme;
    }

    private static native boolean nativeIsDownloadable(String url);
    private static native boolean nativeIsValidForIntentFallbackNavigation(String url);
    private static native boolean nativeIsAcceptedScheme(String url);
    private static native boolean nativeSameDomainOrHost(String primaryUrl, String secondaryUrl,
            boolean includePrivateRegistries);
    private static native String nativeGetDomainAndRegistry(String url,
            boolean includePrivateRegistries);
    public static native boolean nativeIsGoogleSearchUrl(String url);
    public static native boolean nativeIsGoogleHomePageUrl(String url);
    private static native boolean nativeIsUrlWithinScope(String url, String scopeUrl);
    private static native boolean nativeUrlsMatchIgnoringFragments(String url, String url2);
    private static native boolean nativeUrlsFragmentsDiffer(String url, String url2);
}
