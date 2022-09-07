// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.net.Uri;
import android.webkit.WebView;

import java.lang.reflect.InvocationHandler;

/**
 * Boundary interface for org.chromium.android_webview.WebMessageListener.
 */
public interface WebMessageListenerBoundaryInterface extends FeatureFlagHolderBoundaryInterface {
    void onPostMessage(WebView view, /* WebMessage */ InvocationHandler message, Uri sourceOrigin,
            boolean isMainFrame, /* JsReplyProxy */ InvocationHandler replyProxy);
}
