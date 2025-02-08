// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

/**
 * AsyncShouldInterceptRequestCallback interface, which is used to provide a callback to provide to
 * the embedding app. The app can use this callback to provide request interception information to
 * WebView. Each request must be responded to exactly once; calling the same callback object
 * multiple times is not permitted and the implementation should always eventually call the
 * callback.
 */
public interface AsyncShouldInterceptRequestCallback {
    /**
     * Sends WebResponseCallback to embedding app to provide its custom web response to a
     * WebResourceRequest. Called on a background thread.
     *
     * <p>WebResponseCallback can be called from any thread, and can be called either during
     * shouldInterceptRequestAsync or later.
     *
     * <p>This can be called multiple times, even if previous requests are still pending. In the
     * event that some requests are pending, meaning their WebResponseCallback has not been invoked
     * to complete the request, WebView will continue to allow calls to shouldInterceptRequestAsync
     * for other requests.
     *
     * @param request The WebResource request for interception
     * @param callback The callback provided to the app, which will provide the interception
     *     response information.
     */
    public void shouldInterceptRequestAsync(
            AwContentsClient.AwWebResourceRequest request, WebResponseCallback callback);
}
