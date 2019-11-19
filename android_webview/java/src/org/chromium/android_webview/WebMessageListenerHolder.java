// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.mojo.system.impl.CoreImpl;

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
    public void onPostMessage(String message, String sourceOrigin, boolean isMainFrame, int[] ports,
            JsReplyProxy replyProxy) {
        MessagePort[] messagePorts = new MessagePort[ports.length];
        for (int i = 0; i < ports.length; ++i) {
            messagePorts[i] = convertRawHandleToMessagePort(ports[i]);
        }
        mListener.onPostMessage(
                message, Uri.parse(sourceOrigin), isMainFrame, replyProxy, messagePorts);
    }

    private static MessagePort convertRawHandleToMessagePort(int rawHandle) {
        return MessagePort.create(
                CoreImpl.getInstance().acquireNativeHandle(rawHandle).toMessagePipeHandle());
    }
}
