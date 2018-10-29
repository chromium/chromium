// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.util.Base64;

import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.feedrequestmanager.FeedRequestManager;
import com.google.android.libraries.feed.host.config.Configuration;
import com.google.android.libraries.feed.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.host.network.HttpRequest;
import com.google.android.libraries.feed.host.network.HttpRequest.HttpMethod;
import com.google.android.libraries.feed.host.network.HttpResponse;
import com.google.android.libraries.feed.host.network.NetworkClient;
import com.google.protobuf.ByteString;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.search.now.wire.feed.FeedRequestProto.FeedRequest;
import com.google.search.now.wire.feed.RequestProto.Request;
import com.google.search.now.wire.feed.ResponseProto.Response;
import com.google.search.now.wire.feed.mockserver.MockServerProto.ConditionalResponse;
import com.google.search.now.wire.feed.mockserver.MockServerProto.MockServer;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

/** A network client that returns configurable responses
 *  modified from com.google.android.libraries.feed.mocknetworkclient.MockServerNetworkClient
 */
public class TestNetworkClient implements NetworkClient {
    private static final String TAG = "TestNetworkClient";

    private final ExtensionRegistryLite mExtensionRegistry;
    private final long mResponseDelay;

    private MockServer mMockServer;

    public TestNetworkClient() {
        Configuration config = new Configuration.Builder().build();
        mExtensionRegistry = ExtensionRegistryLite.newInstance();
        mExtensionRegistry.add(FeedRequest.feedRequest);
        // TODO(aluo): Add ability to delay responses
        mResponseDelay = config.getValueOrDefault(ConfigKey.MOCK_SERVER_DELAY_MS, 0L);
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

    /** Set stored protobuf responses from the InputStream
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
        // TODO(aluo): Add ability to respond with HTTP Errors
        try {
            Request request = getRequest(httpRequest);
            ByteString requestToken =
                    (request.getExtension(FeedRequest.feedRequest).getFeedQuery().hasPageToken())
                    ? request.getExtension(FeedRequest.feedRequest).getFeedQuery().getPageToken()
                    : null;
            if (requestToken != null) {
                for (ConditionalResponse response : mMockServer.getConditionalResponsesList()) {
                    if (!response.hasContinuationToken()) {
                        Logger.w(TAG, "Conditional response without a token");
                        continue;
                    }
                    if (requestToken.equals(response.getContinuationToken())) {
                        delayedAccept(createHttpResponse(response.getResponse()), responseConsumer);
                        return;
                    }
                }
                delayedAccept(createHttpResponse(Response.getDefaultInstance()),
                              responseConsumer);
            } else {
                delayedAccept(createHttpResponse(mMockServer.getInitialResponse()),
                              responseConsumer);
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private void delayedAccept(HttpResponse httpResponse, Consumer<HttpResponse> responseConsumer) {
        if (mResponseDelay <= 0) {
            responseConsumer.accept(httpResponse);
            return;
        }

        ThreadUtils.postOnUiThreadDelayed(
                () -> responseConsumer.accept(httpResponse), mResponseDelay);
    }

    private Request getRequest(HttpRequest httpRequest) throws IOException {
        byte[] rawRequest = new byte[0];
        if (httpRequest.getMethod().equals(HttpMethod.GET)) {
            if (httpRequest.getUri().getQueryParameter(FeedRequestManager.MOTHERSHIP_PARAM_PAYLOAD)
                    != null) {
                rawRequest = Base64.decode(httpRequest.getUri().getQueryParameter(
                                                   FeedRequestManager.MOTHERSHIP_PARAM_PAYLOAD),
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
        try {
            byte[] rawResponse = response.toByteArray();
            byte[] newResponse = new byte[rawResponse.length + (Integer.SIZE / 8)];
            CodedOutputStream codedOutputStream = CodedOutputStream.newInstance(newResponse);
            codedOutputStream.writeUInt32NoTag(rawResponse.length);
            codedOutputStream.writeRawBytes(rawResponse);
            codedOutputStream.flush();
            return new HttpResponse(200, newResponse);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
