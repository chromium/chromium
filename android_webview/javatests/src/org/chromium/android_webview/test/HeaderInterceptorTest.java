// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static com.google.common.truth.Truth.assertThat;

import android.os.Bundle;
import android.util.Pair;
import android.webkit.WebSettings;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwNavigationParams;
import org.chromium.android_webview.AwPrefetchCallback;
import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.android_webview.http_headers.AwHeaderInterceptor;
import org.chromium.android_webview.test.AwPrefetchTestBase.TestAwPrefetchCallback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.ServerCertificate;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.net.test.util.WebServer.HTTPRequest;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Predicate;

/** Tests for registering interceptors via {@code GetHeaderInterceptorStore()}. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class HeaderInterceptorTest extends AwParameterizedTest {

    private static final String REQUEST_PATH = "/";
    private static final String BASIC_PREFETCH_URL = "/android_webview/test/data/hello_world.html";

    private static final String SW_INDEX_URL = "/index.html";
    private static final String SW_URL = "/sw.js";
    private static final String SW_FETCH_URL = "/content.txt";

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

    private static final String SW_FETCH_CONTENT = "fetch_success";

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwCookieManager mCookieManager;
    private AwContents mAwContents;
    private AwBrowserContext mAwBrowserContext;

    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();

    public HeaderInterceptorTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        AwTestContainerView container =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mCookieManager = new AwCookieManager();
        mAwContents = container.getAwContents();
        mAwBrowserContext = mAwContents.getBrowserContextInternal();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwBrowserContext.getHeaderInterceptorStore().clearInterceptors();
                    mAwContents.getHeaderInterceptorStore().clearInterceptors();
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testBrowserContextInterceptor_canModifyHeaders() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerBrowserContextInterceptorOnUiThread(
                    request -> hashMapOf("X-BrowserContext-Header", "BrowserContext"),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-BrowserContext-Header"))
                    .isEqualTo("BrowserContext");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testWebContentsInterceptor_canModifyHeaders() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-WebContents-Header", "WebContents"),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-WebContents-Header")).isEqualTo("WebContents");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptors_canBeCombined() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerBrowserContextInterceptorOnUiThread(
                    request -> hashMapOf("X-BrowserContext-Header", "BrowserContext"),
                    server.getResponseUrl("/*"));
            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-WebContents-Header", "WebContents"),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-BrowserContext-Header"))
                    .isEqualTo("BrowserContext");
            assertThat(lastRequest.headerValue("X-WebContents-Header")).isEqualTo("WebContents");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canOverrideHeadersByLoadUrl() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "Second-Value"),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    requestUrl,
                    hashMapOf("X-Contents-Header", "First-Value"));

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Contents-Header")).isEqualTo("Second-Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canDeleteHeadersByLoadUrl() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", null), server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    requestUrl,
                    hashMapOf("X-Contents-Header", "First-Value"));

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Contents-Header")).isEqualTo("");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canSeeHeadersByLoadUrl() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            var tracker = new InterceptorRequestTracker();
            registerContentsInterceptorOnUiThread(
                    request -> {
                        tracker.trackRequest(request);
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    requestUrl,
                    hashMapOf("X-Contents-Header", "First-Value"));

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            assertThat(normalizeHeaders(tracker.getLastRequest().getRequestHeaders()))
                    .containsAtLeast("x-contents-header", "First-Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canOverrideHeadersByNavigate() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "Second-Value"),
                    server.getResponseUrl("/*"));
            navigateSync(
                    new AwNavigationParams(
                            requestUrl,
                            /* shouldReplaceCurrentEntry= */ false,
                            hashMapOf("X-Contents-Header", "First-Value")));

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Contents-Header")).isEqualTo("Second-Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canDeleteHeadersByNavigate() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", null), server.getResponseUrl("/*"));
            navigateSync(
                    new AwNavigationParams(
                            requestUrl,
                            /* shouldReplaceCurrentEntry= */ false,
                            hashMapOf("X-Contents-Header", "First-Value")));

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Contents-Header")).isEqualTo("");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canSeeHeadersByNavigate() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            var tracker = new InterceptorRequestTracker();
            registerContentsInterceptorOnUiThread(
                    request -> {
                        tracker.trackRequest(request);
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));
            navigateSync(
                    new AwNavigationParams(
                            requestUrl,
                            /* shouldReplaceCurrentEntry= */ false,
                            hashMapOf("X-Contents-Header", "First-Value")));

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            assertThat(normalizeHeaders(tracker.getLastRequest().getRequestHeaders()))
                    .containsAtLeast("x-contents-header", "First-Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canOverrideOriginMatchedHeaders() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);
            setOriginMatchedHeaderOnUiThread("X-Contents-Header", "First-Value");

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "Second-Value"),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Contents-Header")).isEqualTo("Second-Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canDeleteOriginMatchedHeaders() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);
            setOriginMatchedHeaderOnUiThread("X-Contents-Header", "First-Value");

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", null), server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Contents-Header")).isEqualTo("");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canSeeOriginMatchedHeaders() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);
            setOriginMatchedHeaderOnUiThread("X-Contents-Header", "First-Value");

            var tracker = new InterceptorRequestTracker();
            registerContentsInterceptorOnUiThread(
                    request -> {
                        tracker.trackRequest(request);
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            assertThat(normalizeHeaders(tracker.getLastRequest().getRequestHeaders()))
                    .containsAtLeast("x-contents-header", "First-Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canOverrideHeadersByPreviousInterceptor() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "First-Value"),
                    server.getResponseUrl("/*"));
            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "Second-Value"),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Contents-Header")).isEqualTo("Second-Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canDeleteHeadersByPreviousInterceptor() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "First-Value"),
                    server.getResponseUrl("/*"));
            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", null), server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Contents-Header")).isEqualTo("");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_canSeeHeadersByPreviousInterceptor() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "First-Value"),
                    server.getResponseUrl("/*"));
            var tracker = new InterceptorRequestTracker();
            registerContentsInterceptorOnUiThread(
                    request -> {
                        tracker.trackRequest(request);
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            assertThat(normalizeHeaders(tracker.getLastRequest().getRequestHeaders()))
                    .containsAtLeast("x-contents-header", "First-Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testWebContentsInterceptor_canOverrideHeadersByBrowserContextInterceptor()
            throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "Second-Value"),
                    server.getResponseUrl("/*"));
            // It shouldn't matter that the BrowserContext interceptor is registered after.
            registerBrowserContextInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "First-Value"),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Contents-Header")).isEqualTo("Second-Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testWebContentsInterceptor_canDeleteHeadersByBrowserContextInterceptor()
            throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", null), server.getResponseUrl("/*"));
            // It shouldn't matter that the BrowserContext interceptor is registered after.
            registerBrowserContextInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "First-Value"),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Contents-Header")).isEqualTo("");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testWebContentsInterceptor_canSeeHeadersByBrowserContextInterceptor()
            throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            var tracker = new InterceptorRequestTracker();
            registerContentsInterceptorOnUiThread(
                    request -> {
                        tracker.trackRequest(request);
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));
            // It shouldn't matter that the BrowserContext interceptor is registered after.
            registerBrowserContextInterceptorOnUiThread(
                    request -> hashMapOf("X-Contents-Header", "First-Value"),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            assertThat(normalizeHeaders(tracker.getLastRequest().getRequestHeaders()))
                    .containsAtLeast("x-contents-header", "First-Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_onlyInterceptsMatchingUrlPatterns() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String matchingUrl =
                    server.setResponse("/matching/test.html", "matching", Collections.emptyList());
            String otherUrl =
                    server.setResponse("/other/test.html", "other", Collections.emptyList());

            AtomicInteger callCount = new AtomicInteger(0);
            registerContentsInterceptorOnUiThread(
                    request -> {
                        callCount.incrementAndGet();
                        return hashMapOf("X-Matched-Header", "matched");
                    },
                    server.getResponseUrl("/matching/*"));
            // Request non-matching URL: interceptor should NOT be called.
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), otherUrl);
            // Request matching URL: interceptor should be called.
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), matchingUrl);

            assertThat(callCount.get()).isEqualTo(1);
            assertThat(server.getLastRequest("/other/test.html").headerValue("X-Matched-Header"))
                    .isEqualTo("");
            assertThat(server.getLastRequest("/matching/test.html").headerValue("X-Matched-Header"))
                    .isEqualTo("matched");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptors_calledInOrder() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);
            List<String> executionOrder = Collections.synchronizedList(new ArrayList<>());

            registerBrowserContextInterceptorOnUiThread(
                    request -> {
                        executionOrder.add("BC1");
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));
            registerBrowserContextInterceptorOnUiThread(
                    request -> {
                        executionOrder.add("BC2");
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));
            registerContentsInterceptorOnUiThread(
                    request -> {
                        executionOrder.add("C1");
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));
            registerContentsInterceptorOnUiThread(
                    request -> {
                        executionOrder.add("C2");
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));
            // BrowserContext interceptors are called before WebContents
            // interceptors even if a BrowserContext interceptor is registered
            // after WebContents interceptors.
            registerBrowserContextInterceptorOnUiThread(
                    request -> {
                        executionOrder.add("BC3");
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(executionOrder).containsExactly("BC1", "BC2", "BC3", "C1", "C2").inOrder();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_interceptsRedirects() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String finishPath = "/finish.html";
            String startPath = "/start.html";
            String finishUrl = server.setResponse(finishPath, "Done", Collections.emptyList());
            String startUrl = server.setRedirect(startPath, finishUrl);

            // Register interceptor specifically for the redirect destination (/finish.html).
            registerBrowserContextInterceptorOnUiThread(
                    request -> hashMapOf("X-Redirect-Hop-Header", "hop-applied"), finishUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), startUrl);

            assertThat(server.getRequestCount(startPath)).isEqualTo(1);
            assertThat(server.getRequestCount(finishPath)).isEqualTo(1);
            assertThat(server.getLastRequest(startPath).headerValue("X-Redirect-Hop-Header"))
                    .isEqualTo("");
            assertThat(server.getLastRequest(finishPath).headerValue("X-Redirect-Hop-Header"))
                    .isEqualTo("hop-applied");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_customHeadersClearedOnRedirects() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String finishPath = "/finish.html";
            String startPath = "/start.html";
            String finishUrl = server.setResponse(finishPath, "Done", Collections.emptyList());
            String startUrl = server.setRedirect(startPath, finishUrl);

            // Interceptor registered ONLY for the start URL.
            registerBrowserContextInterceptorOnUiThread(
                    request -> hashMapOf("X-Start-Only-Header", "secret"), startUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), startUrl);

            assertThat(server.getRequestCount(startPath)).isEqualTo(1);
            assertThat(server.getRequestCount(finishPath)).isEqualTo(1);
            assertThat(server.getLastRequest(startPath).headerValue("X-Start-Only-Header"))
                    .isEqualTo("secret");
            assertThat(server.getLastRequest(finishPath).headerValue("X-Start-Only-Header"))
                    .isEqualTo("");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_doesNotSeeHeadersAddedToPreviousRedirect() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String finishPath = "/finish.html";
            String startPath = "/start.html";
            String finishUrl = server.setResponse(finishPath, "Done", Collections.emptyList());
            String startUrl = server.setRedirect(startPath, finishUrl);

            var tracker = new InterceptorRequestTracker();
            registerContentsInterceptorOnUiThread(
                    request -> {
                        if (request.getUrl().contains(startPath)) {
                            return hashMapOf("X-Redirect-Header", "applied-on-start");
                        } else if (request.getUrl().contains(finishPath)) {
                            tracker.trackRequest(request);
                            return hashMapOf();
                        }
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), startUrl);

            // Headers from prior redirect hop should be cleared before next hop interceptor.
            assertThat(normalizeHeaders(tracker.getLastRequest().getRequestHeaders()))
                    .doesNotContainKey("x-redirect-header");
            assertThat(server.getLastRequest(startPath).headerValue("X-Redirect-Header"))
                    .isEqualTo("applied-on-start");
            assertThat(server.getLastRequest(finishPath).headerValue("X-Redirect-Header"))
                    .isEqualTo("");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_interceptsRestorationRequests() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            // Disable cache for this test so that we can assert on the number of
            // requests made to the server.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> mAwContents.getSettings().setCacheMode(WebSettings.LOAD_NO_CACHE));
            String mainUrl = setMockResponse(server);

            var tracker = new InterceptorRequestTracker();
            registerContentsInterceptorOnUiThread(
                    request -> {
                        tracker.trackRequest(request);
                        return hashMapOf("X-My-Header", "My-Value");
                    },
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), mainUrl);
            Bundle savedState = new Bundle();
            boolean saveSucceeded =
                    ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.saveState(savedState));
            assertThat(saveSucceeded).isTrue();
            int currentCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
            boolean restoreSucceeded =
                    ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.restoreState(savedState));
            assertThat(restoreSucceeded).isTrue();
            mContentsClient.getOnPageFinishedHelper().waitForCallback(currentCallCount);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(2);
            assertThat(server.getLastRequest(REQUEST_PATH).headerValue("X-My-Header"))
                    .isEqualTo("My-Value");
            assertThat(tracker.getAllRequests()).hasSize(2);
            // The restoration request should not include headers from the previous
            // interception.
            assertThat(normalizeHeaders(tracker.getLastRequest().getRequestHeaders()))
                    .doesNotContainKey("x-my-header");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_interceptsSubresources() throws Exception {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getSettings().setImagesEnabled(true));
        try (TestWebServer server = TestWebServer.start()) {
            String imgPath = "/subresource.png";
            String scriptPath = "/subresource.js";
            String imgUrl = server.setEmptyResponse(imgPath);
            String scriptUrl =
                    server.setResponse(scriptPath, "// script content", Collections.emptyList());
            String mainContent =
                    "<!DOCTYPE html><html><body>"
                            + "<img src=\""
                            + imgUrl
                            + "\">"
                            + "<script src=\""
                            + scriptUrl
                            + "\"></script>"
                            + "</body></html>";
            String mainPath = "/main.html";
            String mainUrl = server.setResponse(mainPath, mainContent, Collections.emptyList());

            // Interceptor on AwBrowserContext for image
            registerBrowserContextInterceptorOnUiThread(
                    request -> hashMapOf("X-Img-Header", "img-value"), imgUrl);
            // Interceptor on AwContents for script
            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Script-Header", "script-value"), scriptUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), mainUrl);

            assertThat(server.getRequestCount(imgPath)).isEqualTo(1);
            assertThat(server.getRequestCount(scriptPath)).isEqualTo(1);
            HTTPRequest mainReq = server.getLastRequest(mainPath);
            HTTPRequest imgReq = server.getLastRequest(imgPath);
            HTTPRequest scriptReq = server.getLastRequest(scriptPath);
            assertThat(mainReq.headerValue("X-Img-Header")).isEqualTo("");
            assertThat(mainReq.headerValue("X-Script-Header")).isEqualTo("");
            assertThat(imgReq.headerValue("X-Img-Header")).isEqualTo("img-value");
            assertThat(scriptReq.headerValue("X-Script-Header")).isEqualTo("script-value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_interceptsSpeculationRules() throws Exception {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        try (TestWebServer server = TestWebServer.start()) {
            String prefetchPath = "/prefetch.html";
            String prefetchUrl =
                    server.setResponse(prefetchPath, "prefetch content", Collections.emptyList());
            String mainContent =
                    "<!DOCTYPE html><html><head>"
                            + "<script type=\"speculationrules\">"
                            + "{"
                            + "  \"prefetch\": ["
                            + "    {\"source\": \"list\", \"urls\": [\""
                            + prefetchUrl
                            + "\"]}"
                            + "  ]"
                            + "}"
                            + "</script>"
                            + "</head><body></body></html>";
            String mainPath = "/main.html";
            String mainUrl = server.setResponse(mainPath, mainContent, Collections.emptyList());

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-Prefetch-Header", "prefetch-value"), prefetchUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), mainUrl);

            AwActivityTestRule.pollInstrumentationThread(
                    () -> server.getRequestCount(prefetchPath) == 1);
            HTTPRequest prefetchRequest = server.getLastRequest(prefetchPath);
            assertThat(prefetchRequest.headerValue("X-Prefetch-Header"))
                    .isEqualTo("prefetch-value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testBrowserContextInterceptor_interceptsEmbedderPrefetch() throws Exception {
        AwEmbeddedTestServer sslServer =
                AwEmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_TEST_NAMES);
        try {
            String prefetchUrl = sslServer.getURLWithHostName("a.test", BASIC_PREFETCH_URL);
            AtomicInteger interceptorCalls = new AtomicInteger(0);

            registerBrowserContextInterceptorOnUiThread(
                    request -> {
                        interceptorCalls.incrementAndGet();
                        return hashMapOf("X-BrowserContext-Header", "value-1");
                    },
                    sslServer.getURLWithHostName("a.test", "/*"));
            triggerEmbedderPrefetchAndWait(prefetchUrl);

            assertThat(sslServer.getRequestCountForUrl(BASIC_PREFETCH_URL)).isEqualTo(1);
            HashMap<String, String> headerForUrl =
                    sslServer.getRequestHeadersForUrl(BASIC_PREFETCH_URL);
            assertThat(normalizeHeaders(headerForUrl))
                    .containsEntry("x-browsercontext-header", "value-1");
            assertThat(interceptorCalls.get()).isEqualTo(1);
        } finally {
            sslServer.stopAndDestroyServer();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testWebContentsInterceptor_doesNotInterceptEmbedderPrefetch() throws Exception {
        AwEmbeddedTestServer sslServer =
                AwEmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_TEST_NAMES);
        try {
            String prefetchUrl = sslServer.getURLWithHostName("a.test", BASIC_PREFETCH_URL);
            AtomicInteger interceptorCalls = new AtomicInteger(0);

            registerContentsInterceptorOnUiThread(
                    request -> {
                        interceptorCalls.incrementAndGet();
                        return hashMapOf("X-WebContents-Header", "value-1");
                    },
                    sslServer.getURLWithHostName("a.test", "/*"));
            triggerEmbedderPrefetchAndWait(prefetchUrl);

            assertThat(sslServer.getRequestCountForUrl(BASIC_PREFETCH_URL)).isEqualTo(1);
            HashMap<String, String> headerForUrl =
                    sslServer.getRequestHeadersForUrl(BASIC_PREFETCH_URL);
            assertThat(normalizeHeaders(headerForUrl)).doesNotContainKey("x-webcontents-header");
            assertThat(interceptorCalls.get()).isEqualTo(0);
        } finally {
            sslServer.stopAndDestroyServer();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testWebContentsInterceptor_onlyInterceptsSpecificWebContents() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);
            var contentsClient1 = new TestAwContentsClient();
            var testContainerView1 =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient1);
            var awContents1 = testContainerView1.getAwContents();
            var contentsClient2 = new TestAwContentsClient();
            var testContainerView2 =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient2);
            var awContents2 = testContainerView2.getAwContents();

            // Register only on the second AwContents.
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            awContents2
                                    .getHeaderInterceptorStore()
                                    .addInterceptor(
                                            request -> hashMapOf("X-Header", "Value"),
                                            List.of(server.getResponseUrl("/*"))));

            // Load on the first AwContents.
            mActivityTestRule.loadUrlSync(
                    awContents1, contentsClient1.getOnPageFinishedHelper(), requestUrl);

            // Verify that the header was not added.
            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Header")).isEmpty();

            // Load on the second AwContents.
            mActivityTestRule.loadUrlSync(
                    awContents2, contentsClient2.getOnPageFinishedHelper(), requestUrl);

            // Verify that the header was added.
            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(2);
            lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Header")).isEqualTo("Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testBrowserContextInterceptor_interceptsAllWebContents() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);
            var contentsClient1 = new TestAwContentsClient();
            var testContainerView1 =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient1);
            var awContents1 = testContainerView1.getAwContents();
            var contentsClient2 = new TestAwContentsClient();
            var testContainerView2 =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient2);
            var awContents2 = testContainerView2.getAwContents();

            // Register only on the second AwContents.
            registerBrowserContextInterceptorOnUiThread(
                    request -> hashMapOf("X-Header", "Value"), server.getResponseUrl("/*"));

            // Load on the first AwContents.
            mActivityTestRule.loadUrlSync(
                    awContents1, contentsClient1.getOnPageFinishedHelper(), requestUrl);

            // Verify that the header was not added.
            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Header")).isEqualTo("Value");

            // Load on the second AwContents.
            mActivityTestRule.loadUrlSync(
                    awContents2, contentsClient2.getOnPageFinishedHelper(), requestUrl);

            // Verify that the header was added.
            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(2);
            lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-Header")).isEqualTo("Value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_interceptsCachedResponses() throws Exception {
        // Explicitly set the default caching mode, as otherwise the test could be
        // mutated to disable caching.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getSettings().setCacheMode(WebSettings.LOAD_DEFAULT));
        try (TestWebServer server = TestWebServer.start()) {
            String path = "/cache_test.html";
            List<Pair<String, String>> headers =
                    List.of(Pair.create("Cache-Control", "max-age=3600"));
            String requestUrl = server.setResponse(path, "cache body", headers);

            AtomicInteger interceptorCalls = new AtomicInteger(0);
            registerContentsInterceptorOnUiThread(
                    request -> {
                        interceptorCalls.getAndIncrement();
                        return hashMapOf();
                    },
                    server.getResponseUrl("/*"));

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            // Two interception requests.
            assertThat(interceptorCalls.get()).isEqualTo(2);
            // One server request since the second one should be cached.
            assertThat(server.getRequestCount(path)).isEqualTo(1);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_cachedResponse_doesNotSeeHeadersAddedOnPreviousInterception()
            throws Exception {
        // Explicitly set the default caching mode, as otherwise the test could be
        // mutated to disable caching.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getSettings().setCacheMode(WebSettings.LOAD_DEFAULT));
        try (TestWebServer server = TestWebServer.start()) {
            String path = "/cache_test.html";
            List<Pair<String, String>> headers =
                    List.of(Pair.create("Cache-Control", "max-age=3600"));
            String requestUrl = server.setResponse(path, "cache body", headers);

            var tracker = new InterceptorRequestTracker();
            registerContentsInterceptorOnUiThread(
                    request -> {
                        tracker.trackRequest(request);
                        return hashMapOf("X-Other-Header", "Value");
                    },
                    server.getResponseUrl("/*"));

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(tracker.getAllRequests()).hasSize(2);
            assertThat(normalizeHeaders(tracker.getLastRequest().getRequestHeaders()))
                    .doesNotContainKey("x-other-header");
            // One server request since the second one should be cached.
            assertThat(server.getRequestCount(path)).isEqualTo(1);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_cachedResponse_cacheEligibilityNotBroken_requestServedFromCache()
            throws Exception {
        // Explicitly set the default caching mode, as otherwise the test could be
        // mutated to disable caching.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getSettings().setCacheMode(WebSettings.LOAD_DEFAULT));
        try (TestWebServer server = TestWebServer.start()) {
            String path = "/cache_test.html";
            List<Pair<String, String>> headers =
                    List.of(
                            Pair.create("Cache-Control", "max-age=3600"),
                            Pair.create("Vary", "X-Varied-Header"));
            String requestUrl = server.setResponse(path, "cache body", headers);

            AtomicInteger interceptorCalls = new AtomicInteger(0);
            registerContentsInterceptorOnUiThread(
                    request -> {
                        int call = interceptorCalls.incrementAndGet();
                        return hashMapOf(
                                "X-Varied-Header",
                                "constant-value",
                                "X-Other-Header",
                                "other-value-" + call);
                    },
                    server.getResponseUrl("/*"));

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            HTTPRequest firstRequest = server.getLastRequest(path);
            // Load the URL again. The request is eligible for caching, but the interceptor
            // should still be called. Since X-Varied-Header is unchanged, modifying X-Other-Header
            // does not break cache eligibility, so the request is served from cache.
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(interceptorCalls.get()).isEqualTo(2);
            assertThat(server.getRequestCount(path)).isEqualTo(1);
            assertThat(firstRequest.headerValue("X-Varied-Header")).isEqualTo("constant-value");
            assertThat(firstRequest.headerValue("X-Other-Header")).isEqualTo("other-value-1");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_cachedResponse_cacheEligibilityBroken_requestServedFromNetwork()
            throws Exception {
        // Explicitly set the default caching mode, as otherwise the test could be
        // mutated to disable caching.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getSettings().setCacheMode(WebSettings.LOAD_DEFAULT));
        try (TestWebServer server = TestWebServer.start()) {
            String path = "/cache_test.html";
            List<Pair<String, String>> headers =
                    List.of(
                            Pair.create("Cache-Control", "max-age=3600"),
                            Pair.create("Vary", "X-Varied-Header"));
            String requestUrl = server.setResponse(path, "cache body", headers);

            AtomicInteger interceptorCalls = new AtomicInteger(0);
            registerContentsInterceptorOnUiThread(
                    request -> {
                        int call = interceptorCalls.incrementAndGet();
                        return hashMapOf("X-Varied-Header", "vary-value-" + call);
                    },
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            HTTPRequest firstRequest = server.getLastRequest(path);
            // Load the URL again. The interceptor modifies X-Varied-Header to a new value,
            // breaking cache eligibility. The request must go through the network with the
            // modified header.
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            HTTPRequest lastRequest = server.getLastRequest(path);

            assertThat(interceptorCalls.get()).isEqualTo(2);
            assertThat(server.getRequestCount(path)).isEqualTo(2);
            assertThat(firstRequest.headerValue("X-Varied-Header")).isEqualTo("vary-value-1");
            assertThat(lastRequest.headerValue("X-Varied-Header")).isEqualTo("vary-value-2");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testBrowserContextInterceptor_interceptsServiceWorkerRequests() throws Exception {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestWebMessageListener listener = addTestWebMessageListener();
        try (TestWebServer server = TestWebServer.start()) {
            final String fullIndexUrl = server.setResponse(SW_INDEX_URL, SW_INDEX_HTML, null);
            server.setResponse(SW_URL, NETWORK_ACCESS_SW_JS, null);
            server.setResponse(SW_FETCH_URL, SW_FETCH_CONTENT, null);

            registerBrowserContextInterceptorOnUiThread(
                    request -> hashMapOf("X-My-Header", "value"), server.getResponseUrl("/*"));
            // Load the service worker page and wait until it has finished loading
            mActivityTestRule.loadUrlAsync(mAwContents, fullIndexUrl);
            waitForOnPostMessage(
                    listener, (postMessage) -> SW_FETCH_CONTENT.equals(postMessage.getAsString()));

            assertThat(server.getRequestCount(SW_FETCH_URL)).isEqualTo(1);
            HTTPRequest swFetchRequest = server.getLastRequest(SW_FETCH_URL);
            // Headers added through the BrowserContext should be attached.
            assertThat(swFetchRequest.headerValue("X-My-Header")).isEqualTo("value");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testWebContentsInterceptor_doesNotInterceptServiceWorkerRequests()
            throws Exception {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestWebMessageListener listener = addTestWebMessageListener();
        try (TestWebServer server = TestWebServer.start()) {
            final String fullIndexUrl = server.setResponse(SW_INDEX_URL, SW_INDEX_HTML, null);
            server.setResponse(SW_URL, NETWORK_ACCESS_SW_JS, null);
            server.setResponse(SW_FETCH_URL, SW_FETCH_CONTENT, null);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-My-Header", "value"), server.getResponseUrl("/*"));
            // Load the service worker page and wait until it has finished loading
            mActivityTestRule.loadUrlAsync(mAwContents, fullIndexUrl);
            waitForOnPostMessage(
                    listener, (postMessage) -> SW_FETCH_CONTENT.equals(postMessage.getAsString()));

            assertThat(server.getRequestCount(SW_FETCH_URL)).isEqualTo(1);
            HTTPRequest swFetchRequest = server.getLastRequest(SW_FETCH_URL);
            // Headers added through the AwContents should not be attached.
            assertThat(swFetchRequest.headerValue("X-My-Header")).isEqualTo("");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_cannotOverrideUnsafeHeaders() throws Exception {
        // Enable Javascript so WebView sends Client Hints.
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);
            String newValue = "My-Value";
            mCookieManager.setCookie(requestUrl, "cookie1=test1");

            // Headers from https://fetch.spec.whatwg.org/#forbidden-request-header
            // should not be overridden by header interceptors.
            registerContentsInterceptorOnUiThread(
                    request ->
                            hashMapOf(
                                    "Connection",
                                    newValue,
                                    "Cookie",
                                    "cookie1=new-value",
                                    "Host",
                                    newValue,
                                    "Origin",
                                    newValue,
                                    "Sec-Fetch-Mode",
                                    newValue,
                                    "Sec-CH-UA",
                                    newValue),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("Connection")).isNotEqualTo(newValue);
            assertThat(lastRequest.headerValue("Cookie")).isEqualTo("cookie1=test1");
            assertThat(lastRequest.headerValue("Host")).isNotEqualTo(newValue);
            assertThat(lastRequest.headerValue("Origin")).isNotEqualTo(newValue);
            assertThat(lastRequest.headerValue("Sec-Fetch-Mode")).isNotEqualTo(newValue);
            assertThat(lastRequest.headerValue("Sec-CH-UA")).isNotEqualTo(newValue);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_cannotDeleteUnsafeHeaders() throws Exception {
        // Enable Javascript so WebView sends Client Hints.
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);
            mCookieManager.setCookie(requestUrl, "cookie1=test1");

            // Headers from https://fetch.spec.whatwg.org/#forbidden-request-header
            // should not be deleted by header interceptors.
            registerContentsInterceptorOnUiThread(
                    request ->
                            hashMapOf(
                                    "Connection",
                                    null,
                                    "Cookie",
                                    null,
                                    "Host",
                                    null,
                                    "Sec-Fetch-Mode",
                                    null,
                                    "Sec-CH-UA",
                                    null),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("Connection")).isNotEmpty();
            assertThat(lastRequest.headerValue("Cookie")).isNotEmpty();
            assertThat(lastRequest.headerValue("Host")).isNotEmpty();
            assertThat(lastRequest.headerValue("Sec-Fetch-Mode")).isNotEmpty();
            assertThat(lastRequest.headerValue("Sec-CH-UA")).isNotEmpty();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_ignoresInvalidHeaderNames() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("hello, world", "My-Value"), server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("hello, world")).isEmpty();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testInterceptor_ignoresInvalidHeaderValues() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            registerContentsInterceptorOnUiThread(
                    request -> hashMapOf("X-My-Header", "My-Value\r\n\0"),
                    server.getResponseUrl("/*"));
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);

            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            HTTPRequest lastRequest = server.getLastRequest(REQUEST_PATH);
            assertThat(lastRequest.headerValue("X-My-Header")).isEmpty();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testRemoveInterceptor() throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String requestUrl = setMockResponse(server);

            int id =
                    registerContentsInterceptorOnUiThread(
                            request -> hashMapOf("X-Test-Header", "active"),
                            server.getResponseUrl("/*"));

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(1);
            assertThat(server.getLastRequest(REQUEST_PATH).headerValue("X-Test-Header"))
                    .isEqualTo("active");

            // Remove interceptor
            ThreadUtils.runOnUiThreadBlocking(
                    () -> mAwContents.getHeaderInterceptorStore().removeInterceptor(id));

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), requestUrl);
            assertThat(server.getRequestCount(REQUEST_PATH)).isEqualTo(2);
            assertThat(server.getLastRequest(REQUEST_PATH).headerValue("X-Test-Header"))
                    .isEqualTo("");
        }
    }

    private int registerBrowserContextInterceptorOnUiThread(
            AwHeaderInterceptor interceptor, String urlPattern) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwBrowserContext
                                .getHeaderInterceptorStore()
                                .addInterceptor(interceptor, List.of(urlPattern)));
    }

    private int registerContentsInterceptorOnUiThread(
            AwHeaderInterceptor interceptor, String urlPattern) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents
                                .getHeaderInterceptorStore()
                                .addInterceptor(interceptor, List.of(urlPattern)));
    }

    private static String setMockResponse(TestWebServer server) {
        return server.setResponse(REQUEST_PATH, "Hello, World", Collections.emptyList());
    }

    private void setOriginMatchedHeaderOnUiThread(String headerName, String headerValue) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwBrowserContext.setOriginMatchedHeader(
                                headerName, headerValue, Set.of("*")));
    }

    private TestWebMessageListener addTestWebMessageListener() {
        TestWebMessageListener listener = new TestWebMessageListener();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.addWebMessageListener(
                                "testListener", new String[] {"*"}, listener));
        return listener;
    }

    private void waitForOnPostMessage(
            TestWebMessageListener listener, Predicate<TestWebMessageListener.Data> predicate)
            throws Exception {
        TestWebMessageListener.Data postMessage;
        while ((postMessage = listener.waitForOnPostMessage()) != null) {
            if (predicate.test(postMessage)) {
                break;
            }
        }
    }

    private void navigateSync(AwNavigationParams params) throws Exception {
        int currentCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(params));
        mContentsClient.getOnPageFinishedHelper().waitForCallback(currentCallCount);
    }

    private void triggerEmbedderPrefetchAndWait(String url) throws Exception {
        TestAwPrefetchCallback callback = new TestAwPrefetchCallback();
        int prefetchKey =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mAwBrowserContext
                                        .getPrefetchManager()
                                        .startPrefetchRequest(url, null, callback, Runnable::run));
        callback.setPrefetchKey(prefetchKey);

        callback.getOnStatusUpdatedHelper().waitForNext();
        assertThat(callback.getOnStatusUpdatedHelper().getStatusCode())
                .isEqualTo(AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED);
    }

    private static HashMap<String, String> hashMapOf(String... entries) {
        if ((entries.length % 2) != 0) {
            throw new IllegalArgumentException(
                    "Invalid entries (must be even, a sequence of key-value pairs)");
        }

        HashMap<String, String> result = new HashMap<>();
        for (int i = 0; i < entries.length / 2; i++) {
            String key = entries[i * 2];
            String value = entries[i * 2 + 1];

            if (result.containsKey(key)) {
                throw new IllegalArgumentException("Duplicate key: " + key);
            }

            result.put(key, value);
        }

        return result;
    }

    /**
     * Normalizes header keys to be lower case, to account for transformations inside Chrome's code.
     */
    private static Map<String, String> normalizeHeaders(Map<String, String> headers) {
        var result = new HashMap<String, String>();
        for (Map.Entry<String, String> entry : headers.entrySet()) {
            result.put(entry.getKey().toLowerCase(Locale.US), entry.getValue());
        }
        return result;
    }

    private static class InterceptorRequestTracker {
        private final List<AwWebResourceRequest> mRequests =
                Collections.synchronizedList(new ArrayList<>());

        public void trackRequest(AwWebResourceRequest request) {
            mRequests.add(request);
        }

        public @Nullable AwWebResourceRequest getLastRequest() {
            List<AwWebResourceRequest> requestsCopy = getAllRequests();
            return requestsCopy.size() > 0 ? requestsCopy.get(requestsCopy.size() - 1) : null;
        }

        public List<AwWebResourceRequest> getAllRequests() {
            return new ArrayList<>(mRequests);
        }
    }
}
