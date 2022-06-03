// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.MessagePort;

/**
 * Holds the {@link WebMessageListener} instance so that C++ could interact with the {@link
 * WebMessageListener}.
 */
@JNINamespace("android_webview")
public class WebMessageListenerHolder {
    private WebMessageListener mListener;

    public WebMessageListenerHolder(@NonNull WebMessageListener listener) {
        mListener = listener;
    }

    @CalledByNative
    public void onPostMessage(String message, String sourceOrigin, boolean isMainFrame,
            MessagePort[] ports, JsReplyProxy replyProxy) {
        mListener.onPostMessage(message, Uri.parse(sourceOrigin), isMainFrame, replyProxy, ports);
    }

    public WebMessageListener getListener() {
        return mListener;
    }
}
