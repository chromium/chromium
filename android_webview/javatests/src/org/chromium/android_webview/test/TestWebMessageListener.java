// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.net.Uri;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.JsReplyProxy;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.base.ThreadUtils;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;

import java.util.concurrent.LinkedBlockingQueue;

public class TestWebMessageListener implements WebMessageListener {
    private LinkedBlockingQueue<Data> mQueue = new LinkedBlockingQueue<>();

    public static class Data {
        private MessagePayload mPayload;
        public Uri mTopLevelOrigin;
        public Uri mSourceOrigin;
        public boolean mIsMainFrame;
        public JsReplyProxy mReplyProxy;
        public MessagePort[] mPorts;

        public Data(
                MessagePayload payload,
                Uri topLevelOrigin,
                Uri sourceOrigin,
                boolean isMainFrame,
                JsReplyProxy replyProxy,
                MessagePort[] ports) {
            mPayload = payload;
            mTopLevelOrigin = topLevelOrigin;
            mSourceOrigin = sourceOrigin;
            mIsMainFrame = isMainFrame;
            mReplyProxy = replyProxy;
            mPorts = ports;
        }

        public String getAsString() {
            return mPayload.getAsString();
        }

        public byte[] getAsArrayBuffer() {
            return mPayload.getAsArrayBuffer();
        }
    }

    public static void addWebMessageListenerOnUiThread(
            final AwContents awContents,
            final String jsObjectName,
            final String[] allowedOriginRules,
            final WebMessageListener listener)
            throws Exception {
        AwActivityTestRule.checkJavaScriptEnabled(awContents);
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.addWebMessageListener(jsObjectName, allowedOriginRules, listener));
    }

    public static void removeWebMessageListenerOnUiThread(
            final AwContents awContents, final String jsObjectName) throws Exception {
        AwActivityTestRule.checkJavaScriptEnabled(awContents);
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.removeWebMessageListener(jsObjectName));
    }

    @Override
    public void onPostMessage(
            MessagePayload payload,
            Uri topLevelOrigin,
            Uri sourceOrigin,
            boolean isMainFrame,
            JsReplyProxy replyProxy,
            MessagePort[] ports) {
        mQueue.add(new Data(payload, topLevelOrigin, sourceOrigin, isMainFrame, replyProxy, ports));
    }

    public Data waitForOnPostMessage() throws Exception {
        return AwActivityTestRule.waitForNextQueueElement(mQueue);
    }

    public boolean hasNoMoreOnPostMessage() {
        return mQueue.isEmpty();
    }
}
