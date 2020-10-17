// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import android.util.Base64;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;

import org.chromium.base.Consumer;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpResponse;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.feedrequestmanager.RequestHelper;
import org.chromium.components.feed.core.proto.wire.FeedRequestProto.FeedRequest;
import org.chromium.components.feed.core.proto.wire.RequestProto.Request;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.components.feed.core.proto.wire.mockserver.MockServerProto.ConditionalResponse;
import org.chromium.components.feed.core.proto.wire.mockserver.MockServerProto.MockServer;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A network client that returns configurable responses
 *  modified from org.chromium.chrome.browser.feed.library.mocknetworkclient.MockServerNetworkClient
 */
public class TestNetworkClient implements NetworkClient {
    private static final String TAG = "TestNetworkClient";

    private final ExtensionRegistryLite mExtensionRegistry;
    private final long mResponseDelay;
    private final AtomicBoolean mAlreadyClosed = new AtomicBoolean(false);

    private MockServer mMockServer;

    public TestNetworkClient() {
        Configuration config = new Configuration.Builder().build();
        mExtensionRegistry = ExtensionRegistryLite.newInstance();
        mExtensionRegistry.add(FeedRequest.feedRequest);
        // TODO(aluo): Add ability to delay responses.
        mResponseDelay = 0L;
        mMockServer = MockServer.getDefaultInstance();
    }

    /**
     * Set stored protobuf responses from the filePath
     *
     * @param filePath The file path of the compiled MockServer proto, pass in null to use the
     *                 default response.
     */
    @VisibleForTesting
    public void setNetworkResponseFile(String filePath) throws IOException {
        if (filePath == null) {
            setResponseData(null);
        } else {
            FileInputStream fs = new FileInputStream(filePath);
            setResponseData(fs);
            fs.close();
        }
    }

    /**
     * Set stored protobuf responses from the InputStream
     *
     * @param fs A {@link InputStream} with response pb data.
     *           Pass in null to clear.
     */
    public void setResponseData(InputStream fs) throws IOException {
        if (fs == null) {
            mMockServer = MockServer.getDefaultInstance();
        } else {
            mMockServer = MockServer.parseFrom(fs);
        }
    }

    @Override
    public void send(HttpRequest httpRequest, Consumer<HttpResponse> responseConsumer) {
        // TODO(aluo): Add ability to respond with HTTP Errors.
        try {
            Request request = getRequest(httpRequest);
            ByteString requestToken =
                    (request.getExtension(FeedRequest.feedRequest).getFeedQuery().hasPageToken())
                    ? request.getExtension(FeedRequest.feedRequest).getFeedQuery().getPageToken()
                    : null;
            if (requestToken != null) {
                for (ConditionalResponse response : mMockServer.getConditionalResponsesList()) {
                    if (!response.hasContinuationToken()) {
                        continue;
                    }
                    if (requestToken.equals(response.getContinuationToken())) {
                        delayedAccept(createHttpResponse(response.getResponse()), responseConsumer);
                        return;
                    }
                }
                delayedAccept(createHttpResponse(Response.getDefaultInstance()), responseConsumer);
            } else {
                delayedAccept(
                        createHttpResponse(mMockServer.getInitialResponse()), responseConsumer);
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private void delayedAccept(HttpResponse httpResponse, Consumer<HttpResponse> responseConsumer) {
        if (mResponseDelay <= 0) {
            maybeAccept(httpResponse, responseConsumer);
        } else {
            PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT,
                    () -> maybeAccept(httpResponse, responseConsumer), mResponseDelay);
        }
    }

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
    public void close() {
        mAlreadyClosed.set(true);
    }

    private HttpResponse createHttpResponse(Response response) {
        try {
            byte[] rawResponse = response.toByteArray();
            byte[] newResponse = new byte[rawResponse.length + (Integer.SIZE / 8)];
            CodedOutputStream codedOutputStream = CodedOutputStream.newInstance(newResponse);
            codedOutputStream.writeUInt32NoTag(rawResponse.length);
            codedOutputStream.writeRawBytes(rawResponse);
            codedOutputStream.flush();
            return new HttpResponse(200, newResponse, /* isSignedIn= */ false);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private void maybeAccept(HttpResponse httpResponse, Consumer<HttpResponse> responseConsumer) {
        if (!mAlreadyClosed.get()) {
            responseConsumer.accept(httpResponse);
        }
    }
}
