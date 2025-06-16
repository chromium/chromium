// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.annotation.NonNull;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

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
import org.chromium.android_webview.AwNoVarySearchData;
import org.chromium.android_webview.AwPrefetchParameters;
import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.android_webview.test.AwPrefetchTest.TestAwPrefetchCallback;
import org.chromium.android_webview.test.TestWebMessageListener.Data;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.ServerCertificate;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.net.test.util.WebServer;
import org.chromium.net.test.util.WebServer.HTTPRequest;
import org.chromium.url.GURL;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/** Tests for {@link org.chromium.android_webview.AwBrowserContext#setOriginMatchedHeader}. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class ProfileExtraHeadersTest extends AwParameterizedTest {

    private static final String REQUEST_PATH = "/";

    private static final String BASIC_PREFETCH_URL = "/android_webview/test/data/hello_world.html";

    public static final String SW_INDEX_URL = "/index.html";
    public static final String SW_URL = "/sw.js";
    public static final String SW_FETCH_URL = "/content.txt";

    private static final String SW_INDEX_HTML =
            """
          <!DOCTYPE html>
          <script>
              function setState(newState) {
                  testListener.postMessage(newState);
              }
              function swReady(sw) {
                  setState('sw_ready');
                  sw.postMessage({fetches: 1});
              }
              navigator.serviceWorker.register('sw.js')
                  .then(sw_reg => {
                      setState('sw_registered');
                      let sw = sw_reg.installing || sw_reg.waiting || sw_reg.active;
                      if (sw.state == 'activated') {
                          swReady(sw);
                      } else {
                          sw.addEventListener('statechange', e => {
                              if (e.target.state == 'activated') swReady(e.target);
                          });
                      }
                  }).catch(err => {
                      setState('sw_registration_error');
                  });
              navigator.serviceWorker.addEventListener('message',
                  event => setState(event.data.msg));
              setState('page_loaded');
          </script>
          """;

    private static final String NETWORK_ACCESS_SW_JS =
            """
          self.addEventListener('message', async event => {
              try {
                  const resp = await fetch('content.txt');
                  if (resp && resp.ok) {
                      event.source.postMessage({ msg: await resp.text() });
                  } else {
                      event.source.postMessage({ msg: 'fetch_not_ok' });
                  }
              } catch {
                  event.source.postMessage({ msg: 'fetch_catch' });
              }
          });
          """;

    private static final String FETCH_CONTENT = "fetch_success";

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwContents mAwContents;
    private AwBrowserContext mAwBrowserContext;

    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();

    public ProfileExtraHeadersTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        AwTestContainerView container =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = container.getAwContents();
        mAwBrowserContext = mAwContents.getBrowserContextInternal();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(mAwBrowserContext::clearAllOriginMatchedHeaders);
    }

    private void setOriginMatchedHeaderOnUiThread(
            String headerName, String headerValue, Set<String> originRules) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwBrowserContext.setOriginMatchedHeader(
                                headerName, headerValue, originRules));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testHeaderNameAndValuesAreValidated() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () ->
                                    mAwBrowserContext.setOriginMatchedHeader(
                                            "",
                                            "NameShouldNotBeEmpty",
                                            Set.of("https://*.example.com")));
                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () ->
                                    mAwBrowserContext.setOriginMatchedHeader(
                                            "X-ShouldNotHaveNewline\n",
                                            "Value",
                                            Set.of("https://*.example.com")));

                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () ->
                                    mAwBrowserContext.setOriginMatchedHeader(
                                            "X-ShouldNotHaveNewline",
                                            "Value\n",
                                            Set.of("https://*.example.com")));
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void willAttachHeader() throws Exception {
        try (TestWebServer server = TestWebServer.start(); ) {
            String requestUrl = setMockResponse(server);

            setOriginMatchedHeaderOnUiThread(
                    "X-ExtraHeader", "active", getPatternSetForUrl(requestUrl));

            Assert.assertTrue(
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> mAwBrowserContext.hasOriginMatchedHeader("X-ExtraHeader")));
            Assert.assertFalse(
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> mAwBrowserContext.hasOriginMatchedHeader("X-OtherHeader")));

            try (HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectBooleanRecordTimes(
                                    "Android.WebView.AndroidX.Profile.ExtraHeaderAttached", true, 1)
                            .build()) {
                mActivityTestRule.loadUrlSync(
                        mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            }
            Assert.assertEquals(1, server.getRequestCount(REQUEST_PATH));
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            Assert.assertEquals("active", lastRequest.headerValue("X-ExtraHeader"));

            // Test that we can attach multiple headers
            setOriginMatchedHeaderOnUiThread(
                    "X-OtherHeader", "enabled", getPatternSetForUrl(requestUrl));
            Assert.assertTrue(
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> mAwBrowserContext.hasOriginMatchedHeader("X-OtherHeader")));

            try (HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectBooleanRecordTimes(
                                    "Android.WebView.AndroidX.Profile.ExtraHeaderAttached", true, 2)
                            .build()) {
                mActivityTestRule.loadUrlSync(
                        mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            }
            Assert.assertEquals(2, server.getRequestCount(REQUEST_PATH));
            lastRequest = server.getLastRequest(REQUEST_PATH);
            Assert.assertEquals("enabled", lastRequest.headerValue("X-OtherHeader"));
            Assert.assertEquals("active", lastRequest.headerValue("X-ExtraHeader"));

            ThreadUtils.runOnUiThreadBlocking(
                    () -> mAwBrowserContext.clearOriginMatchedHeader("X-ExtraHeader"));
            Assert.assertFalse(
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> mAwBrowserContext.hasOriginMatchedHeader("X-ExtraHeader")));

            try (HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectBooleanRecordTimes(
                                    "Android.WebView.AndroidX.Profile.ExtraHeaderAttached", true, 1)
                            .build()) {
                mActivityTestRule.loadUrlSync(
                        mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            }
            Assert.assertEquals(3, server.getRequestCount(REQUEST_PATH));
            Assert.assertEquals(
                    "", server.getLastRequest(REQUEST_PATH).headerValue("X-ExtraHeader"));
            Assert.assertEquals("enabled", lastRequest.headerValue("X-OtherHeader"));
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void canClearAllHeaders() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            setOriginMatchedHeaderOnUiThread(
                    "X-ExtraHeader", "active", getPatternSetForUrl(requestUrl));
            setOriginMatchedHeaderOnUiThread(
                    "X-OtherHeader", "enabled", getPatternSetForUrl(requestUrl));

            try (HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectBooleanRecordTimes(
                                    "Android.WebView.AndroidX.Profile.ExtraHeaderAttached", true, 2)
                            .build()) {
                mActivityTestRule.loadUrlSync(
                        mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            }
            Assert.assertEquals(1, server.getRequestCount(REQUEST_PATH));
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            Assert.assertEquals("active", lastRequest.headerValue("X-ExtraHeader"));
            Assert.assertEquals("enabled", lastRequest.headerValue("X-OtherHeader"));

            ThreadUtils.runOnUiThreadBlocking(mAwBrowserContext::clearAllOriginMatchedHeaders);

            try (HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectNoRecords("Android.WebView.AndroidX.Profile.ExtraHeaderAttached")
                            .build()) {
                mActivityTestRule.loadUrlSync(
                        mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            }

            Assert.assertEquals(2, server.getRequestCount(REQUEST_PATH));
            lastRequest = server.getLastRequest(REQUEST_PATH);
            Assert.assertEquals("", lastRequest.headerValue("X-ExtraHeader"));
            Assert.assertEquals("", lastRequest.headerValue("X-OtherHeader"));
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void appliesHeadersFromServiceWorkers() throws Exception {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestWebMessageListener listener = addTestWebMessageListener();
        try (TestWebServer server = TestWebServer.start()) {
            final String fullIndexUrl = server.setResponse(SW_INDEX_URL, SW_INDEX_HTML, null);
            server.setResponse(SW_URL, NETWORK_ACCESS_SW_JS, null);
            server.setResponse(SW_FETCH_URL, FETCH_CONTENT, null);

            setOriginMatchedHeaderOnUiThread(
                    "X-ExtraHeader", "active", getPatternSetForUrl(fullIndexUrl));

            // Load the service worker page and wait until it has finished loading
            mActivityTestRule.loadUrlAsync(mAwContents, fullIndexUrl);
            Data postMessage;
            while ((postMessage = listener.waitForOnPostMessage()) != null) {
                if (FETCH_CONTENT.equals(postMessage.getAsString())) {
                    break;
                }
            }

            // Assert that the fetch from the service worker had the header attached.
            Assert.assertEquals(1, server.getRequestCount(SW_FETCH_URL));
            Assert.assertEquals(
                    "active", server.getLastRequest(SW_FETCH_URL).headerValue("X-ExtraHeader"));
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void headerNotChangedOnValidationError() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            Set<String> originPatterns = getPatternSetForUrl(requestUrl);
            setOriginMatchedHeaderOnUiThread("X-ExtraHeader", "active", originPatterns);

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            Assert.assertEquals(1, server.getRequestCount(REQUEST_PATH));
            Assert.assertEquals(
                    "active", server.getLastRequest(REQUEST_PATH).headerValue("X-ExtraHeader"));

            // Perform an invalid update and assert that state has not changed.
            try {
                setOriginMatchedHeaderOnUiThread("X-OtherHeader", "active", Set.of("--"));
                Assert.fail("We expected an exception to be thrown");
            } catch (Exception ignored) {
            }

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            Assert.assertEquals(2, server.getRequestCount(REQUEST_PATH));
            Assert.assertEquals(
                    "active", server.getLastRequest(REQUEST_PATH).headerValue("X-ExtraHeader"));
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void onlyAttachesHeaderToRequestedOriginDuringRedirect() throws Exception {

        String startPath = "/start_attach.html";
        String redirectToOtherPath = "/redirect_to_other_attach.html";
        String redirectBackPath = "/redirect_back_noattach.html";
        String finishPath = "/finish_attach.html";

        try (TestWebServer serverA = TestWebServer.start();
                TestWebServer serverB = TestWebServer.startAdditional()) {
            // Set up an A -> A -> B -> A redirect chain
            String finishUrl = serverA.setResponse(finishPath, "Done!", Collections.emptyList());
            String redirectBackUrl = serverB.setRedirect(redirectBackPath, finishUrl);
            String redirectToOtherUrl = serverA.setRedirect(redirectToOtherPath, redirectBackUrl);
            String startUrl = serverA.setRedirect(startPath, redirectToOtherUrl);

            // Set the extra header for the 'A' server.
            setOriginMatchedHeaderOnUiThread(
                    "X-ExtraHeader", "active", getPatternSetForUrl(startUrl));

            // Expect the header attached once on each of the 3 requests to serverA, but no
            // histogram emitted for the request to serverB.
            try (HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectBooleanRecordTimes(
                                    "Android.WebView.AndroidX.Profile.ExtraHeaderAttached", true, 3)
                            .build()) {
                mActivityTestRule.loadUrlSync(
                        mAwContents, mContentsClient.getOnPageFinishedHelper(), startUrl);
            }

            Assert.assertEquals(1, serverA.getRequestCount(startPath));
            Assert.assertEquals(1, serverA.getRequestCount(redirectToOtherPath));
            Assert.assertEquals(1, serverA.getRequestCount(finishPath));

            Assert.assertEquals(1, serverB.getRequestCount(redirectBackPath));

            Assert.assertEquals(
                    "active", serverA.getLastRequest(startPath).headerValue("X-ExtraHeader"));

            Assert.assertEquals(
                    "active",
                    serverA.getLastRequest(redirectToOtherPath).headerValue("X-ExtraHeader"));

            Assert.assertEquals(
                    "active", serverA.getLastRequest(finishPath).headerValue("X-ExtraHeader"));

            Assert.assertEquals(
                    "", serverB.getLastRequest(redirectBackPath).headerValue("X-ExtraHeader"));
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void onlyAttachesHeaderToRequestedOriginDuringRedirectThroughOrigin() throws Exception {

        String startPath = "/start_noattach.html";
        String redirectBackPath = "/redirect_back_attach.html";
        String finishPath = "/finish_noattach.html";

        try (TestWebServer serverA = TestWebServer.start();
                TestWebServer serverB = TestWebServer.startAdditional()) {
            // Set up an B -> A -> B redirect chain
            String finishUrl = serverB.setResponse(finishPath, "Done!", Collections.emptyList());
            String redirectBackUrl = serverA.setRedirect(redirectBackPath, finishUrl);
            String startUrl = serverB.setRedirect(startPath, redirectBackUrl);

            // Set the extra header for the 'A' server.
            setOriginMatchedHeaderOnUiThread(
                    "X-ExtraHeader", "active", getPatternSetForUrl(serverA.getBaseUrl()));

            // Expect the header attached once for the request to serverA, but not for the requests
            // to serverB.
            try (HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectBooleanRecordTimes(
                                    "Android.WebView.AndroidX.Profile.ExtraHeaderAttached", true, 1)
                            .build()) {

                mActivityTestRule.loadUrlSync(
                        mAwContents, mContentsClient.getOnPageFinishedHelper(), startUrl);
            }

            Assert.assertEquals(1, serverB.getRequestCount(startPath));
            Assert.assertEquals(1, serverB.getRequestCount(finishPath));
            Assert.assertEquals(1, serverA.getRequestCount(redirectBackPath));

            Assert.assertEquals("", serverB.getLastRequest(startPath).headerValue("X-ExtraHeader"));

            Assert.assertEquals(
                    "", serverB.getLastRequest(finishPath).headerValue("X-ExtraHeader"));

            Assert.assertEquals(
                    "active",
                    serverA.getLastRequest(redirectBackPath).headerValue("X-ExtraHeader"));
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void willSupplyHeaderInShouldInterceptRequest() throws Exception {
        final String requestUrl = "http://example.com/";

        CompletableFuture<Map<String, String>> interceptFuture = new CompletableFuture<>();

        TestAwContentsClient interceptClient =
                new TestAwContentsClient() {
                    @Override
                    public WebResourceResponseInfo shouldInterceptRequest(
                            AwWebResourceRequest request) {
                        if (requestUrl.equals(request.getUrl())) {
                            interceptFuture.complete(request.getRequestHeaders());
                        }
                        return super.shouldInterceptRequest(request);
                    }
                };

        AwTestContainerView container =
                mActivityTestRule.createAwTestContainerViewOnMainSync(interceptClient);
        mAwContents = container.getAwContents();
        mAwBrowserContext = mAwContents.getBrowserContextInternal();

        setOriginMatchedHeaderOnUiThread(
                "X-ExtraHeader", "active", getPatternSetForUrl(requestUrl));

        mActivityTestRule.loadUrlAsync(mAwContents, requestUrl);

        Map<String, String> interceptedHeaders = interceptFuture.get(1, TimeUnit.SECONDS);
        Assert.assertEquals("active", interceptedHeaders.get("X-ExtraHeader"));
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void willAddHeaderToPrefetchRequests() throws Throwable {

        // EmbeddedTestServer handles closing internally.
        AwEmbeddedTestServer sslServer =
                AwEmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_TEST_NAMES);

        String prefetchUrl = sslServer.getURLWithHostName("a.test", BASIC_PREFETCH_URL);

        setOriginMatchedHeaderOnUiThread(
                "X-ExtraHeader", "active", getPatternSetForUrl(prefetchUrl));

        // Do the prefetch request.
        TestAwPrefetchCallback callback = startPrefetching(prefetchUrl);

        // wait then do the checks
        callback.getOnStatusUpdatedHelper().waitForNext();
        Assert.assertEquals(1, sslServer.getRequestCountForUrl(BASIC_PREFETCH_URL));
        HashMap<String, String> headerForUrl =
                sslServer.getRequestHeadersForUrl(BASIC_PREFETCH_URL);
        Assert.assertEquals("active", headerForUrl.get("X-ExtraHeader"));
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    @Test
    public void doesNotOverrideJsFetchHeaders() throws Exception {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestWebMessageListener listener = addTestWebMessageListener();

        String mainContent =
                """
            <!DOCTYPE html>
            <html><body><script>
            fetch("/fetch.res", {headers: {"X-ExtraHeader": "SetByJs"}}).then(resp => {
              testListener.postMessage("done");
            });
            </script></body></html>
            """;
        try (TestWebServer server = TestWebServer.start(); ) {
            final String fullIndexUrl = server.setResponse("/index.html", mainContent, null);
            server.setResponse("/fetch.res", "hello, world", null);

            setOriginMatchedHeaderOnUiThread(
                    "X-ExtraHeader", "SetByApplication", getPatternSetForUrl(server.getBaseUrl()));
            setOriginMatchedHeaderOnUiThread(
                    "X-ApplicationHeader",
                    "SetByApplication",
                    getPatternSetForUrl(server.getBaseUrl()));
            // Expect both headers attached on the initial request, but only 1 header attached on
            // the fetch: 3 success, 1 failure to attach.
            try (HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectBooleanRecordTimes(
                                    "Android.WebView.AndroidX.Profile.ExtraHeaderAttached", true, 3)
                            .expectBooleanRecordTimes(
                                    "Android.WebView.AndroidX.Profile.ExtraHeaderAttached",
                                    false,
                                    1)
                            .build()) {
                mActivityTestRule.loadUrlAsync(mAwContents, fullIndexUrl);
                listener.waitForOnPostMessage();
            }

            // Assert that the fetch request did not have it's custom header replaced, but other
            // headers were still added.
            Assert.assertEquals(1, server.getRequestCount("/fetch.res"));
            Assert.assertEquals(
                    "SetByJs", server.getLastRequest("/fetch.res").headerValue("X-ExtraHeader"));
            Assert.assertEquals(
                    "SetByApplication",
                    server.getLastRequest("/fetch.res").headerValue("X-ApplicationHeader"));
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    @Test
    public void willAttachHeaderOnCrossOriginResourceRequests() throws Exception {
        // Explicitly enable image loading to make the test resilient against external settings
        // changes (e.g. variations).
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getSettings().setImagesEnabled(true));
        String mainContentTemplate =
                """
            <!DOCTYPE html>
            <html><body><img src="%s"></body></html>
            """;
        try (TestWebServer server = TestWebServer.start();
                TestWebServer corsServer = TestWebServer.startAdditional()) {

            setOriginMatchedHeaderOnUiThread(
                    "X-ApplicationHeader",
                    "SetByApplication",
                    getPatternSetForUrl(corsServer.getBaseUrl()));

            String corsUrl = corsServer.setEmptyResponse("/cors.png");
            String mainContent = String.format(mainContentTemplate, corsUrl);
            String mainUrl = server.setResponse("/index.html", mainContent, null);

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), mainUrl);

            HTTPRequest lastRequest = server.getLastRequest("/index.html");
            Assert.assertEquals("", lastRequest.headerValue("X-ApplicationHeader"));

            Assert.assertEquals(1, corsServer.getRequestCount("/cors.png"));
            HTTPRequest corsRequest = corsServer.getLastRequest("/cors.png");
            Assert.assertEquals("SetByApplication", corsRequest.headerValue("X-ApplicationHeader"));
        }
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    @Test
    public void doesNotAttachHeaderToCorsPreflights() throws Exception {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestWebMessageListener listener = addTestWebMessageListener();

        String mainContentTemplate =
                """
        <!DOCTYPE html>
        <html><body><script>
        fetch("%s", {headers: {"X-ExtraHeader": "SetByJs"}}).then(resp => {
          testListener.postMessage("done");
        });
        </script></body></html>
        """;
        String preflightResponse =
                """
        HTTP/1.1 204 No Content
        Access-Control-Allow-Origin: *
        Access-Control-Allow-Methods: GET
        Access-Control-Allow-Headers: x-extraheader

        """;
        try (TestWebServer server = TestWebServer.start();
                TestWebServer corsServer = TestWebServer.startAdditional()) {

            String fetchPath = "/fetch.res";
            String corsUrl = corsServer.getResponseUrl(fetchPath);
            String mainContent = String.format(mainContentTemplate, corsUrl);
            final String fullIndexUrl = server.setResponse("/index.html", mainContent, null);
            setOriginMatchedHeaderOnUiThread(
                    "X-ApplicationHeader",
                    "SetByApplication",
                    getPatternSetForUrl(corsServer.getBaseUrl()));

            BlockingQueue<HTTPRequest> requests = new LinkedBlockingQueue<>();

            corsServer.setRequestHandler(
                    (request, stream) -> {
                        requests.add(request);
                        try {
                            switch (request.getMethod()) {
                                case "OPTIONS" -> {
                                    stream.write(
                                            preflightResponse.getBytes(StandardCharsets.UTF_8));
                                }
                                case "GET" -> {
                                    String statusAndHeaders =
                                            WebServer.STATUS_OK
                                                    + "\r\n"
                                                    + "Access-Control-Allow-Origin: *";
                                    WebServer.writeResponse(
                                            stream,
                                            statusAndHeaders,
                                            "content".getBytes(StandardCharsets.UTF_8));
                                }
                                default -> Assert.fail("Got method: " + request.getMethod());
                            }
                        } catch (IOException e) {
                            throw new RuntimeException(e);
                        }
                    });

            try (HistogramWatcher watcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            "Android.WebView.AndroidX.Profile.ExtraHeaderTargetsCorsPreflight",
                            true)) {
                mActivityTestRule.loadUrlAsync(mAwContents, fullIndexUrl);
                listener.waitForOnPostMessage();
            }
            Assert.assertEquals(2, requests.size());
            HTTPRequest preflightRequest = requests.take();
            HTTPRequest getRequest = requests.take();

            Assert.assertEquals(
                    "x-extraheader",
                    preflightRequest.headerValue("Access-Control-Request-Headers"));
            Assert.assertEquals("", preflightRequest.headerValue("X-ApplicationHeader"));

            Assert.assertEquals("SetByApplication", getRequest.headerValue("X-ApplicationHeader"));
            Assert.assertEquals("SetByJs", getRequest.headerValue("X-ExtraHeader"));
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void doesNotReplaceHeaderFromLoadUrl() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);
            setOriginMatchedHeaderOnUiThread(
                    "X-ExtraHeader", "SetFromProfile", getPatternSetForUrl(requestUrl));

            try (HistogramWatcher watcher =
                    HistogramWatcher.newBuilder()
                            .expectBooleanRecord(
                                    "Android.WebView.AndroidX.Profile.ExtraHeaderAttached", false)
                            .build()) {
                mActivityTestRule.loadUrlSync(
                        mAwContents,
                        mContentsClient.getOnPageFinishedHelper(),
                        requestUrl,
                        Map.of("X-ExtraHeader", "SetByLoadUrl"));
            }
            Assert.assertEquals(1, server.getRequestCount(REQUEST_PATH));
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            Assert.assertEquals("SetByLoadUrl", lastRequest.headerValue("X-ExtraHeader"));
        }
    }

    @NonNull
    private TestWebMessageListener addTestWebMessageListener() {
        TestWebMessageListener listener = new TestWebMessageListener();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.addWebMessageListener(
                                "testListener", new String[] {"*"}, listener));
        return listener;
    }

    private static String setMockResponse(TestWebServer server) {
        return server.setResponse(REQUEST_PATH, "Hello, World", Collections.emptyList());
    }

    @NonNull
    private static Set<String> getPatternSetForUrl(String requestUrl) {
        GURL gurl = new GURL(requestUrl);
        return Set.of(
                gurl.getScheme()
                        + "://"
                        + gurl.getHost()
                        + (gurl.getPort().isEmpty() ? "" : ":" + gurl.getPort()));
    }

    private TestAwPrefetchCallback startPrefetching(String url) {
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(
                        Collections.emptyMap(),
                        new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null),
                        true);

        TestAwPrefetchCallback callback = new TestAwPrefetchCallback();
        int prefetchKey =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mActivityTestRule
                                        .getAwBrowserContext()
                                        .getPrefetchManager()
                                        .startPrefetchRequest(
                                                url, prefetchParameters, callback, Runnable::run));
        callback.setPrefetchKey(prefetchKey);
        return callback;
    }
}
