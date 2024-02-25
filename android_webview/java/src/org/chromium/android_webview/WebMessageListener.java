// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;

import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;

/**
 * WebMessageListener interface, which is used to listen {@link AwContents#onPostMessage}
 * callback in app. See also {@link AwContents#setWebMessageListener}.
 *
 */
public interface WebMessageListener {
    /**
     * Receives postMessage information.
     *
     * @param payload The message payload from JavaScript.
     * @param topLevelOrigin The origin of the top level frame where the message is from.
     * @param sourceOrigin The origin of the frame where the message is from.
     * @param isMainFrame If the message is from a main frame.
     * @param jsReplyProxy Used for reply message to the injected JavaScript object.
     * @param ports JavaScript code could post message with additional message ports. Receive ports
     *     to establish new communication channels. Could be empty array but won't be null.
     */
    void onPostMessage(
            MessagePayload payload,
            Uri topLevelOrigin,
            Uri sourceOrigin,
            boolean isMainFrame,
            JsReplyProxy jsReplyProxy,
            MessagePort[] ports);
}
