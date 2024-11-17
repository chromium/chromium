// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to record histogram to determine whether the Custom Tab was launched with what looks
 * like an OAuth URL.
 */
public class CustomTabAuthUrlHeuristics {
    // This should be kept in sync with the definition |CustomTabsAuthScheme| in
    // tools/metrics/histograms/metadata/custom_tabs/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({AuthScheme.OTHER, AuthScheme.HTTP, AuthScheme.HTTPS})
    @Retention(RetentionPolicy.SOURCE)
    @interface AuthScheme {
        int OTHER = 0;
        int HTTP = 1;
        int HTTPS = 2;
        int COUNT = 3;
    }

    // Bit flags for potential OAuth params.
    private static final int FLAG_EMPTY = 0;
    private static final int FLAG_CLIENT_ID = 1 << 0;
    private static final int FLAG_REDIRECT_URI = 1 << 1;
    private static final int FLAG_RESPONSE_TYPE = 1 << 2;
    private static final int FLAG_SCOPE = 1 << 3;
    private static final int FLAG_STATE = 1 << 4;

    // Strings for potential OAuth params.
    private static final String CLIENT_ID = "client_id";
    private static final String REDIRECT_URI = "redirect_uri";
    private static final String RESPONSE_TYPE = "response_type";
    private static final String SCOPE = "scope";
    private static final String STATE = "state";

    private static final String CUSTOM_TABS_AUTH_VIEW_URL_PARAMS = "CustomTabs.AuthView.UrlParams";

    /**
     * Extracts potential OAuth params from the provided URL and records a bit field with each bit
     * representing the presence of a param.
     *
     * @param url The url to extract query params from.
     */
    public static void recordUrlParamsHistogram(String url) {
        if (TextUtils.isEmpty(url)) return;
        Uri uri = Uri.parse(url);

        try {
            var params = uri.getQueryParameterNames();

            int flags = FLAG_EMPTY;
            if (params.contains(CLIENT_ID)) {
                flags |= FLAG_CLIENT_ID;
            }
            if (params.contains(REDIRECT_URI)) {
                flags |= FLAG_REDIRECT_URI;
            }
            if (params.contains(RESPONSE_TYPE)) {
                flags |= FLAG_RESPONSE_TYPE;
            }
            if (params.contains(SCOPE)) {
                flags |= FLAG_SCOPE;
            }
            if (params.contains(STATE)) {
                flags |= FLAG_STATE;
            }
            RecordHistogram.recordSparseHistogram(CUSTOM_TABS_AUTH_VIEW_URL_PARAMS, flags);
        } catch (UnsupportedOperationException ignored) {
        }
    }

    public static void setFirstCctPageLoadForMetrics(Tab tab) {
        CustomTabAuthUrlHeuristicsJni.get().setFirstCctPageLoadForPasswords(tab);
    }

    public static void recordRedirectUriSchemeHistogram(
            BrowserServicesIntentDataProvider intentDataProvider) {
        if (intentDataProvider.isAuthTab()) return;

        String url = intentDataProvider.getUrlToLoad();
        if (TextUtils.isEmpty(url)) return;

        Uri uri = Uri.parse(url);
        if (!uri.isHierarchical()) return;

        String redirectUri = uri.getQueryParameter("redirect_uri");
        if (TextUtils.isEmpty(redirectUri)) return;

        int schemeEnum = getAuthSchemeEnum(Uri.parse(redirectUri).getScheme());
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.AuthTab.RedirectUriScheme",
                schemeEnum,
                CustomTabAuthUrlHeuristics.AuthScheme.COUNT);
    }

    static @CustomTabAuthUrlHeuristics.AuthScheme int getAuthSchemeEnum(String scheme) {
        if (UrlConstants.HTTP_SCHEME.equalsIgnoreCase(scheme)) {
            return CustomTabAuthUrlHeuristics.AuthScheme.HTTP;
        } else if (UrlConstants.HTTPS_SCHEME.equalsIgnoreCase(scheme)) {
            return CustomTabAuthUrlHeuristics.AuthScheme.HTTPS;
        } else {
            return CustomTabAuthUrlHeuristics.AuthScheme.OTHER;
        }
    }

    @NativeMethods
    public interface Natives {
        void setFirstCctPageLoadForPasswords(Tab tab);
    }
}
