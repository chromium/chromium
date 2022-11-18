// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.v2;

import com.google.protobuf.CodedOutputStream;

import org.chromium.base.Log;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.WebServer;

import java.io.IOException;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.util.Arrays;

/**
 * A fake server which supplies Feedv2 content.
 */
public class TestFeedServer implements WebServer.RequestHandler {
    private static final String TAG = "TestFeedServer";
    private WebServer mServer;
    private boolean mReceivedQueryRequest;

    public TestFeedServer() {
        try {
            mServer = new WebServer(0, false);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                UserPrefs.get(Profile.getLastUsedRegularProfile())
                        .setString(Pref.HOST_OVERRIDE_HOST, getBaseUrl());
            });
            mServer.setRequestHandler(this);
        } catch (Exception e) {
            Log.e(TAG, "Unexpected exception", e);
        }
    }

    public void shutdown() {
        mServer.shutdown();
    }

    public String getBaseUrl() {
        return mServer.getBaseUrl();
    }

    public boolean receivedQueryRequest() {
        return mReceivedQueryRequest;
    }

    @Override
    public void handleRequest(WebServer.HTTPRequest request, OutputStream output) {
        try {
            tryHandleRequest(request, output);
        } catch (IOException e) {
            Log.e(TAG, "Exception while handling request " + request, e);
        }
    }

    private void tryHandleRequest(WebServer.HTTPRequest request, OutputStream output)
            throws IOException {
        if (request.getMethod().equals("GET") && request.getURI().contains("/FeedQuery?")) {
            mReceivedQueryRequest = true;

            WebServer.writeResponse(output, WebServer.STATUS_OK,
                    feedQueryResponse("/chrome/test/data/android/feed/v2/"
                            + "feed_query_normal_response.binarypb"));
            return;
        }
        // Note: Support could be added for NextPageQuery and actions:upload.
        Log.e(TAG, "Unhandled request: " + request);
    }

    private byte[] readFile(String filePath) throws IOException {
        RandomAccessFile file = new RandomAccessFile(filePath, "r");
        byte[] bytes = new byte[(int) file.length()];
        int bytesRead = file.read(bytes);
        if (bytesRead != bytes.length) {
            return Arrays.copyOfRange(bytes, 0, bytesRead);
        }
        return bytes;
    }

    private byte[] feedQueryResponse(String bodyFilePath) throws IOException {
        // Protos returned by the server have a varint header that encodes the size.
        byte[] encodedProtoResponse = readFile(UrlUtils.getIsolatedTestFilePath(bodyFilePath));
        byte[] fullResponse = new byte[encodedProtoResponse.length + 5];

        CodedOutputStream codedOutputStream = CodedOutputStream.newInstance(fullResponse);
        codedOutputStream.writeUInt32NoTag(encodedProtoResponse.length);
        codedOutputStream.writeRawBytes(encodedProtoResponse);
        codedOutputStream.flush();
        return Arrays.copyOfRange(fullResponse, 0, codedOutputStream.getTotalBytesWritten());
    }
}
