// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android.httpclient;

import android.content.Context;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.android.httpclient.SimpleHttpClient.HttpResponse;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.NetworkTrafficAnnotationTag;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Integration test for {@link SimpleHttpClient}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SimpleHttpClientTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";

    @Rule public ChromeBrowserTestRule mTestRule = new ChromeBrowserTestRule();

    private Context mContext;
    private EmbeddedTestServer mTestServer;
    private SimpleHttpClient mHttpClient;

    private Callback<HttpResponse> mCallback;

    public HttpResponse mLastAcceptedResponse;

    private final CallbackHelper mCallbackHelper = new CallbackHelper();

    @Before
    public void setUp() throws ExecutionException {
        mContext = ContextUtils.getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(mContext);
        mLastAcceptedResponse = null;
        mCallback =
                response -> {
                    mLastAcceptedResponse = response;
                    mCallbackHelper.notifyCalled();
                };

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mHttpClient =
                                new SimpleHttpClient(ProfileManager.getLastUsedRegularProfile()));
    }

    @Test
    @SmallTest
    public void testSendRequest_OnUiThread() throws TimeoutException {
        String url = mTestServer.getURL(TEST_PAGE);
        GURL gurl = new GURL(url);
        String body = "";

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mHttpClient.send(
                                gurl,
                                "POST",
                                body.getBytes(),
                                new HashMap<>(),
                                NetworkTrafficAnnotationTag.TRAFFIC_ANNOTATION_FOR_TESTS,
                                mCallback));

        mCallbackHelper.waitForOnly();
        Assert.assertNotNull(mLastAcceptedResponse);
    }

    @Test
    @SmallTest
    public void testSendRequest_OnBackgroundThread() throws TimeoutException {
        Assert.assertFalse(ThreadUtils.runningOnUiThread());
        String url = mTestServer.getURL(TEST_PAGE);
        GURL gurl = new GURL(url);
        String body = "";

        mHttpClient.send(
                gurl,
                "POST",
                body.getBytes(),
                new HashMap<>(),
                NetworkTrafficAnnotationTag.TRAFFIC_ANNOTATION_FOR_TESTS,
                mCallback);

        mCallbackHelper.waitForOnly();
        Assert.assertNotNull(mLastAcceptedResponse);
    }

    @Test
    @LargeTest
    public void testSendRequest_DestroyedClient() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        CallbackHelper receivedRequestCallback = new CallbackHelper();
        CallbackHelper serverRespondedCallbackHelper = new CallbackHelper();
        try {
            String url =
                    webServer.setResponseWithRunnableAction(
                            TEST_PAGE,
                            "Content Body Here",
                            null,
                            () -> {
                                receivedRequestCallback.notifyCalled();
                                // Simulate a slow download so we can destroy the client before
                                // the response arrives.
                                try {
                                    Thread.sleep(500);
                                } catch (InterruptedException e) {

                                } finally {
                                    serverRespondedCallbackHelper.notifyCalled();
                                }
                            });
            GURL gurl = new GURL(url);
            String body = "";

            mHttpClient.send(
                    gurl,
                    "POST",
                    body.getBytes(),
                    new HashMap<>(),
                    NetworkTrafficAnnotationTag.TRAFFIC_ANNOTATION_FOR_TESTS,
                    mCallback);

            receivedRequestCallback.waitForOnly();
            ThreadUtils.runOnUiThreadBlocking(() -> mHttpClient.destroy());

            serverRespondedCallbackHelper.waitForOnly();
            Assert.assertThrows(
                    TimeoutException.class, () -> mCallbackHelper.waitForOnly(1, TimeUnit.SECONDS));
            Assert.assertNull(mLastAcceptedResponse);
        } finally {
            webServer.shutdown();
        }
    }
}
