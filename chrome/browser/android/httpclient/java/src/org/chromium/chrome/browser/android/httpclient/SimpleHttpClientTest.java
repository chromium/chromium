// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android.httpclient;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.android.httpclient.SimpleHttpClient.HttpResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetworkTrafficAnnotationTag;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.concurrent.ExecutionException;
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
    public void testSendRequest() throws TimeoutException {
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
}
