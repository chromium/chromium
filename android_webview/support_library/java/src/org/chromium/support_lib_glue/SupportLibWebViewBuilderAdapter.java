// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.content.Context;
import android.webkit.WebView;

import androidx.annotation.Nullable;

import com.android.webview.chromium.SharedWebViewChromium;
import com.android.webview.chromium.WebkitToSharedGlueConverter;

import org.chromium.android_webview.AwBrowserContextStore;
import org.chromium.android_webview.AwContents;
import org.chromium.support_lib_boundary.WebViewBuilderBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewBuilderBoundaryInterface.ConfigField;

import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.Consumer;

class SupportLibWebViewBuilderAdapter implements WebViewBuilderBoundaryInterface {
    @Override
    public WebView build(
            Context context, Consumer<BiConsumer<@ConfigField Integer, Object>> buildConfig) {
        if (context == null) {
            // TODO(crbug.com/414645298): Confirm if we can validate for an activity here.
            throw new IllegalArgumentException(
                    "A context must be provided to WebViewBuilder#build");
        }

        Builder builder = new Builder();
        buildConfig.accept(builder);
        // Build is an explicit separate API call from accepting the config because there
        // are some configurations we need to handle the order of operations for like the profile
        // so it helps to know these values up front.
        return builder.build(context);
    }

    static class Builder implements BiConsumer<@ConfigField Integer, Object> {
        private boolean mRestrictJavascriptInterface;
        private @Nullable List<Object> mJavascriptInterfaceObjects;
        private @Nullable List<String> mJavascriptInterfaceNames;
        private @Nullable List<List<String>> mJavascriptInterfaceOriginPatterns;
        private @Nullable String mProfileName;

        @Override
        public void accept(@ConfigField Integer key, Object value) {
            try {
                switch (key) {
                    case ConfigField.BASELINE:
                        {
                            // TODO(crbug.com/419726203): Add baselines.
                            break;
                        }
                    case ConfigField.RESTRICT_JAVASCRIPT_INTERFACE:
                        {
                            mRestrictJavascriptInterface = (Boolean) value;
                            break;
                        }
                    case ConfigField.JAVASCRIPT_INTERFACE:
                        {
                            Object[] interfaceParams = (Object[]) value;
                            mJavascriptInterfaceObjects = (List<Object>) interfaceParams[0];
                            mJavascriptInterfaceNames = (List<String>) interfaceParams[1];
                            mJavascriptInterfaceOriginPatterns =
                                    (List<List<String>>) interfaceParams[2];
                            break;
                        }
                    case ConfigField.PROFILE_NAME:
                        {
                            mProfileName = (String) value;
                            break;
                        }
                }
            } catch (ClassCastException e) {
                throw new InternalApiMismatchException(key + " was not configured correctly", e);
            }
        }

        WebView build(Context context) {
            // Constructing the WebView internally will verify if this is on
            // the UIThread or not.
            WebView webview = new WebView(context);

            SharedWebViewChromium sharedWebViewChromium =
                    WebkitToSharedGlueConverter.getSharedWebViewChromium(webview);
            AwContents awContents = sharedWebViewChromium.getAwContents();

            if (mProfileName != null) {
                awContents.setBrowserContextForPublicApi(
                        AwBrowserContextStore.getNamedContext(mProfileName, true));
            }

            boolean allowlistJavascriptInterfaces =
                    mJavascriptInterfaceObjects != null
                            && mJavascriptInterfaceNames != null
                            && mJavascriptInterfaceOriginPatterns != null
                            && mJavascriptInterfaceObjects.size() > 0;

            if (mRestrictJavascriptInterface) {
                awContents.restrictJavascriptInterface();
            } else if (allowlistJavascriptInterfaces) {
                throw new IllegalArgumentException(
                        "JavascriptInterface cannot be allowlisted without first restricting"
                                + " javascript interface via restrictJavascriptInterface()");
            }

            if (allowlistJavascriptInterfaces) {
                sharedWebViewChromium.addJavascriptInterfaces(
                        mJavascriptInterfaceObjects,
                        mJavascriptInterfaceNames,
                        mJavascriptInterfaceOriginPatterns);
            }

            return webview;
        }
    }

    // This exception means something went wrong between AndroidX and Chromium when converting
    // the internal types.
    static class InternalApiMismatchException extends RuntimeException {
        InternalApiMismatchException(String message, Throwable e) {
            super(message, e);
        }
    }
}
