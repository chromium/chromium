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
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.base.ThreadUtils;
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
 * <p>This test is temporary, and should be deleted once the WebViewXRequestedWithDeprecation trial
 * ends.
 *
 * <p>Tests in this class start a server on specific ports since the port number is part of the
 * origin, which is encoded in the static trial tokens. To reduce the likelihood of collisions when
 * running the full test suite, the tests use different ports, but it is still possible that the
 * server may not be able to start if the same test is run multiple times in quick succession.
 */
@Batch(Batch.PER_CLASS)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@CommandLineFlags.Add({
    "origin-trial-public-key=dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=",
    "enable-features=PersistentOriginTrials,WebViewXRequestedWithHeaderControl"
})
public class XRWOriginTrialTest extends AwParameterizedTest {
    private static final String ORIGIN_TRIAL_HEADER = "Origin-Trial";
    private static final String CRITICAL_ORIGIN_TRIAL_HEADER = "Critical-Origin-Trial";
    private static final String XRW_HEADER = "X-Requested-With";

    private static final String PERSISTENT_TRIAL_NAME = "WebViewXRequestedWithDeprecation";

    private static final String REQUEST_PATH = "/";

    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    public XRWOriginTrialTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwBrowserContext context = mActivityTestRule.getAwBrowserContext();
                    context.clearPersistentOriginTrialStorageForTesting();
                });
        mActivityTestRule.destroyAwContentsOnMainSync(mAwContents);
    }

    @Test
    @SmallTest
    public void testNoHeaderWithoutOriginTrial() throws Throwable {
        // Port number unique to this test method. Other tests should use different ports.
        try (TestWebServer server = TestWebServer.start(22433)) {
            String requestUrl = setResponseHeadersForUrl(server, Collections.emptyMap());

            loadUrlSync(requestUrl);

            Assert.assertFalse(getLastRequestHeaders(server, REQUEST_PATH).containsKey(XRW_HEADER));
            Assert.assertEquals(1, server.getRequestCount(REQUEST_PATH));

            // Make a second request to verify the header stays off if no trial is set.
            loadUrlSync(requestUrl);
            Assert.assertFalse(getLastRequestHeaders(server, REQUEST_PATH).containsKey(XRW_HEADER));
            Assert.assertEquals(2, server.getRequestCount(REQUEST_PATH));
        }
    }

    @Test
    @SmallTest
    public void testHeaderOnSecondRequestWithTrial() throws Throwable {
        /* Generated with
           tools/origin_trials/generate_token.py http://localhost:22443 \
           WebViewXRequestedWithDeprecation --expire-timestamp=2000000000
        */
        final String trialToken =
                "AyNFCtEW5OxS8NeOGQ5IN10l6pQiiDRWtvgLq7teZDxi7gl//fxZ/EBVYXDWqYs8LQ5IhCx/xya5ZHh1NT"
                    + "FA1AwAAABpeyJvcmlnaW4iOiAiaHR0cDovL2xvY2FsaG9zdDoyMjQ0MyIsICJmZWF0dXJlIjogIl"
                    + "dlYlZpZXdYUmVxdWVzdGVkV2l0aERlcHJlY2F0aW9uIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";
        // Port number unique to this test method. Other tests should use different ports.
        try (TestWebServer server = TestWebServer.start(22443)) {
            var headers = Map.of(ORIGIN_TRIAL_HEADER, trialToken);
            String requestUrl = setResponseHeadersForUrl(server, headers);

            loadUrlSync(requestUrl);
            Assert.assertFalse(getLastRequestHeaders(server, REQUEST_PATH).containsKey(XRW_HEADER));
            Assert.assertEquals(1, server.getRequestCount(REQUEST_PATH));

            // Make a second request to verify the header is enabled when the trial is active.
            loadUrlSync(requestUrl);
            Assert.assertTrue(getLastRequestHeaders(server, REQUEST_PATH).containsKey(XRW_HEADER));
            Assert.assertEquals(2, server.getRequestCount(REQUEST_PATH));
        }
    }

    @Test
    @SmallTest
    public void testCriticalEnablesHeader() throws Throwable {
        /* Generated with
           tools/origin_trials/generate_token.py http://localhost:22453 \
           WebViewXRequestedWithDeprecation --expire-timestamp=2000000000
        */
        final String trialToken =
                "A/kHrOHw8h7WZ6L54iqkbhLpjf8m6dhrvKfZ1IQS3lF32ZFowFpx3E9LFYftApKPUZ5HBkSr5GI1UQra8c"
                    + "nK3QQAAABpeyJvcmlnaW4iOiAiaHR0cDovL2xvY2FsaG9zdDoyMjQ1MyIsICJmZWF0dXJlIjogIl"
                    + "dlYlZpZXdYUmVxdWVzdGVkV2l0aERlcHJlY2F0aW9uIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";
        // Port number unique to this test method. Other tests should use different ports.
        try (TestWebServer server = TestWebServer.start(22453)) {
            var headers =
                    Map.of(
                            ORIGIN_TRIAL_HEADER,
                            trialToken,
                            CRITICAL_ORIGIN_TRIAL_HEADER,
                            PERSISTENT_TRIAL_NAME);

            String requestUrl = setResponseHeadersForUrl(server, headers);

            // When the trial is requested as critical, the WebView should automatically make two
            // requests to enable the trial on the last one.
            loadUrlSync(requestUrl);
            Assert.assertTrue(getLastRequestHeaders(server, REQUEST_PATH).containsKey(XRW_HEADER));
            Assert.assertEquals(2, server.getRequestCount(REQUEST_PATH));
        }
    }

    @Test
    @SmallTest
    public void testThirdPartyTokensEnablesHeader() throws Throwable {
        // Port number unique to this test method. Other tests should use different ports.
        final int mainServerPort = 22463;
        final int thirdPartyPort = 22473;

        // Generated with tools/origin_trials/generate_token.py http://localhost:22473
        // WebViewXRequestedWithDeprecation --expire-timestamp=2000000000 --is-third-party
        final String trialToken =
                "A6cHxkfwJmfXXuUv6PrTqnYqPcrnrDj50ZrzAJaIR394yEKISBDrhAiLecCfb1fSBA/8H4jAHQf0uUREEm"
                    + "HLcwYAAAB/eyJvcmlnaW4iOiAiaHR0cDovL2xvY2FsaG9zdDoyMjQ3MyIsICJmZWF0dXJlIjogIl"
                    + "dlYlZpZXdYUmVxdWVzdGVkV2l0aERlcHJlY2F0aW9uIiwgImV4cGlyeSI6IDIwMDAwMDAwMDAsIC"
                    + "Jpc1RoaXJkUGFydHkiOiB0cnVlfQ==";
        try (TestWebServer primaryServer = TestWebServer.start(mainServerPort);
                TestWebServer thirdPartyServer = TestWebServer.startAdditional(thirdPartyPort)) {
            // This is our target request, which should have the XRW header.
            final String thirdPartyEnabledCheckPath = "/cross-origin";
            String crossOriginUrl =
                    thirdPartyServer.setResponse(
                            thirdPartyEnabledCheckPath,
                            "window.test_done = 1",
                            List.of(new Pair<>("Content-Type", "application/javascript")));

            // This script is loaded from the main origin but injecting a third-party token,
            // which creates a deliberate mismatch between the injecting origin and the target
            // origin.
            String injectJs =
                    "window.test_done = 0;\n"
                            + "const otMeta = document.createElement('meta');\n"
                            + "otMeta.httpEquiv = 'origin-trial';\n"
                            + "otMeta.content = '"
                            + trialToken
                            + "';\n"
                            + "document.head.append(otMeta);\n"
                            + "console.log(otMeta);\n"
                            // Inject an extra script tag to make a request with trial enabled.
                            + "const triggerScriptLoad = function() {\n"
                            + "  const targetScript = document.createElement('script');\n"
                            + "  targetScript.src = '"
                            + crossOriginUrl
                            + "';\n"
                            + "  document.head.append(targetScript);\n"
                            + "  console.log(targetScript);\n"
                            + "};" // triggerScriptLoad
                            + "setTimeout(triggerScriptLoad, 500);";

            // Load the inject script from the primary origin, to confirm we can inject a
            // cross-origin token.
            final String injectScriptPath = "/inject.js";
            String injectUrl =
                    primaryServer.setResponse(
                            injectScriptPath,
                            injectJs,
                            List.of(new Pair<>("Content-Type", "application/javascript")));

            // The main web site which simply loads the injectJs script.
            String mainResponse =
                    "<!DOCTYPE html><head><script src=\""
                            + injectUrl
                            + "\"></script>\n<body>hello world";
            String requestUrl =
                    primaryServer.setResponse(REQUEST_PATH, mainResponse, Collections.emptyList());

            loadUrlSync(requestUrl);
            waitForTestDone();

            Assert.assertEquals(1, primaryServer.getRequestCount(injectScriptPath));
            Assert.assertEquals(1, thirdPartyServer.getRequestCount(thirdPartyEnabledCheckPath));
            Map<String, String> headerMap =
                    getLastRequestHeaders(thirdPartyServer, thirdPartyEnabledCheckPath);
            Assert.assertTrue(headerMap.containsKey(XRW_HEADER));
        }
    }

    /** Wait for {@code window.test_done == 1}. */
    private void waitForTestDone() {
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    try {
                        return Integer.parseInt(
                                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                                mAwContents, mContentsClient, "window.test_done"))
                                == 1;
                    } catch (Exception e) {
                        throw new AssertionError("Unable to get success", e);
                    }
                });
    }

    private void loadUrlSync(String requestUrl) throws Exception {
        OnPageFinishedHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, requestUrl);
        Assert.assertEquals(requestUrl, onPageFinishedHelper.getUrl());
    }

    @NonNull
    private Map<String, String> getLastRequestHeaders(TestWebServer server, String requestPath) {
        HTTPRequest lastRequest = server.getLastRequest(requestPath);
        HTTPHeader[] headers = lastRequest.getHeaders();
        Map<String, String> headerMap = new HashMap<>();
        for (var header : headers) {
            headerMap.put(header.key, header.value);
        }
        return headerMap;
    }

    private String setResponseHeadersForUrl(TestWebServer server, Map<String, String> headers) {
        List<Pair<String, String>> headerList = new ArrayList<>();
        for (var entry : headers.entrySet()) {
            headerList.add(new Pair<>(entry.getKey(), entry.getValue()));
        }
        return server.setResponse("/", "<!DOCTYPE html><html><body>Hello, World", headerList);
    }
}
