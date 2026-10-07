// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_callback_glue;

import android.graphics.Color;
import android.webkit.WebChromeClient;
import android.webkit.WebView;

import androidx.annotation.Nullable;

import org.jspecify.annotations.NullMarked;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.support_lib_boundary.WebChromeClientBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;

@NullMarked
public class SupportLibWebChromeClientAdapter {
    private static final String WEB_CHROME_CLIENT_COMPAT_NAME =
            "androidx.webkit.WebChromeClientCompat";
    private static final String[] EMPTY_FEATURE_LIST = new String[0];

    private static final String COMPAT_CLIENT_HISTOGRAM =
            "Android.WebView.SupportLibrary.ChromeClientIsCompat";

    // If {@code null}, this indicates the WebChromeClient is not a
    // WebChromeClientCompat. Otherwise, this is a Proxy for the
    // WebChromeClientCompat.
    @Nullable private WebChromeClientBoundaryInterface mWebChromeClient;
    private String[] mWebChromeClientSupportedFeatures;

    public SupportLibWebChromeClientAdapter() {
        mWebChromeClientSupportedFeatures = EMPTY_FEATURE_LIST;
    }

    public void setWebChromeClient(WebChromeClient possiblyCompatClient) {
        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped("SupportLibWebChromeClientAdapter.setWebChromeClient")) {
            mWebChromeClient = convertCompatClient(possiblyCompatClient);
            mWebChromeClientSupportedFeatures =
                    mWebChromeClient == null
                            ? EMPTY_FEATURE_LIST
                            : mWebChromeClient.getSupportedFeatures();

            // We ignore the case where the client is set to null, since this is often done by
            // WebView's internal logic (such as during destroy()), and would otherwise skew data.
            if (possiblyCompatClient != null) {
                RecordHistogram.recordBooleanHistogram(
                        COMPAT_CLIENT_HISTOGRAM, mWebChromeClient != null);
            }
        }
    }

    @Nullable
    private WebChromeClientBoundaryInterface convertCompatClient(
            WebChromeClient possiblyCompatClient) {
        if (possiblyCompatClient == null) {
            return null;
        }

        if (!BoundaryInterfaceReflectionUtil.instanceOfInOwnClassLoader(
                possiblyCompatClient, WEB_CHROME_CLIENT_COMPAT_NAME)) {
            return null;
        }

        InvocationHandler handler =
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(possiblyCompatClient);

        return BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                WebChromeClientBoundaryInterface.class, handler);
    }

    /**
     * Indicates whether this client can handle the callback(s) associated with {@param
     * featureName}. This should be called with the correct feature name before invoking the
     * corresponding callback, and the callback must not be called if this returns {@code false} for
     * the feature.
     *
     * @param featureName the feature for the desired callback.
     * @return {@code true} if this client can handle the feature.
     */
    public boolean isFeatureAvailable(String featureName) {
        if (mWebChromeClient == null) return false;
        return BoundaryInterfaceReflectionUtil.containsFeature(
                mWebChromeClientSupportedFeatures, featureName);
    }

    public void onReceivedThemeColor(WebView webView, Color color) {
        if (mWebChromeClient == null || !isFeatureAvailable(Features.THEME_COLOR_CALLBACK)) {
            return;
        }

        mWebChromeClient.onReceivedThemeColor(webView, color);
    }
}
