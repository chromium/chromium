// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;



/**
 * Parameters to {@link AwContents#navigate}.
 *
 * <p>Note: This object does no validation - the url may be null, so check that before using.
 */
public class AwNavigationParams {
    public final String url;
    public final boolean shouldReplaceCurrentEntry;

    /** Constructs this object specifying all parameters. */
    public AwNavigationParams(String url, boolean shouldReplaceCurrentEntry) {
        // Note: If this gets more complex, consider a builder.
        this.url = url;
        this.shouldReplaceCurrentEntry = shouldReplaceCurrentEntry;
    }

    /** Constructs the object with default values. */
    public AwNavigationParams(String url) {
        this(url, /* shouldReplaceCurrentEntry= */ false);
    }
}
