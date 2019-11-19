// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.Resources;
import android.text.Spannable;
import android.text.style.ForegroundColorSpan;
import android.text.style.StrikethroughSpan;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.components.security_state.ConnectionSecurityLevel;

import java.util.Locale;

/**
 * A helper class that emphasizes the various components of a URL. Useful in the
 * Omnibox and Page Info popup where different parts of the URL should appear
 * in different colours depending on the scheme, host and connection.
 */
public class OmniboxUrlEmphasizer {

    /**
     * Describes the components of a URL that should be emphasized.
     */
    public static class EmphasizeComponentsResponse {
        /** The start index of the scheme. */
        public final int schemeStart;
        /** The length of the scheme. */
        public final int schemeLength;
        /** The start index of the host. */
        public final int hostStart;
        /** The length of the host. */
        public final int hostLength;

        EmphasizeComponentsResponse(
                int schemeStart, int schemeLength, int hostStart, int hostLength) {
            this.schemeStart = schemeStart;
            this.schemeLength = schemeLength;
            this.hostStart = hostStart;
            this.hostLength = hostLength;
        }

        /**
         * @return Whether the URL has a scheme to be emphasized.
         */
        public boolean hasScheme() {
            return schemeLength > 0;
        }

        /**
         * @return Whether the URL has a host to be emphasized.
         */
        public boolean hasHost() {
            return hostLength > 0;
        }

        /**
         * @return The scheme extracted from |url|, canonicalized to lowercase.
         */
        public String extractScheme(String url) {
            if (!hasScheme()) return "";
            return url.subSequence(schemeStart, schemeStart + schemeLength)
                    .toString()
                    .toLowerCase(Locale.US);
        }
    }

    /**
     * Parses the |text| passed in and determines the location of the scheme and
     * host components to be emphasized.
     *
     * @param profile The profile to be used for parsing.
     * @param text The text to be parsed for emphasis components.
     * @return The response object containing the locations of the emphasis
     *         components.
     */
    public static EmphasizeComponentsResponse parseForEmphasizeComponents(
            Profile profile, String text) {
        int[] emphasizeValues =
                OmniboxUrlEmphasizerJni.get().parseForEmphasizeComponents(profile, text);
        assert emphasizeValues != null;
        assert emphasizeValues.length == 4;

        return new EmphasizeComponentsResponse(
                emphasizeValues[0], emphasizeValues[1], emphasizeValues[2], emphasizeValues[3]);
    }

    /**
     * Denotes that a span is used for emphasizing the URL.
     */
    @VisibleForTesting
    public interface UrlEmphasisSpan {}

    /**
     * Used for emphasizing the URL text by changing the text color.
     */
    @VisibleForTesting
    static class UrlEmphasisColorSpan extends ForegroundColorSpan
            implements UrlEmphasisSpan {
        private int mEmphasisColor;

        /**
         * @param color The color to set the text.
         */
        public UrlEmphasisColorSpan(int color) {
            super(color);
            mEmphasisColor = color;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof UrlEmphasisColorSpan)) return false;
            return ((UrlEmphasisColorSpan) obj).mEmphasisColor == mEmphasisColor;
        }

        @Override
        public String toString() {
            return getClass().getName() + ", color: " + mEmphasisColor;
        }
    }

    /**
     * Used for emphasizing the URL text by striking through the https text.
     */
    @VisibleForTesting
    static class UrlEmphasisSecurityErrorSpan extends StrikethroughSpan
            implements UrlEmphasisSpan {
        @Override
        public boolean equals(Object obj) {
            return obj instanceof UrlEmphasisSecurityErrorSpan;
        }
    }

    /**
     * Modifies the given URL to emphasize the host and scheme.
     * TODO(sashab): Make this take an EmphasizeComponentsResponse object to
     *               prevent calling parseForEmphasizeComponents() again.
     *
     * @param url The URL spannable to add emphasis to. This variable is
     *            modified.
     * @param resources Resources for the given application context.
     * @param profile The profile viewing the given URL.
     * @param securityLevel A valid ConnectionSecurityLevel for the specified
     *                      web contents.
     * @param isInternalPage Whether this page is an internal Chrome page.
     * @param useDarkColors Whether the text colors should be dark (i.e.
     *                      appropriate for use on a light background).
     * @param emphasizeScheme Whether the scheme should be emphasized.
     */
    public static void emphasizeUrl(Spannable url, Resources resources, Profile profile,
            int securityLevel, boolean isInternalPage, boolean useDarkColors,
            boolean emphasizeScheme) {
        String urlString = url.toString();
        EmphasizeComponentsResponse emphasizeResponse =
                parseForEmphasizeComponents(profile, urlString);

        int nonEmphasizedColorId = R.color.url_emphasis_non_emphasized_text;
        if (!useDarkColors) {
            nonEmphasizedColorId = R.color.url_emphasis_light_non_emphasized_text;
        }

        int startSchemeIndex = emphasizeResponse.schemeStart;
        int endSchemeIndex = emphasizeResponse.schemeStart + emphasizeResponse.schemeLength;

        int startHostIndex = emphasizeResponse.hostStart;
        int endHostIndex = emphasizeResponse.hostStart + emphasizeResponse.hostLength;

        // Format the scheme, if present, based on the security level.
        ForegroundColorSpan span;
        if (emphasizeResponse.hasScheme()) {
            int colorId = nonEmphasizedColorId;
            if (!isInternalPage) {
                boolean strikeThroughScheme = false;
                switch (securityLevel) {
                    case ConnectionSecurityLevel.NONE:
                    // Intentional fall-through:
                    case ConnectionSecurityLevel.WARNING:
                        // Draw attention to the data: URI scheme for anti-spoofing reasons.
                        if (UrlConstants.DATA_SCHEME.equals(
                                    emphasizeResponse.extractScheme(urlString))) {
                            colorId = useDarkColors ? R.color.default_text_color_dark
                                                    : R.color.default_text_color_light;
                        }
                        break;
                    case ConnectionSecurityLevel.DANGEROUS:
                        if (emphasizeScheme) {
                            colorId = useDarkColors ? R.color.default_red_dark
                                                    : R.color.default_red_light;
                        }
                        strikeThroughScheme = true;
                        break;
                    case ConnectionSecurityLevel.EV_SECURE:
                    // Intentional fall-through:
                    case ConnectionSecurityLevel.SECURE:
                        if (emphasizeScheme) {
                            colorId = useDarkColors ? R.color.default_green_dark
                                                    : R.color.default_green_light;
                        }
                        break;
                    default:
                        assert false;
                }

                if (strikeThroughScheme) {
                    UrlEmphasisSecurityErrorSpan ss = new UrlEmphasisSecurityErrorSpan();
                    url.setSpan(ss, startSchemeIndex, endSchemeIndex,
                            Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
                }
            }
            span = new UrlEmphasisColorSpan(ApiCompatibilityUtils.getColor(resources, colorId));
            url.setSpan(
                    span, startSchemeIndex, endSchemeIndex, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

            // Highlight the portion of the URL visible between the scheme and the host,
            // typically :// or : depending on the scheme.
            if (emphasizeResponse.hasHost()) {
                span = new UrlEmphasisColorSpan(
                        ApiCompatibilityUtils.getColor(resources, nonEmphasizedColorId));
                url.setSpan(span, endSchemeIndex, startHostIndex,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }

        if (emphasizeResponse.hasHost()) {
            // Highlight the complete host.
            int hostColorId = R.color.url_emphasis_domain_and_registry;
            if (!useDarkColors) {
                hostColorId = R.color.url_emphasis_light_domain_and_registry;
            }
            span = new UrlEmphasisColorSpan(ApiCompatibilityUtils.getColor(resources, hostColorId));
            url.setSpan(span, startHostIndex, endHostIndex, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

            // Highlight the remainder of the URL.
            if (endHostIndex < urlString.length()) {
                span = new UrlEmphasisColorSpan(
                        ApiCompatibilityUtils.getColor(resources, nonEmphasizedColorId));
                url.setSpan(span, endHostIndex, urlString.length(),
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        } else if (UrlConstants.DATA_SCHEME.equals(emphasizeResponse.extractScheme(urlString))) {
            // Dim the remainder of the URL for anti-spoofing purposes.
            span = new UrlEmphasisColorSpan(
                    ApiCompatibilityUtils.getColor(resources, nonEmphasizedColorId));
            url.setSpan(span, emphasizeResponse.schemeStart + emphasizeResponse.schemeLength,
                    urlString.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
    }

    /**
     * Reset the modifications done to emphasize components of the URL.
     *
     * @param url The URL spannable to remove emphasis from. This variable is
     *            modified.
     */
    public static void deEmphasizeUrl(Spannable url) {
        UrlEmphasisSpan[] emphasisSpans = getEmphasisSpans(url);
        if (emphasisSpans.length == 0) return;
        for (UrlEmphasisSpan span : emphasisSpans) {
            url.removeSpan(span);
        }
    }

    /**
     * Returns whether the given URL has any emphasis spans applied.
     *
     * @param url The URL spannable to check emphasis on.
     * @return True if the URL has emphasis spans, false if not.
     */
    public static boolean hasEmphasisSpans(Spannable url) {
        return getEmphasisSpans(url).length != 0;
    }

    /**
     * Returns the emphasis spans applied to the URL.
     *
     * @param url The URL spannable to get spans for.
     * @return The spans applied to the URL with emphasizeUrl().
     */
    public static UrlEmphasisSpan[] getEmphasisSpans(Spannable url) {
        return url.getSpans(0, url.length(), UrlEmphasisSpan.class);
    }

    /**
     * Returns the index of the first character containing non-origin
     * information, or 0 if the URL does not contain an origin.
     *
     * For "data" URLs, the URL is not considered to contain an origin.
     * For non-http and https URLs, the whole URL is considered the origin.
     *
     * For example, HTTP and HTTPS urls return the index of the first character
     * after the domain:
     *   http://www.google.com/?q=foo => 21 (up to the 'm' in google.com)
     *   https://www.google.com/?q=foo => 22
     *
     * Data urls always return 0, since they do not contain an origin:
     *   data:kf94hfJEj#N => 0
     *
     * Other URLs treat the whole URL as an origin:
     *   file://my/pc/somewhere/foo.html => 31
     *   about:blank => 11
     *   chrome://version => 18
     *   chrome-native://bookmarks => 25
     *   invalidurl => 10
     *
     * TODO(sashab): Make this take an EmphasizeComponentsResponse object to
     *               prevent calling parseForEmphasizeComponents() again.
     *
     * @param url The URL to find the last origin character in.
     * @param profile The profile visiting this URL (used for parsing the URL).
     * @return The index of the last character containing origin information.
     */
    public static int getOriginEndIndex(String url, Profile profile) {
        EmphasizeComponentsResponse emphasizeResponse =
                parseForEmphasizeComponents(profile, url.toString());
        if (!emphasizeResponse.hasScheme()) return url.length();

        String scheme = emphasizeResponse.extractScheme(url);

        if (scheme.equals(UrlConstants.HTTP_SCHEME) || scheme.equals(UrlConstants.HTTPS_SCHEME)) {
            return emphasizeResponse.hostStart + emphasizeResponse.hostLength;
        } else if (scheme.equals(UrlConstants.DATA_SCHEME)) {
            return 0;
        } else {
            return url.length();
        }
    }

    @NativeMethods
    interface Natives {
        int[] parseForEmphasizeComponents(Profile profile, String text);
    }
}
