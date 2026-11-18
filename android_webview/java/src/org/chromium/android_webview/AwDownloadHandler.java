// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;

/**
 * Receives downloads started in an AwContents that has opted into WebView-managed downloads via
 * {@link AwContents#setDownloadHandler}.
 */
@Lifetime.WebView
@NullMarked
public interface AwDownloadHandler {
    /**
     * Called on the UI thread for each new download. The handler must eventually call either {@link
     * AwWebDownload#saveTo} or {@link AwWebDownload#cancel()}, and may do so after this method
     * returns.
     */
    void onDownloadRequested(AwWebDownload download);
}
