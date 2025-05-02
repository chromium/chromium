// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.content.Context;
import android.webkit.WebView;

import com.android.webview.chromium.SharedWebViewChromium;
import com.android.webview.chromium.WebkitToSharedGlueConverter;

import org.chromium.support_lib_boundary.WebViewBuilderBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewBuilderBoundaryInterface.ConfigField;

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
        private Integer mBaseline;

        @Override
        public void accept(@ConfigField Integer key, Object value) {
            try {
                switch (key) {
                    case ConfigField.BASELINE:
                        {
                            mBaseline = (Integer) value;
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

            if (mBaseline != null) {
                sharedWebViewChromium.configureBaseline(mBaseline);
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
