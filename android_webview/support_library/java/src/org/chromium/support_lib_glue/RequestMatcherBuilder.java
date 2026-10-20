// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.support_lib_boundary.RequestMatcherConfig;

import java.util.List;
import java.util.function.BiConsumer;

/**
 * Builder for a Request Matcher via the boundary interface.
 *
 * <p>This class does not actually build the request matcher (this happens in native code, so we
 * avoid exposing native pointers to Java), but collects and exposes the necessary arguments to
 * build it.
 */
@NullMarked
/* package */ class RequestMatcherBuilder
        implements BiConsumer<@RequestMatcherConfig Integer, @Nullable Object> {
    private @Nullable List<String> mUrlPatterns;

    @Override
    // Values sent over the BiConsumer<> are Object to keep the interface generic,
    // but it's okay to throw a ClassCastException if we get an incorrect value.
    @SuppressWarnings("unchecked")
    public void accept(@RequestMatcherConfig Integer key, @Nullable Object value) {
        if (key < 0) {
            throw new UnsupportedOperationException(
                    "The current WebView version doesn't support this config: " + key);
        }
        switch (key) {
            case RequestMatcherConfig.URL_PATTERNS:
                if (value == null || !(value instanceof List<?>)) {
                    throw new IllegalArgumentException(
                            "The value for URL_PATTERNS should be a List<String>");
                }
                mUrlPatterns = ((List<String>) value);
                break;
        }
    }

    public List<String> getUrlPatterns() {
        List<String> result = mUrlPatterns;
        if (result == null) {
            throw new IllegalStateException("No URL Patterns provided");
        }
        return result;
    }
}
