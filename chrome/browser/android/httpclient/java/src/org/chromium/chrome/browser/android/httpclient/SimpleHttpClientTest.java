// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android.httpclient;

import android.content.Context;
import android.util.Pair;

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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.android.httpclient.SimpleHttpClient.HttpResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetworkTrafficAnnotationTag;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.url.GURL;

import java.util.ArrayList;
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

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHttpClient = new SimpleHttpClient(Profile.getLastUsedRegularProfile()));
    }

    @Test
    @SmallTest
    public void testSendRequest_OnUiThread() throws TimeoutException {
        String url = mTestServer.getURL(TEST_PAGE);
        GURL gurl = new GURL(url);
        String body = "";

        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        mHttpClient.send(
                                gurl,
                                "POST",
                                body.getBytes(),
                                new HashMap<>(),
                                NetworkTrafficAnnotationTag.TRAFFIC_ANNOTATION_FOR_TESTS,
                                mCallback));

        mCallbackHelper.waitForFirst();
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

        mCallbackHelper.waitForFirst();
        Assert.assertNotNull(mLastAcceptedResponse);
    }

    // Disable temporarily to land actual fix. See https://crbug.com/1517165.
    @DisabledTest
    @Test
    @LargeTest
    public void testSendRequest_DestroyedClient() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        CallbackHelper serverRespondedCallbackHelper = new CallbackHelper();
        try {
            String url =
                    webServer.setResponseWithRunnableAction(
                            TEST_PAGE,
                            "",
                            new ArrayList<Pair<String, String>>(),
                            () -> {
                                // Simulate a slow download so we can destroy the client before
                                // the response arrives.
                                try {
                                    Thread.sleep(2000);
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
            TestThreadUtils.runOnUiThreadBlocking(() -> mHttpClient.destroy());

            serverRespondedCallbackHelper.waitForFirst();
            Assert.assertThrows(
                    TimeoutException.class,
                    () -> mCallbackHelper.waitForFirst(3, TimeUnit.SECONDS));
            Assert.assertNull(mLastAcceptedResponse);
        } finally {
            webServer.shutdown();
        }
    }
}
