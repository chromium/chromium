// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static com.google.common.truth.Truth.assertThat;

import android.annotation.SuppressLint;
import android.util.Pair;
import android.webkit.WebSettings;

import androidx.annotation.NonNull;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

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
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.TestWebMessageListener.Data;
import org.chromium.android_webview.test.util.AwQuotaManagerBridgeTestUtil;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.InstrumentationUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.net.test.util.WebServer.HTTPHeader;
import org.chromium.net.test.util.WebServer.HTTPRequest;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/** Tests for the AwQuotaManagerBridge. */
@Batch(Batch.PER_CLASS)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwQuotaManagerBridgeTest extends AwParameterizedTest {

    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestView;
    private AwContents mAwContents;
    private AwQuotaManagerBridge mAwQuotaManager;
    private TestWebServer mWebServer;
    private String mOrigin;

    public AwQuotaManagerBridgeTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    private static class LongValueCallbackHelper extends CallbackHelper {

        private long mValue;

        public void notifyCalled(long value) {
            mValue = value;
            notifyCalled();
        }

        public long getValue() {
            assertThat(getCallCount()).isGreaterThan(0);
            return mValue;
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestView.getAwContents();
        mWebServer = TestWebServer.start();
        mOrigin = mWebServer.getBaseUrl();

        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        // These tests assume default caching behavior (i.e. respecting cache headers).
        // Setting it explicitly since it gets changed by permutation testing.
        settings.setCacheMode(WebSettings.LOAD_DEFAULT);

        mAwQuotaManager = mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();

        // Clear all data just in case other tests have put something there.
        deleteBrowsingDataSync();
    }

    @After
    public void tearDown() throws TimeoutException {
        deleteBrowsingDataSync();
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    /**
     * Calls deleteBrowsingData on the UI thread and waits for the response callback. Asserts that a
     * value of {@code true} was returned in the callback.
     */
    private void deleteBrowsingDataSync() {
        SettableFuture<Boolean> future = SettableFuture.create();
        ThreadUtils.runOnUiThread(
                () -> {
                    mAwQuotaManager.deleteBrowsingData(future::set);
                });
        Assert.assertTrue(AwActivityTestRule.waitForFuture(future));
    }

    /**
     * Calls deleteBrowsingDataForSite on the UI thread and waits for the response callback. Returns
     * the result of {@link AwQuotaManagerBridge#deleteBrowsingDataForSite(String, Callback)}
     *
     * @param site Domain or url to delete data for.
     * @return Actual site used for deletion.
     */
    private String deleteBrowsingDataForSiteSync(String site) throws Throwable {
        SettableFuture<Boolean> future = SettableFuture.create();
        String deletedSite =
                InstrumentationUtils.runOnMainSyncAndGetResult(
                        InstrumentationRegistry.getInstrumentation(),
                        () -> mAwQuotaManager.deleteBrowsingDataForSite(site, future::set));
        Assert.assertTrue(AwActivityTestRule.waitForFuture(future));
        return deletedSite;
    }

    private long getQuotaForOrigin() throws Exception {
        final LongValueCallbackHelper callbackHelper = new LongValueCallbackHelper();
        int callCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwQuotaManager.getQuotaForOrigin("foo.com", callbackHelper::notifyCalled));
        callbackHelper.waitForCallback(callCount);
        return callbackHelper.getValue();
    }

    private long getUsageForOrigin(final String origin) throws Exception {
        final LongValueCallbackHelper callbackHelper = new LongValueCallbackHelper();
        int callCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwQuotaManager.getUsageForOrigin(origin, callbackHelper::notifyCalled));
        callbackHelper.waitForCallback(callCount);
        return callbackHelper.getValue();
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    */
    @Test
    @DisabledTest(message = "crbug.com/609980")
    public void testDeleteAllFramework() throws Exception {
        final long initialUsage = getUsageForOrigin(mOrigin);

        AwActivityTestRule.pollInstrumentationThread(
                () -> getUsageForOrigin(mOrigin) > initialUsage);

        ThreadUtils.runOnUiThreadBlocking(() -> mAwQuotaManager.deleteAllDataFramework());
        AwActivityTestRule.pollInstrumentationThread(() -> getUsageForOrigin(mOrigin) == 0);
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    */
    @Test
    @DisabledTest(message = "crbug.com/609980")
    public void testDeleteOriginFramework() throws Exception {
        final long initialUsage = getUsageForOrigin(mOrigin);

        AwActivityTestRule.pollInstrumentationThread(
                () -> getUsageForOrigin(mOrigin) > initialUsage);

        ThreadUtils.runOnUiThreadBlocking(() -> mAwQuotaManager.deleteOriginFramework(mOrigin));
        AwActivityTestRule.pollInstrumentationThread(() -> getUsageForOrigin(mOrigin) == 0);
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    */
    @Test
    @DisabledTest(message = "crbug.com/609980")
    public void testGetResultsMatch() throws Exception {
        AwActivityTestRule.pollInstrumentationThread(
                () -> AwQuotaManagerBridgeTestUtil.getOrigins(mAwQuotaManager).mOrigins.length > 0);

        AwQuotaManagerBridge.Origins origins =
                AwQuotaManagerBridgeTestUtil.getOrigins(mAwQuotaManager);
        Assert.assertEquals(origins.mOrigins.length, origins.mUsages.length);
        Assert.assertEquals(origins.mOrigins.length, origins.mQuotas.length);

        for (int i = 0; i < origins.mOrigins.length; ++i) {
            Assert.assertEquals(origins.mUsages[i], getUsageForOrigin(origins.mOrigins[i]));
            Assert.assertEquals(origins.mQuotas[i], getQuotaForOrigin());
        }
    }

    @Test
    @SmallTest
    public void testGetDomainName() {
        Assert.assertEquals(
                "A site should return itself.",
                "example.com",
                AwQuotaManagerBridge.getDomainName("example.com"));
        Assert.assertEquals(
                "An url to a site should return the site",
                "example.com",
                AwQuotaManagerBridge.getDomainName("http://example.com"));
        Assert.assertEquals(
                "An url to a site with a path should return the site",
                "example.com",
                AwQuotaManagerBridge.getDomainName("http://example.com/example"));
        Assert.assertEquals(
                "An url with port and path should return the site",
                "example.com",
                AwQuotaManagerBridge.getDomainName("http://example.com:80/example"));
        Assert.assertEquals(
                "A secure url should return the site",
                "example.com",
                AwQuotaManagerBridge.getDomainName("https://example.com:80/example"));

        Assert.assertEquals(
                "A domain name with subdomain should return the full subdomain",
                "www.example.com",
                AwQuotaManagerBridge.getDomainName("www.example.com"));
        Assert.assertEquals(
                "An url with a subdomain should return the subdomain",
                "www.example.com",
                AwQuotaManagerBridge.getDomainName("https://www.example.com"));

        Assert.assertEquals(
                "Localhost is valid input",
                "localhost",
                AwQuotaManagerBridge.getDomainName("localhost"));
        Assert.assertEquals(
                "IPv4 addresses are valid input",
                "127.0.0.1",
                AwQuotaManagerBridge.getDomainName("127.0.0.1"));

        Assert.assertNull(
                "IBAN URIs are not valid input",
                AwQuotaManagerBridge.getDomainName("iban:123456789"));
    }

    @Test
    @SmallTest
    public void testDeleteBrowsingDataForSiteReturnsSite() throws Throwable {
        Assert.assertEquals(
                "A site domain should be returned as-is",
                "example.com",
                deleteBrowsingDataForSiteSync("example.com"));
        Assert.assertEquals(
                "A subdomain should return just the site",
                "example.com",
                deleteBrowsingDataForSiteSync("www.example.com"));
        Assert.assertEquals(
                "An url with a subdomain should return just the site.",
                "example.com",
                deleteBrowsingDataForSiteSync("https://www.example.com"));
    }

    @Test
    @SmallTest
    public void testDeleteBrowsingDataForSiteThrowsOnInvalidDomain() {
        Assert.assertThrows(
                IllegalArgumentException.class,
                () -> deleteBrowsingDataForSiteSync("not:a:domain"));

        Assert.assertThrows(
                IllegalArgumentException.class, () -> deleteBrowsingDataForSiteSync(""));
    }

    @Test
    @MediumTest
    public void testDeleteBrowsingDataClearsHttpCache() throws Exception {
        String requestUrl =
                mWebServer.setResponse(
                        "/", "hello world", List.of(new Pair<>("Cache-Control", "max-age=604800")));

        // Load the content twice and check that the cache header was respected so the server only
        // saw one request.
        loadUrlSync(requestUrl);
        loadUrlSync(requestUrl);
        Assert.assertEquals(1, mWebServer.getRequestCount("/"));

        deleteBrowsingDataSync();

        // Load again and check that the server saw a new request this time.
        loadUrlSync(requestUrl);
        Assert.assertEquals(2, mWebServer.getRequestCount("/"));
    }

    @Test
    @MediumTest
    public void testDeleteBrowsingDataForSiteClearsHttpCache() throws Throwable {
        try (TestWebServer otherServer = TestWebServer.startAdditional()) {
            otherServer.setServerHost("127.0.0.1");
            List<Pair<String, String>> cacheHeaders =
                    List.of(new Pair<>("Cache-Control", "max-age=604800"));
            String requestUrl = mWebServer.setResponse("/", "hello world", cacheHeaders);
            String otherRequestUrl = otherServer.setResponse("/", "hello world", cacheHeaders);

            Assert.assertNotEquals(
                    new GURL(requestUrl).getHost(), new GURL(otherRequestUrl).getHost());

            // Load our target url into the cache.
            loadUrlSync(requestUrl);
            loadUrlSync(requestUrl);
            Assert.assertEquals(1, mWebServer.getRequestCount("/"));

            // Load a different page into the cache.
            loadUrlSync(otherRequestUrl);
            loadUrlSync(otherRequestUrl);
            Assert.assertEquals(1, otherServer.getRequestCount("/"));

            // Clear and check that the cache is cleared.
            deleteBrowsingDataForSiteSync(requestUrl);

            loadUrlSync(requestUrl);
            Assert.assertEquals(2, mWebServer.getRequestCount("/"));

            // Assert that the other domain was not touched.
            loadUrlSync(otherRequestUrl);
            Assert.assertEquals(1, otherServer.getRequestCount("/"));
        }
    }

    @Test
    @MediumTest
    public void testDeleteBrowsingDataClearsHttpCookie() throws Exception {
        String requestUrl =
                mWebServer.setResponse(
                        "/",
                        "hello world",
                        List.of(
                                new Pair<>(
                                        "Set-Cookie", "durable_cookie=durable; max-age=604800;")));

        loadUrlSync(requestUrl);
        loadUrlSync(requestUrl);
        Set<String> expectedCookies = Set.of("durable_cookie=durable");
        Assert.assertEquals(expectedCookies, getCookies(mWebServer.getLastRequest("/")));

        deleteBrowsingDataSync();

        loadUrlSync(requestUrl);
        Assert.assertEquals(Collections.emptySet(), getCookies(mWebServer.getLastRequest("/")));
    }

    @Test
    @MediumTest
    public void testDeleteBrowsingDataForSiteClearsHttpCookie() throws Throwable {
        try (TestWebServer otherServer = TestWebServer.startAdditional()) {
            otherServer.setServerHost("127.0.0.1");
            List<Pair<String, String>> cookieHeaders =
                    List.of(new Pair<>("Set-Cookie", "durable_cookie=durable; max-age=604800;"));
            String requestUrl = mWebServer.setResponse("/", "hello world", cookieHeaders);
            String otherRequestUrl = otherServer.setResponse("/", "hello world", cookieHeaders);

            Assert.assertNotEquals(requestUrl, otherRequestUrl);

            Set<String> expectedCookies = Set.of("durable_cookie=durable");

            // Load our target url into the cache.
            loadUrlSync(requestUrl);
            loadUrlSync(requestUrl);
            Assert.assertEquals(expectedCookies, getCookies(mWebServer.getLastRequest("/")));

            // Load a different page into the cache.
            loadUrlSync(otherRequestUrl);
            loadUrlSync(otherRequestUrl);
            Assert.assertEquals(expectedCookies, getCookies(otherServer.getLastRequest("/")));

            // Clear and check that the cookies are cleared.
            deleteBrowsingDataForSiteSync(requestUrl);

            loadUrlSync(requestUrl);
            Assert.assertEquals(Collections.emptySet(), getCookies(mWebServer.getLastRequest("/")));

            // Assert that the other domain was not touched.
            loadUrlSync(otherRequestUrl);
            Assert.assertEquals(expectedCookies, getCookies(otherServer.getLastRequest("/")));
        }
    }

    @Test
    @MediumTest
    public void testDeleteBrowsingDataClearsServiceWorker() throws Exception {
        String mainHtml =
                """
            <!DOCTYPE html>
            <script>

            function swReady(sw) {
              testListener.postMessage("loaded");
            }

            navigator.serviceWorker.register('/sw.js')
            .then(sw_reg => {
                let sw = sw_reg.installing || sw_reg.waiting || sw_reg.active;
                if (sw.state == 'activated') {
                    swReady(sw);
                } else {
                    sw.addEventListener('statechange', e => {
                        if (e.target.state == 'activated') swReady(e.target);
                    });
                }
            })
            .catch(err => testListener.postMessage("" + err));

            </script>
            """;
        String serviceWorker =
                """
            self.addEventListener("install", event => {
              event.waitUntil(fetch("/done"));
            });
            """;

        TestWebMessageListener testListener = new TestWebMessageListener();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.addWebMessageListener(
                                "testListener", new String[] {"*"}, testListener));

        String mainUrl =
                mWebServer.setResponse(
                        "/", mainHtml, List.of(new Pair<>("Content-Type", "text/html")));
        mWebServer.setResponse(
                "/sw.js",
                serviceWorker,
                List.of(new Pair<>("Content-Type", "application/javascript")));
        mWebServer.setResponse("/done", "done", Collections.emptyList());

        {
            // Initial load. We expect the service worker installer to run and fetch "/done".
            loadUrlSync(mainUrl);
            // wait for the service worker to be ready.
            Data data = testListener.waitForOnPostMessage();
            Assert.assertEquals("loaded", data.getAsString());
            Assert.assertEquals(1, mWebServer.getRequestCount("/done"));
        }
        {
            // Second load. We do not expect the request count for "/done" to increase, as the
            // service worker should already be installed
            loadUrlSync(mainUrl);
            // wait for the service worker to be ready.
            Data data = testListener.waitForOnPostMessage();
            Assert.assertEquals("loaded", data.getAsString());
            Assert.assertEquals(1, mWebServer.getRequestCount("/done"));
        }

        deleteBrowsingDataSync();

        {
            // Third load. We expect the install event to be fired again and the request count to
            // increase.
            loadUrlSync(mainUrl);
            // wait for the service worker to be ready.
            Data data = testListener.waitForOnPostMessage();
            Assert.assertEquals("loaded", data.getAsString());
            Assert.assertEquals(2, mWebServer.getRequestCount("/done"));
        }
    }

    @NonNull
    private static Set<String> getCookies(HTTPRequest request) {
        HTTPHeader[] headers = request.getHeaders();
        Set<String> cookies = new HashSet<>();
        for (HTTPHeader header : headers) {
            if (header.key.equals("Cookie")) {
                String[] values = header.value.split(";\\s*");
                cookies.addAll(Arrays.asList(values));
            }
        }
        return cookies;
    }

    private void loadUrlSync(String otherRequestUrl) throws Exception {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), otherRequestUrl);
    }
}
