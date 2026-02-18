// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.Nullable;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Parameters to {@link AwContents#navigate}.
 *
 * <p>Note: This object does no validation - the url may be null, so check that before using.
 */
public class AwNavigationParams {
    public final String url;
    public final boolean shouldReplaceCurrentEntry;
    public final Map<String, String> extraHeaders;

    /** Constructs this object specifying all parameters. */
    public AwNavigationParams(
            String url,
            boolean shouldReplaceCurrentEntry,
            @Nullable Map<String, String> extraHeaders) {
        // Note: If this gets more complex, consider a builder.
        this.url = url;
        this.shouldReplaceCurrentEntry = shouldReplaceCurrentEntry;
        if (extraHeaders == null) {
            this.extraHeaders = Collections.emptyMap();
        } else {
            this.extraHeaders = new HashMap<>(extraHeaders);
        }
    }

    /** Constructs the object with default values. */
    public AwNavigationParams(String url) {
        this(url, /* shouldReplaceCurrentEntry= */ false, /* extraHeaders= */ null);
    }
}
