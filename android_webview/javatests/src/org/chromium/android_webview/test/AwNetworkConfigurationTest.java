// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.webkit.JavascriptInterface;

import com.google.common.util.concurrent.SettableFuture;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.base.BuildInfo;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.net.URLEncoder;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * A test suite for WebView's network-related configuration. This tests WebView's default settings,
 * which are configured by either AwURLRequestContextGetter or NetworkContext.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwNetworkConfigurationTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testSHA1LocalAnchorsAllowed() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_SHA1_LEAF);
        try {
            CallbackHelper onReceivedSslErrorHelper = mContentsClient.getOnReceivedSslErrorHelper();
            int count = onReceivedSslErrorHelper.getCallCount();
            String url = mTestServer.getURL("/android_webview/test/data/hello_world.html");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            if (BuildInfo.isAtLeastQ()) {
                Assert.assertEquals("We should generate an SSL error on >= Q", count + 1,
                        onReceivedSslErrorHelper.getCallCount());
            } else {
                Assert.assertEquals("We should not have received any SSL errors on < Q", count,
                        onReceivedSslErrorHelper.getCallCount());
            }
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithHeaderMainFrame() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), echoHeaderUrl);
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            final String xRequestedWith = mActivityTestRule.getJavaScriptResultBodyTextContent(
                    mAwContents, mContentsClient);
            final String packageName = InstrumentationRegistry.getInstrumentation()
                                               .getTargetContext()
                                               .getPackageName();
            Assert.assertEquals("X-Requested-With header should be the app package name",
                    packageName, xRequestedWith);
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithHeaderSubResource() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            // Use the test server's root path as the baseUrl to satisfy same-origin restrictions.
            final String baseUrl = mTestServer.getURL("/");
            final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
            final String pageWithIframeHtml = "<html><body><p>Main frame</p><iframe src='"
                    + echoHeaderUrl + "'/></body></html>";
            // We use loadDataWithBaseUrlSync because we need to dynamically control the HTML
            // content, which EmbeddedTestServer doesn't support. We don't need to encode content
            // because loadDataWithBaseUrl() encodes content itself.
            mActivityTestRule.loadDataWithBaseUrlSync(mAwContents,
                    mContentsClient.getOnPageFinishedHelper(), pageWithIframeHtml,
                    /* mimeType */ null, /* isBase64Encoded */ false, baseUrl, /* historyUrl */
                    null);
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            final String xRequestedWith =
                    getJavaScriptResultIframeTextContent(mAwContents, mContentsClient);
            final String packageName = InstrumentationRegistry.getInstrumentation()
                                               .getTargetContext()
                                               .getPackageName();
            Assert.assertEquals("X-Requested-With header should be the app package name",
                    packageName, xRequestedWith);
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithHeaderHttpRedirect() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
            final String encodedEchoHeaderUrl = URLEncoder.encode(echoHeaderUrl, "UTF-8");
            // Returns a server-redirect (301) to echoHeaderUrl.
            final String redirectToEchoHeaderUrl =
                    mTestServer.getURL("/server-redirect?" + encodedEchoHeaderUrl);
            mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                    redirectToEchoHeaderUrl);
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            final String xRequestedWith = mActivityTestRule.getJavaScriptResultBodyTextContent(
                    mAwContents, mContentsClient);
            final String packageName = InstrumentationRegistry.getInstrumentation()
                                               .getTargetContext()
                                               .getPackageName();
            Assert.assertEquals("X-Requested-With header should be the app package name",
                    packageName, xRequestedWith);
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithApplicationValuePreferred() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
            final String applicationRequestedWithValue = "foo";
            final Map<String, String> extraHeaders = new HashMap<>();
            extraHeaders.put("X-Requested-With", applicationRequestedWithValue);
            mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                    echoHeaderUrl, extraHeaders);
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            final String xRequestedWith = mActivityTestRule.getJavaScriptResultBodyTextContent(
                    mAwContents, mContentsClient);
            Assert.assertEquals("Should prefer app's provided X-Requested-With header",
                    applicationRequestedWithValue, xRequestedWith);
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithHeaderShouldInterceptRequest() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            final String url = mTestServer.getURL("/any-http-url-will-suffice.html");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            AwWebResourceRequest request =
                    mContentsClient.getShouldInterceptRequestHelper().getRequestsForUrl(url);
            Assert.assertFalse("X-Requested-With should be invisible to shouldInterceptRequest",
                    request.requestHeaders.containsKey("X-Requested-With"));
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testAccessControlAllowOriginHeader() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

            final SettableFuture<Boolean> fetchResultFuture = SettableFuture.create();
            Object injectedObject = new Object() {
                @JavascriptInterface
                public void success() {
                    fetchResultFuture.set(true);
                }
                @JavascriptInterface
                public void error() {
                    fetchResultFuture.set(false);
                }
            };
            AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                    mAwContents, injectedObject, "injectedObject");

            // The test server will add the Access-Control-Allow-Origin header to the HTTP response
            // for this resource. We should check WebView correctly respects this.
            final String fetchWithAllowOrigin =
                    mTestServer.getURL("/set-header?Access-Control-Allow-Origin:%20*");
            String html = "<html>"
                    + "  <head>"
                    + "  </head>"
                    + "  <body>"
                    + "    HTML content does not matter."
                    + "  </body>"
                    + "</html>";
            final String baseUrl = "http://some.origin.test/index.html";
            mActivityTestRule.loadDataWithBaseUrlSync(mAwContents,
                    mContentsClient.getOnPageFinishedHelper(), html,
                    /* mimeType */ null, /* isBase64Encoded */ false, baseUrl,
                    /* historyUrl */ null);

            String script = "fetch('" + fetchWithAllowOrigin + "')"
                    + "  .then(() => { injectedObject.success(); })"
                    + "  .catch(() => { injectedObject.failure(); });";
            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, script);
            Assert.assertTrue("fetch() should succeed, due to Access-Control-Allow-Origin header",
                    fetchResultFuture.get(WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
            // If we timeout, this indicates the fetch() was erroneously blocked by CORS (as was the
            // root cause of https://crbug.com/960165).
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    /**
     * Like {@link AwActivityTestRule#getJavaScriptResultBodyTextContent}, but it gets the text
     * content of the iframe instead. This assumes the main frame has only a single iframe.
     */
    private String getJavaScriptResultIframeTextContent(
            final AwContents awContents, final TestAwContentsClient viewClient) throws Exception {
        final String script =
                "document.getElementsByTagName('iframe')[0].contentDocument.body.textContent";
        String raw =
                mActivityTestRule.executeJavaScriptAndWaitForResult(awContents, viewClient, script);
        return mActivityTestRule.maybeStripDoubleQuotes(raw);
    }
}
