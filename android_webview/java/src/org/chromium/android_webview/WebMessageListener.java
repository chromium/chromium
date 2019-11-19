// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;

import org.chromium.content_public.browser.MessagePort;

/**
 * WebMessageListener interface, which is used to listen {@link AwContents#onPostMessage}
 * callback in app. See also {@link AwContents#setWebMessageListener}.
 *
 */
public interface WebMessageListener {
    /**
     * Receives postMessage information.
     * @param message      The message from JavaScript.
     * @param sourceOrigin The origin of the frame where the message is from.
     * @param isMainframe  If the message is from a main frame.
     * @param jsReplyProxy Used for reply message to the injected JavaScript object.
     * @param ports        JavaScript code could post message with additional message ports. Receive
     *                     ports to establish new communication channels. Could be empty array but
     *                     won't be null.
     */
    void onPostMessage(String message, Uri sourceOrigin, boolean isMainFrame,
            JsReplyProxy jsReplyProxy, MessagePort[] ports);
}
