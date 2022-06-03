// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Consumer;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.survey.SurveyHttpClientBridge.HttpResponse;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Integration test for {@link SurveyHttpClientBridge}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SurveyHttpClientBridgeTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";

    @Rule
    public ChromeBrowserTestRule mTestRule = new ChromeBrowserTestRule();

    private Context mContext;
    private EmbeddedTestServer mTestServer;
    private SurveyHttpClientBridge mHttpClient;

    private Consumer<HttpResponse> mConsumer;

    public HttpResponse mLastAcceptedResponse;

    private final CallbackHelper mCallbackHelper = new CallbackHelper();

    @Before
    public void setUp() throws ExecutionException {
        mContext = ContextUtils.getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(mContext);
        mConsumer = response -> {
            mLastAcceptedResponse = response;
            mCallbackHelper.notifyCalled();
        };

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mHttpClient = new SurveyHttpClientBridge(
                                   HttpClientType.SURVEY, Profile.getLastUsedRegularProfile()));
    }

    @Test
    @SmallTest
    public void testSendRequest() throws TimeoutException {
        String url = mTestServer.getURL(TEST_PAGE);
        GURL gurl = new GURL(url);
        String body = "";

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHttpClient.send(gurl, "POST", body.getBytes(), new HashMap<>(), mConsumer));

        mCallbackHelper.waitForFirst();
        Assert.assertNotNull(mLastAcceptedResponse);
    }
}
