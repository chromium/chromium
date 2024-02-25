// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.xsurface.ImageFetchClient;

/** Implementation of xsurface's ImageFetchClient. Calls through to the native network stack. */
@JNINamespace("feed")
public class FeedImageFetchClient implements ImageFetchClient {
    private static class HttpResponseImpl implements ImageFetchClient.HttpResponse {
        private int mStatus;
        private byte[] mBody;

        public HttpResponseImpl(int status, byte[] body) {
            mStatus = status;
            mBody = body;
        }

        @Override
        public int status() {
            return mStatus;
        }

        @Override
        public byte[] body() {
            return mBody;
        }
    }

    @Override
    public int sendCancelableRequest(
            String url, ImageFetchClient.HttpResponseConsumer responseConsumer) {
        assert ThreadUtils.runningOnUiThread();
        return FeedImageFetchClientJni.get().sendRequest(url, responseConsumer);
    }

    @Override
    public void sendRequest(String url, ImageFetchClient.HttpResponseConsumer responseConsumer) {
        sendCancelableRequest(url, responseConsumer);
    }

    @Override
    public void cancel(int requestId) {
        assert ThreadUtils.runningOnUiThread();
        FeedImageFetchClientJni.get().cancel(requestId);
    }

    @CalledByNative
    public static void onHttpResponse(
            ImageFetchClient.HttpResponseConsumer responseConsumer, int status, byte[] body) {
        responseConsumer.requestComplete(new HttpResponseImpl(status, body));
    }

    @NativeMethods
    interface Natives {
        int sendRequest(String url, ImageFetchClient.HttpResponseConsumer responseConsumer);

        void cancel(int requestId);
    }
}
