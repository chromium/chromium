// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.List;
import java.util.Set;

@Batch(Batch.PER_CLASS)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwCrossOriginIsolatedAllowlistTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private AwTestContainerView mTestContainerView;
    private TestWebMessageListener mListener;

    private static final String INDEX_HTML =
            """
            <html>
            <body>
                <script>

                    const sab = new SharedArrayBuffer(16);
                    const view = new Int32Array(sab);
                    view[0] = 42;
                    const myWorker = new Worker('worker.js');
                    myWorker.addEventListener('message',
                        event => javaListener.postMessage('success'));
                    myWorker.postMessage(sab);
                </script>
            </body>
            </html>
            """;
    private static final String WORKER_JS =
            """
                self.addEventListener('message', e => {
                    const view = new Int32Array(e.data);
                    if (view[0] === 42) {
                        self.postMessage('success');
                    }
                });
            """;

    private static final String ISOLATE_INDEX_HTML =
            """
            <html>
            <body>
            <script>
                javaListener.postMessage(window.crossOriginIsolated ? "true" : "false");
            </script>
            </body>
            </html>
            """;

    public AwCrossOriginIsolatedAllowlistTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.startSsl();

        mListener = new TestWebMessageListener();

        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "javaListener", new String[] {"*"}, mListener);
    }

    @After
    public void tearDown() {
        if (mWebServer != null) mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSharedArrayBufferInWorker() throws Throwable {
        List<Pair<String, String>> requestHeaders =
                List.of(new Pair<>("Document-Isolation-Policy", "isolate-and-credentialless"));
        String fullIndexUrl = mWebServer.setResponse("/index.html", INDEX_HTML, requestHeaders);
        String fullSwUrl = mWebServer.setResponse("/worker.js", WORKER_JS, requestHeaders);

        String baseUrl = mWebServer.getBaseUrl();
        String origin = baseUrl.substring(0, baseUrl.length() - 1);

        mActivityTestRule.getAwBrowserContext().setCrossOriginIsolatedAllowList(Set.of(origin));

        loadPage(fullIndexUrl);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals("success", data.getAsString());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAllowListWithoutHeaderDoesNotEnableIsolationApis() throws Throwable {
        mActivityTestRule.getAwBrowserContext().setCrossOriginIsolatedAllowList(Set.of("*"));

        String fullIndexUrl = mWebServer.setResponse("/index.html", ISOLATE_INDEX_HTML, null);

        loadPage(fullIndexUrl);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals("false", data.getAsString());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMismatchedOriginAllowedDoesNotEnableIsolationApis() throws Throwable {
        List<Pair<String, String>> requestHeaders =
                List.of(new Pair<>("Document-Isolation-Policy", "isolate-and-credentialless"));
        String fullIndexUrl =
                mWebServer.setResponse("/index.html", ISOLATE_INDEX_HTML, requestHeaders);
        mActivityTestRule
                .getAwBrowserContext()
                .setCrossOriginIsolatedAllowList(Set.of("https://example.com"));

        loadPage(fullIndexUrl);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals("false", data.getAsString());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHeaderWithoutAllowListDoesNotEnableIsolationApis() throws Throwable {
        List<Pair<String, String>> requestHeaders =
                List.of(new Pair<>("Document-Isolation-Policy", "isolate-and-credentialless"));
        String fullIndexUrl =
                mWebServer.setResponse("/index.html", ISOLATE_INDEX_HTML, requestHeaders);
        loadPage(fullIndexUrl);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals("false", data.getAsString());
    }

    private void loadPage(final String fullIndexUrl) throws Exception {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), fullIndexUrl);
    }
}
