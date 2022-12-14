// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.annotation.SuppressLint;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.net.test.util.WebServer.HTTPHeader;
import org.chromium.net.test.util.WebServer.HTTPRequest;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Integration test for the X-Requested-With origin trial in WebView.
 *
 * This test is temporary, and should be deleted once the WebViewXRequestedWithDeprecation trial
 * ends.
 *
 * Tests in this class start a server on specific ports since the port number is part of the origin,
 * which is encoded in the static trial tokens. To reduce the likelihood of collisions when running
 * the full test suite, the tests use different ports, but it is still possible that the server may
 * not be able to start if the same test is run multiple times in quick succession.
 */
@Batch(Batch.PER_CLASS)
@RunWith(AwJUnit4ClassRunner.class)
@CommandLineFlags.Add({"origin-trial-public-key=dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=",
        "enable-features=PersistentOriginTrials,WebViewXRequestedWithHeaderControl"})
public class XRWOriginTrialTest {
    private static final String ORIGIN_TRIAL_HEADER = "Origin-Trial";
    private static final String CRITICAL_ORIGIN_TRIAL_HEADER = "Critical-Origin-Trial";
    private static final String XRW_HEADER = "X-Requested-With";

    private static final String PERSISTENT_TRIAL_NAME = "WebViewXRequestedWithDeprecation";

    private static final String REQUEST_PATH = "/";

    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mServer;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    @SuppressLint("VisibleForTests")
    @After
    public void tearDown() throws Exception {
        // Clean up the stored tokens after tests
        mActivityTestRule.runOnUiThread(() -> {
            AwBrowserContext context = mActivityTestRule.getAwBrowserContext();
            context.clearPersistentOriginTrialStorageForTesting();
        });
        mActivityTestRule.destroyAwContentsOnMainSync(mAwContents);
        // Tests are expected to start their servers on different ports, but they should all be shut
        // down at the end.
        mServer.shutdown();
    }

    @Test
    @SmallTest
    public void testNoHeaderWithoutOriginTrial() throws Throwable {
        // Port number unique to this test method. Other tests should use different ports.
        mServer = TestWebServer.start(22433);
        String requestUrl = setResponseHeadersForUrl(Collections.emptyMap());

        loadUrlSync(requestUrl);
        Assert.assertFalse(getLastRequestHeaders().containsKey(XRW_HEADER));
        Assert.assertEquals(1, mServer.getRequestCount(REQUEST_PATH));

        // Make a second request to verify the header stays off if no trial is set.
        loadUrlSync(requestUrl);
        Assert.assertFalse(getLastRequestHeaders().containsKey(XRW_HEADER));
        Assert.assertEquals(2, mServer.getRequestCount(REQUEST_PATH));
    }

    @Test
    @SmallTest
    public void testHeaderOnSecondRequestWithTrial() throws Throwable {
        // Port number unique to this test method. Other tests should use different ports.
        mServer = TestWebServer.start(22443);
        /* Generated with
           tools/origin_trials/generate_token.py http://localhost:22443 \
           WebViewXRequestedWithDeprecation --expire-timestamp=2000000000
        */
        final String trialToken =
                "AyNFCtEW5OxS8NeOGQ5IN10l6pQiiDRWtvgLq7teZDxi7gl//fxZ/EBVYXDWqYs8LQ5IhCx/xya5ZHh1NT"
                + "FA1AwAAABpeyJvcmlnaW4iOiAiaHR0cDovL2xvY2FsaG9zdDoyMjQ0MyIsICJmZWF0dXJlIjogIl"
                + "dlYlZpZXdYUmVxdWVzdGVkV2l0aERlcHJlY2F0aW9uIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";

        var headers = Map.of(ORIGIN_TRIAL_HEADER, trialToken);
        String requestUrl = setResponseHeadersForUrl(headers);

        loadUrlSync(requestUrl);
        Assert.assertFalse(getLastRequestHeaders().containsKey(XRW_HEADER));
        Assert.assertEquals(1, mServer.getRequestCount(REQUEST_PATH));

        // Make a second request to verify the header is enabled when the trial is active.
        loadUrlSync(requestUrl);
        Assert.assertTrue(getLastRequestHeaders().containsKey(XRW_HEADER));
        Assert.assertEquals(2, mServer.getRequestCount(REQUEST_PATH));
    }

    @Test
    @SmallTest
    public void testCriticalEnablesHeader() throws Throwable {
        // Port number unique to this test method. Other tests should use different ports.
        mServer = TestWebServer.start(22453);
        /* Generated with
           tools/origin_trials/generate_token.py http://localhost:22453 \
           WebViewXRequestedWithDeprecation --expire-timestamp=2000000000
        */
        final String trialToken =
                "A/kHrOHw8h7WZ6L54iqkbhLpjf8m6dhrvKfZ1IQS3lF32ZFowFpx3E9LFYftApKPUZ5HBkSr5GI1UQra8c"
                + "nK3QQAAABpeyJvcmlnaW4iOiAiaHR0cDovL2xvY2FsaG9zdDoyMjQ1MyIsICJmZWF0dXJlIjogIl"
                + "dlYlZpZXdYUmVxdWVzdGVkV2l0aERlcHJlY2F0aW9uIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";
        var headers = Map.of(ORIGIN_TRIAL_HEADER, trialToken, CRITICAL_ORIGIN_TRIAL_HEADER,
                PERSISTENT_TRIAL_NAME);

        String requestUrl = setResponseHeadersForUrl(headers);

        // When the trial is requested as critical, the WebView should automatically make two
        // requests to enable the trial on the last one.
        loadUrlSync(requestUrl);
        Assert.assertTrue(getLastRequestHeaders().containsKey(XRW_HEADER));
        Assert.assertEquals(2, mServer.getRequestCount(REQUEST_PATH));
    }

    private void loadUrlSync(String requestUrl) throws Exception {
        OnPageFinishedHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, requestUrl);
        Assert.assertEquals(requestUrl, onPageFinishedHelper.getUrl());
    }

    @NonNull
    private Map<String, String> getLastRequestHeaders() {
        HTTPRequest lastRequest = mServer.getLastRequest(REQUEST_PATH);
        HTTPHeader[] headers = lastRequest.getHeaders();
        Map<String, String> headerMap = new HashMap<>();
        for (var header : headers) {
            headerMap.put(header.key, header.value);
        }
        return headerMap;
    }

    private String setResponseHeadersForUrl(Map<String, String> headers) {
        List<Pair<String, String>> headerList = new ArrayList<>();
        for (var entry : headers.entrySet()) {
            headerList.add(new Pair<>(entry.getKey(), entry.getValue()));
        }
        return mServer.setResponse("/", "<!DOCTYPE html><html><body>Hello, World", headerList);
    }
}
