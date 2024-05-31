// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.text.TextUtils;
import android.util.Patterns;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Provides public methods for detecting and eliding sensitive PII. */
public class PiiElider {
    private static final String EMAIL_ELISION = "XXX@EMAIL.ELIDED";

    private static final String URL_ELISION = "HTTP://WEBADDRESS.ELIDED";

    private static final String GOOD_IRI_CHAR = "a-zA-Z0-9\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF";

    private static final String IP_ADDRESS =
            "((25[0-5]|2[0-4][0-9]|[0-1][0-9]{2}|[1-9][0-9]|[1-9])\\.(25[0-5]|2[0-4]"
                    + "[0-9]|[0-1][0-9]{2}|[1-9][0-9]|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]"
                    + "[0-9]{2}|[1-9][0-9]|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1][0-9]{2}"
                    + "|[1-9][0-9]|[0-9]))";

    private static final String IRI =
            "[" + GOOD_IRI_CHAR + "]([" + GOOD_IRI_CHAR + "-]{0,61}[" + GOOD_IRI_CHAR + "]){0,1}";

    private static final String GOOD_GTLD_CHAR = "a-zA-Z\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF";
    private static final String GTLD = "[" + GOOD_GTLD_CHAR + "]{2,63}";
    private static final String HOST_NAME = "(" + IRI + "\\.)+" + GTLD;

    private static final String URI_ENCODED_CHAR = "(%[a-fA-F0-9]{2})";

    private static final String URI_CHAR = "([a-zA-Z0-9$_.+!*'(),;?&=-]|" + URI_ENCODED_CHAR + ")";

    private static final String PATH_CHAR =
            // Either a single valid path component character or a URI-encoded character.
            "(([" + GOOD_IRI_CHAR + ";/?:@&=#~.+!*'(),_-])|" + URI_ENCODED_CHAR + ")";

    private static final String URI_SCHEME =
            "((http|https|Http|Https|rtsp|Rtsp)://"
                    + "("
                    + URI_CHAR
                    + "{1,64}(:"
                    + URI_CHAR
                    + "{1,25})?@)?)";

    private static final String DOMAIN_NAME = "(" + HOST_NAME + "|" + IP_ADDRESS + ")";

    private static final String PORT = "(:\\d{1,5})";

    private static final String URL_WITH_OPTIONAL_SCHEME_AND_PORT =
            "(" + URI_SCHEME + "?" + DOMAIN_NAME + PORT + "?)";

    private static final String PATH_COMPONENT = "(" + PATH_CHAR + "+)";

    // Based on: http://www.faqs.org/rfcs/rfc2396.html#:~:text=Scheme%20Component
    private static final String INTENT_SCHEME = "[a-zA-Z][a-zA-Z0-9+.-]+://";

    private static final String INTENT = "(" + INTENT_SCHEME + PATH_COMPONENT + ")";

    private static final String URL_OR_INTENT =
            "(" + URL_WITH_OPTIONAL_SCHEME_AND_PORT + "|" + INTENT + ")";

    private static final Pattern WEB_URL =
            Pattern.compile(
                    "(\\b|^)" // Always start on a word boundary or start of string.
                            + "("
                            + URL_OR_INTENT
                            + ")" // Main URL or Intent scheme/domain/root path.
                            + "(/"
                            + PATH_CHAR
                            + "*)?" // Rest of the URI path.
                            + "(\\b|$)"); // Always end on a word boundary or end of string.

    private static final Pattern NOT_URLS_PATTERN =
            Pattern.compile(
                    ""
                            // When a class is not found it can fail to satisfy our isClass
                            // check but is still worth noting what it was.
                            + "^(?:Caused by: )?java\\.lang\\."
                            + "(?:ClassNotFoundException|NoClassDefFoundError):|"
                            // Ensure common local paths are not interpreted as URLs.
                            + "(?:[\"' ]/(?:apex|data|mnt|proc|sdcard|storage|system))/");

    private static final String IP_ELISION = "1.2.3.4";
    private static final String MAC_ELISION = "01:23:45:67:89:AB";
    private static final String CONSOLE_ELISION = "[ELIDED:CONSOLE(0)] ELIDED CONSOLE MESSAGE";

    private static final Pattern MAC_ADDRESS =
            Pattern.compile("([0-9a-fA-F]{2}[-:]+){5}[0-9a-fA-F]{2}");

    private static final Pattern CONSOLE_MSG = Pattern.compile("\\[\\w*:CONSOLE.*\\].*");

    private static final String[] APP_NAMESPACE =
            new String[] {"org.chromium.", "com.google.", "com.chrome."};

    private static final String[] SYSTEM_NAMESPACE =
            new String[] {
                "android.",
                "com.android.",
                "dalvik.",
                "java.",
                "javax.",
                "org.apache.",
                "org.json.",
                "org.w3c.dom.",
                "org.xml.",
                "org.xmlpull.",
                "System."
            };

    /**
     * Elides any emails in the specified {@link String} with
     * {@link #EMAIL_ELISION}.
     *
     * @param original String potentially containing emails.
     * @return String with elided emails.
     */
    public static String elideEmail(String original) {
        return Patterns.EMAIL_ADDRESS.matcher(original).replaceAll(EMAIL_ELISION);
    }

    /**
     * Elides any URLs in the specified {@link String} with
     * {@link #URL_ELISION}.
     *
     * @param original String potentially containing URLs.
     * @return String with elided URLs.
     */
    public static String elideUrl(String original) {
        // Url-matching is fussy. If something looks like an exception message, just return.
        if (NOT_URLS_PATTERN.matcher(original).find()) return original;
        StringBuilder buffer = new StringBuilder(original);
        Matcher matcher = WEB_URL.matcher(buffer);
        int start = 0;
        while (matcher.find(start)) {
            start = matcher.start();
            int end = matcher.end();
            String url = buffer.substring(start, end);
            if (!likelyToBeAppNamespace(url)
                    && !likelyToBeChromeApkName(url)
                    && !likelyToBeSystemNamespace(url)
                    && !likelyToBeClassOrMethodName(url)) {
                buffer.replace(start, end, URL_ELISION);
                end = start + URL_ELISION.length();
                matcher = WEB_URL.matcher(buffer);
            }
            start = end;
        }
        return buffer.toString();
    }

    private static boolean likelyToBeClassOrMethodName(String url) {
        if (isClassName(url)) return true;

        // Since the suspected URL could actually be a method name, check if the portion preceding
        // the last subdomain is a class name.
        int indexOfLastPeriod = url.lastIndexOf(".");
        if (indexOfLastPeriod == -1) return false;
        return isClassName(url.substring(0, indexOfLastPeriod));
    }

    private static boolean likelyToBeChromeApkName(String url) {
        // Our Java stacktraces always contain a line that looks like:
        // at Z94.e(chromium-TrichromeChromeGoogle6432.aab-canary-651000033:14)
        if (url.startsWith("chromium-") && (url.endsWith(".apk") || url.endsWith(".aab"))) {
            return true;
        }
        return false;
    }

    private static boolean isClassName(String url) {
        try {
            Class.forName(url, false, ContextUtils.getApplicationContext().getClassLoader());
            return true;
        } catch (Throwable e) {
            // Some examples: ClassNotFoundException, NoClassDefFoundException, VerifyError.
        }
        return false;
    }

    private static boolean likelyToBeAppNamespace(String url) {
        for (String ns : APP_NAMESPACE) {
            if (url.startsWith(ns)) {
                return true;
            }
        }
        return false;
    }

    private static boolean likelyToBeSystemNamespace(String url) {
        for (String ns : SYSTEM_NAMESPACE) {
            if (url.startsWith(ns)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Elides any IP addresses in the specified {@link String} with
     * {@link #IP_ELISION}.
     *
     * @param original String potentially containing IPs.
     * @return String with elided IPs.
     */
    public static String elideIp(String original) {
        return Patterns.IP_ADDRESS.matcher(original).replaceAll(IP_ELISION);
    }

    /**
     * Elides any MAC addresses in the specified {@link String} with
     * {@link #MAC_ELISION}.
     *
     * @param original String potentially containing MACs.
     * @return String with elided MACs.
     */
    public static String elideMac(String original) {
        return MAC_ADDRESS.matcher(original).replaceAll(MAC_ELISION);
    }

    /**
     * Elides any console messages in the specified {@link String} with
     * {@link #CONSOLE_ELISION}.
     *
     * @param original String potentially containing console messages.
     * @return String with elided console messages.
     */
    public static String elideConsole(String original) {
        return CONSOLE_MSG.matcher(original).replaceAll(CONSOLE_ELISION);
    }

    /**
     * Elides any URL in the exception messages contained inside a stacktrace with
     * {@link #URL_ELISION}.
     *
     * @param stacktrace Multiline stacktrace as a string.
     * @return Stacktrace with elided URLs.
     */
    public static String sanitizeStacktrace(String stacktrace) {
        if (TextUtils.isEmpty(stacktrace)) {
            return "";
        }
        String[] lines = stacktrace.split("\\n");
        boolean foundAtLine = false;
        for (int i = 0; i < lines.length; i++) {
            if (lines[i].startsWith("\tat ")) {
                foundAtLine = true;
            } else {
                lines[i] = elideUrl(lines[i]);
            }
        }
        // Guard against non-properly formatted stacktraces to ensure checking for "\tat :" is
        // sufficient (e.g.: no logging-related line prefixes).
        // There can be no frames when a native thread creates an exception using JNI.
        assert foundAtLine || lines.length == 1 : "Was not a stack trace: " + stacktrace;
        return TextUtils.join("\n", lines);
    }
}
