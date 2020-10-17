// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.mocknetworkclient;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.provider.Settings.Global;
import android.util.Base64;

import com.google.protobuf.ByteString;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpResponse;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.feedrequestmanager.RequestHelper;
import org.chromium.components.feed.core.proto.wire.FeedRequestProto.FeedRequest;
import org.chromium.components.feed.core.proto.wire.RequestProto.Request;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.components.feed.core.proto.wire.mockserver.MockServerProto.ConditionalResponse;
import org.chromium.components.feed.core.proto.wire.mockserver.MockServerProto.MockServer;

import java.io.IOException;
import java.util.List;

/** A network client that returns mock responses provided via a config proto */
public final class MockServerNetworkClient implements NetworkClient {
    private static final String TAG = "MockServerNetworkClient";

    private final Context mContext;
    private final Response mInitialResponse;
    private final List<ConditionalResponse> mConditionalResponses;
    private final ExtensionRegistryLite mExtensionRegistry;
    private final long mResponseDelay;

    public MockServerNetworkClient(
            Context context, MockServer mockServer, long responseDelayMillis) {
        this.mContext = context;
        mInitialResponse = mockServer.getInitialResponse();
        mConditionalResponses = mockServer.getConditionalResponsesList();

        mExtensionRegistry = ExtensionRegistryLite.newInstance();
        mExtensionRegistry.add(FeedRequest.feedRequest);
        mResponseDelay = responseDelayMillis;
    }

    @Override
    public void send(HttpRequest httpRequest, Consumer<HttpResponse> responseConsumer) {
        try {
            if (isAirplaneModeOn()) {
                delayedAccept(new HttpResponse(400, new byte[] {}, false), responseConsumer);
                return;
            }
            Request request = getRequest(httpRequest);
            ByteString requestToken =
                    (request.getExtension(FeedRequest.feedRequest).getFeedQuery().hasPageToken())
                    ? request.getExtension(FeedRequest.feedRequest).getFeedQuery().getPageToken()
                    : null;
            if (requestToken != null) {
                for (ConditionalResponse response : mConditionalResponses) {
                    if (!response.hasContinuationToken()) {
                        Logger.w(TAG, "Conditional response without a token");
                        continue;
                    }
                    if (requestToken.equals(response.getContinuationToken())) {
                        delayedAccept(createHttpResponse(response.getResponse()), responseConsumer);
                        return;
                    }
                }
            }
            delayedAccept(createHttpResponse(mInitialResponse), responseConsumer);
        } catch (IOException e) {
            // TODO : handle errors here
            Logger.e(TAG, e.getMessage());
            delayedAccept(new HttpResponse(400, new byte[] {}, false), responseConsumer);
        }
    }

    private boolean isAirplaneModeOn() {
        return Settings.System.getInt(mContext.getContentResolver(), Global.AIRPLANE_MODE_ON, 0)
                != 0;
    }

    private void delayedAccept(HttpResponse httpResponse, Consumer<HttpResponse> responseConsumer) {
        if (mResponseDelay <= 0) {
            responseConsumer.accept(httpResponse);
            return;
        }

        new Handler(Looper.getMainLooper())
                .postDelayed(() -> responseConsumer.accept(httpResponse), mResponseDelay);
    }

    // TODO Fix nullness violation: incompatible types in argument.
    @SuppressWarnings("nullness:argument.type.incompatible")
    private Request getRequest(HttpRequest httpRequest) throws IOException {
        byte[] rawRequest = new byte[0];
        if (httpRequest.getMethod().equals(HttpMethod.GET)) {
            if (httpRequest.getUri().getQueryParameter(RequestHelper.MOTHERSHIP_PARAM_PAYLOAD)
                    != null) {
                rawRequest = Base64.decode(httpRequest.getUri().getQueryParameter(
                                                   RequestHelper.MOTHERSHIP_PARAM_PAYLOAD),
                        Base64.URL_SAFE);
            }
        } else {
            rawRequest = httpRequest.getBody();
        }
        return Request.parseFrom(rawRequest, mExtensionRegistry);
    }

    @Override
    public void close() {}

    private HttpResponse createHttpResponse(Response response) {
        if (response == null) {
            return new HttpResponse(500, new byte[] {}, false);
        }

        try {
            byte[] rawResponse = response.toByteArray();
            byte[] newResponse = new byte[rawResponse.length + (Integer.SIZE / 8)];
            CodedOutputStream codedOutputStream = CodedOutputStream.newInstance(newResponse);
            codedOutputStream.writeUInt32NoTag(rawResponse.length);
            codedOutputStream.writeRawBytes(rawResponse);
            codedOutputStream.flush();
            return new HttpResponse(200, newResponse, false);
        } catch (IOException e) {
            Logger.e(TAG, "Error creating response", e);
            return new HttpResponse(500, new byte[] {}, false);
        }
    }
}
