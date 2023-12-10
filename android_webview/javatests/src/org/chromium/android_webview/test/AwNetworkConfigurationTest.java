// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.os.Build;
import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import com.google.common.util.concurrent.SettableFuture;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedSslErrorHelper;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.url.GURL;

import java.net.URLEncoder;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * A test suite for WebView's network-related configuration. This tests WebView's default settings,
 * which are configured by either AwURLRequestContextGetter or NetworkContext.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwNetworkConfigurationTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private EmbeddedTestServer mTestServer;

    public AwNetworkConfigurationTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    @After
    public void tearDown() throws Exception {
        // Clean up any X-Requested-With allow-lists that test may have set.
        AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        awSettings.setRequestedWithHeaderOriginAllowList(Collections.emptySet());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testSHA1LocalAnchorsAllowed() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_SHA1_LEAF);
        OnReceivedSslErrorHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int count = onReceivedSslErrorHelper.getCallCount();
        String url = mTestServer.getURL("/android_webview/test/data/hello_world.html");
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            Assert.assertEquals(
                    "We should generate an SSL error on >= Q",
                    count + 1,
                    onReceivedSslErrorHelper.getCallCount());
        } else {
            if (count != onReceivedSslErrorHelper.getCallCount()) {
                Assert.fail(
                        "We should not have received any SSL errors on < Q but we received"
                                + " error "
                                + onReceivedSslErrorHelper.getError());
            }
        }
    }

    private static String getPackageName() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName();
    }

    private String getXRequestedWithFromResultBody() throws Exception {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        return mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"disable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderMainFrameLegacy() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), echoHeaderUrl);
        Assert.assertEquals(
                "X-Requested-With header should be the app package name",
                getPackageName(),
                getXRequestedWithFromResultBody());
    }

    private void allowRequestOrigin(String echoHeaderUrl) throws Exception {
        AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        GURL gurl = new GURL(echoHeaderUrl);
        String origin = gurl.getScheme() + "://" + gurl.getHost() + ":" + gurl.getPort();
        awSettings.setRequestedWithHeaderOriginAllowList(Set.of(origin));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"enable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderMainFrame() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), echoHeaderUrl);
        Assert.assertEquals(
                "No X-Requested-With header should be set",
                "None",
                getXRequestedWithFromResultBody());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"enable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderMainFrameOriginAllowed() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        allowRequestOrigin(echoHeaderUrl);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), echoHeaderUrl);
        Assert.assertEquals(
                "X-Requested-With header should be the app package name",
                getPackageName(),
                getXRequestedWithFromResultBody());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"enable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderMainFrameUnrelatedOriginAllowed() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        awSettings.setRequestedWithHeaderOriginAllowList(Set.of("https://google.com"));
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), echoHeaderUrl);
        Assert.assertEquals(
                "No X-Requested-With header should be set",
                "None",
                getXRequestedWithFromResultBody());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"enable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderMainFrameInvalidOriginPattern() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        try {
            // An origin pattern must have a scheme, so this is expected to fail
            awSettings.setRequestedWithHeaderOriginAllowList(Set.of("google.com"));
            Assert.fail("An IllegalArgumentException was expected");
        } catch (IllegalArgumentException expected) {
            // Expected
        }
    }

    private String getXRequestedWithFromIframe() throws Exception {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        final String xRequestedWith =
                getJavaScriptResultIframeTextContent(mAwContents, mContentsClient);
        return xRequestedWith;
    }

    private void loadPageInIframe(String echoHeaderUrl) throws Throwable {
        // Use the test server's root path as the baseUrl to satisfy same-origin restrictions.
        final String baseUrl = mTestServer.getURL("/");
        final String pageWithIframeHtml =
                "<html><body><p>Main frame</p><iframe src='" + echoHeaderUrl + "'/></body></html>";
        // We use loadDataWithBaseUrlSync because we need to dynamically control the HTML
        // content, which EmbeddedTestServer doesn't support. We don't need to encode content
        // because loadDataWithBaseUrl() encodes content itself.
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                pageWithIframeHtml,
                /* mimeType= */ null,
                /* isBase64Encoded= */ false,
                baseUrl,
                /* historyUrl= */ null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"disable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderSubResourceLegacy() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        loadPageInIframe(echoHeaderUrl);
        Assert.assertEquals(
                "X-Requested-With header should be the app package name",
                getPackageName(),
                getXRequestedWithFromIframe());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"enable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderSubResource() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        loadPageInIframe(echoHeaderUrl);
        Assert.assertEquals(
                "X-Requested-With header should not be set", "None", getXRequestedWithFromIframe());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"enable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderSubResourceOriginAllowed() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        allowRequestOrigin(echoHeaderUrl);
        loadPageInIframe(echoHeaderUrl);
        Assert.assertEquals(
                "X-Requested-With header should be the app package name",
                getPackageName(),
                getXRequestedWithFromIframe());
    }

    private void requestRedirectToUrl(String echoHeaderUrl) throws Exception {
        final String encodedEchoHeaderUrl = URLEncoder.encode(echoHeaderUrl, "UTF-8");
        // Returns a server-redirect (301) to echoHeaderUrl.
        final String redirectToEchoHeaderUrl =
                mTestServer.getURL("/server-redirect?" + encodedEchoHeaderUrl);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), redirectToEchoHeaderUrl);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"disable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderHttpRedirectLegacy() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        requestRedirectToUrl(echoHeaderUrl);
        Assert.assertEquals(
                "X-Requested-With header should be the app package name",
                getPackageName(),
                getXRequestedWithFromResultBody());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"enable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderHttpRedirect() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        requestRedirectToUrl(echoHeaderUrl);
        Assert.assertEquals(
                "X-Requested-With header should be None",
                "None",
                getXRequestedWithFromResultBody());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"enable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithHeaderHttpRedirectAllowOrigin() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        allowRequestOrigin(echoHeaderUrl);
        requestRedirectToUrl(echoHeaderUrl);
        Assert.assertEquals(
                "X-Requested-With header should be the app package name",
                getPackageName(),
                getXRequestedWithFromResultBody());
    }

    private void testRequestedWithApplicationValuePreferredBase() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
        final String applicationRequestedWithValue = "foo";
        final Map<String, String> extraHeaders = new HashMap<>();
        extraHeaders.put("X-Requested-With", applicationRequestedWithValue);
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                echoHeaderUrl,
                extraHeaders);
        Assert.assertEquals(
                "Should prefer app's provided X-Requested-With header",
                applicationRequestedWithValue,
                getXRequestedWithFromResultBody());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"disable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithApplicationValuePreferredLegacy() throws Throwable {
        // Application preference should override the header and allow it to be set whether the
        // feature is enabled or not.
        testRequestedWithApplicationValuePreferredBase();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    @CommandLineFlags.Add({"enable-features=WebViewXRequestedWithHeaderControl"})
    public void testRequestedWithApplicationValuePreferred() throws Throwable {
        // Application preference should override the header and allow it to be set whether the
        // feature is enabled or not.
        testRequestedWithApplicationValuePreferredBase();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithHeaderShouldInterceptRequest() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String url = mTestServer.getURL("/android_webview/test/data/hello_world.html");
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        AwWebResourceRequest request =
                mContentsClient.getShouldInterceptRequestHelper().getRequestsForUrl(url);
        Assert.assertFalse(
                "X-Requested-With should be invisible to shouldInterceptRequest",
                request.requestHeaders.containsKey("X-Requested-With"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testAccessControlAllowOriginHeader() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        final SettableFuture<Boolean> fetchResultFuture = SettableFuture.create();
        Object injectedObject =
                new Object() {
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
        String html =
                "<html>"
                        + "  <head>"
                        + "  </head>"
                        + "  <body>"
                        + "    HTML content does not matter."
                        + "  </body>"
                        + "</html>";
        final String baseUrl = "http://some.origin.test/index.html";
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                html,
                /* mimeType= */ null,
                /* isBase64Encoded= */ false,
                baseUrl,
                /* historyUrl= */ null);
        String script =
                "fetch('"
                        + fetchWithAllowOrigin
                        + "')"
                        + "  .then(() => { injectedObject.success(); })"
                        + "  .catch(() => { injectedObject.failure(); });";
        mActivityTestRule.executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, script);
        Assert.assertTrue(
                "fetch() should succeed, due to Access-Control-Allow-Origin header",
                fetchResultFuture.get(WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        // If we timeout, this indicates the fetch() was erroneously blocked by CORS (as was the
        // root cause of https://crbug.com/960165).
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
