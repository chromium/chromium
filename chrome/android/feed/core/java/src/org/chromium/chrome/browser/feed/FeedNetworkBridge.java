// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpResponse;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.common.functional.Consumer;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Provides access to native implementations of NetworkClient.
 */
@JNINamespace("feed")
public class FeedNetworkBridge implements NetworkClient {
    private long mNativeBridge;

    /**
     * Creates a FeedNetworkBridge for accessing native network implementation for the current
     * user.
     *
     * @param profile Profile of the user we are rendering the Feed for.
     */
    public FeedNetworkBridge(Profile profile) {
        mNativeBridge = FeedNetworkBridgeJni.get().init(FeedNetworkBridge.this, profile);
    }

    /*
     * Cleans up native half of this bridge.
     */
    public void destroy() {
        assert mNativeBridge != 0;
        FeedNetworkBridgeJni.get().destroy(mNativeBridge, FeedNetworkBridge.this);
        mNativeBridge = 0;
    }

    @Override
    public void send(HttpRequest request, Consumer<HttpResponse> responseConsumer) {
        if (mNativeBridge == 0) {
            responseConsumer.accept(createHttpResponse(500, new byte[0]));
        } else {
            FeedNetworkBridgeJni.get().sendNetworkRequest(mNativeBridge, FeedNetworkBridge.this,
                    request.getUri().toString(), request.getMethod(), request.getBody(),
                    result -> responseConsumer.accept(result));
        }
    }

    @Override
    public void close() {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeBridge == 0) return;

        FeedNetworkBridgeJni.get().cancelRequests(mNativeBridge, FeedNetworkBridge.this);
    }

    @CalledByNative
    private static HttpResponse createHttpResponse(int code, byte[] body) {
        return new HttpResponse(code, body);
    }

    @NativeMethods
    interface Natives {
        long init(FeedNetworkBridge caller, Profile profile);
        void destroy(long nativeFeedNetworkBridge, FeedNetworkBridge caller);
        void sendNetworkRequest(long nativeFeedNetworkBridge, FeedNetworkBridge caller, String url,
                String requestType, byte[] body, Callback<HttpResponse> resultCallback);
        void cancelRequests(long nativeFeedNetworkBridge, FeedNetworkBridge caller);
    }
}
