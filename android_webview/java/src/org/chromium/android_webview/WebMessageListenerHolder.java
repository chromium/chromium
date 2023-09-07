// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;

import androidx.annotation.NonNull;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;

/**
 * Holds the {@link WebMessageListener} instance so that C++ could interact with the {@link
 * WebMessageListener}.
 */
@Lifetime.Temporary
@JNINamespace("android_webview")
public class WebMessageListenerHolder {
    private final WebMessageListener mListener;

    public WebMessageListenerHolder(@NonNull WebMessageListener listener) {
        mListener = listener;
    }

    @CalledByNative
    public void onPostMessage(MessagePayload payload, String sourceOrigin, boolean isMainFrame,
            MessagePort[] ports, JsReplyProxy replyProxy) {
        AwThreadUtils.postToCurrentLooper(() -> {
            mListener.onPostMessage(
                    payload, Uri.parse(sourceOrigin), isMainFrame, replyProxy, ports);
        });
    }

    public WebMessageListener getListener() {
        return mListener;
    }
}
