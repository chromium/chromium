// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwWebContent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.support_lib_boundary.web.WebContentConfig;

import java.util.function.BiConsumer;

/** Builder for AwWebContent. */
@NullMarked
/* package */ class WebContentBuilder
        implements BiConsumer<@WebContentConfig Integer, @Nullable Object> {
    @Override
    public void accept(@WebContentConfig Integer key, @Nullable Object value) {
        if (key < 0) {
            throw new UnsupportedOperationException(
                    "The current WebView version doesn't support this config: " + key);
        }
        // If we get here then it means that there's an optional operation that the
        // current WebView version doesn't support and it's safe to ignore.
    }

    public AwWebContent build() {
        return new AwWebContent();
    }
}
