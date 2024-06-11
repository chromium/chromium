// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.webkit.JavascriptInterface;

import com.google.common.util.concurrent.SettableFuture;

// Utility class to verify the loading behavior when loading back
// and forward. If the BFCache is enabled, the PageFinished callback
// will no longer get called. The class can be injected to javascript
// to notify a future object when the page load finishes.
// Typically this class shall be used with the test page
// verify_bfcache.html.
public class TestPageLoadedNotifier {
    @JavascriptInterface
    public void done() {
        if (mPageFullyLoadedFuture != null) {
            mPageFullyLoadedFuture.set(true);
        }
    }

    public void setFuture(SettableFuture<Boolean> future) {
        mPageFullyLoadedFuture = future;
    }

    private SettableFuture<Boolean> mPageFullyLoadedFuture;
}
