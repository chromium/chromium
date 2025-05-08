// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;


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
import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.io.ByteArrayInputStream;
import java.nio.charset.Charset;
import java.util.List;

/** Tests Service Worker Client related APIs. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwServiceWorkerClientTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private AwTestContainerView mTestContainerView;
    private TestAwServiceWorkerClient mServiceWorkerClient;

    private static final String INDEX_HTML =
            """
        <!DOCTYPE html>
        <html>
          <body>
            <script>
              console.log("Registering serviceworker");
              success = 0;
              navigator.serviceWorker.register('sw.js').then(function(reg) {
                 success = 1;
              }).catch(function(err) {
                 console.error(err);
              });
            </script>
          </body>
        </html>
        """;

    private static final String SW_HTML =
            """
        console.log("Running in serviceworker");
        fetch('fetch.html');
        """;
    private static final String FETCH_HTML = ";)";

    public AwServiceWorkerClientTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mServiceWorkerClient = new TestAwServiceWorkerClient();
        mActivityTestRule
                .getAwBrowserContext()
                .getServiceWorkerController()
                .setServiceWorkerClient(mServiceWorkerClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    @After
    public void tearDown() {
        if (mWebServer != null) mWebServer.shutdown();
    }

    @Test
    @SmallTest
    public void testInvokeInterceptCallback() throws Throwable {
        final String fullIndexUrl = mWebServer.setResponse("/index.html", INDEX_HTML, null);
        final String fullSwUrl = mWebServer.setResponse("/sw.js", SW_HTML, null);
        final String fullFetchUrl = mWebServer.setResponse("/fetch.html", FETCH_HTML, null);

        TestAwServiceWorkerClient.ShouldInterceptRequestHelper helper =
                mServiceWorkerClient.getShouldInterceptRequestHelper();

        loadPage(fullIndexUrl, helper, 2, /* nullIntercept= */ true);
        // Check that the two service worker related callbacks were correctly intercepted.
        List<AwWebResourceRequest> requests = helper.getAwWebResourceRequests();
        Assert.assertEquals(2, requests.size());
        Assert.assertEquals(fullSwUrl, requests.get(0).getUrl());
        Assert.assertEquals(fullFetchUrl, requests.get(1).getUrl());
    }

    // Verify that WebView ServiceWorker code can properly handle http errors that happened
    // in ServiceWorker fetches.
    @Test
    @SmallTest
    public void testFetchHttpError() throws Throwable {
        final String fullIndexUrl = mWebServer.setResponse("/index.html", INDEX_HTML, null);
        final String fullSwUrl = mWebServer.setResponse("/sw.js", SW_HTML, null);
        mWebServer.setResponseWithNotFoundStatus("/fetch.html");

        TestAwServiceWorkerClient.ShouldInterceptRequestHelper helper =
                mServiceWorkerClient.getShouldInterceptRequestHelper();

        loadPage(fullIndexUrl, helper, 1, /* nullIntercept= */ true);
        // Check that the two service worker related callbacks were correctly intercepted.
        List<AwWebResourceRequest> requests = helper.getAwWebResourceRequests();
        Assert.assertEquals(2, requests.size());
        Assert.assertEquals(fullSwUrl, requests.get(0).getUrl());
    }

    // Verify that WebView ServiceWorker code can properly handle resource loading errors
    // that happened in ServiceWorker fetches.
    @Test
    @SmallTest
    public void testFetchResourceLoadingError() throws Throwable {
        final String fullIndexUrl = mWebServer.setResponse("/index.html", INDEX_HTML, null);
        final String fullSwUrl =
                mWebServer.setResponse("/sw.js", "fetch('https://google.gov');", null);

        TestAwServiceWorkerClient.ShouldInterceptRequestHelper helper =
                mServiceWorkerClient.getShouldInterceptRequestHelper();
        loadPage(fullIndexUrl, helper, 1, /* nullIntercept= */ true);
        // Check that the two service worker related callbacks were correctly intercepted.
        List<AwWebResourceRequest> requests = helper.getAwWebResourceRequests();
        Assert.assertEquals(2, requests.size());
        Assert.assertEquals(fullSwUrl, requests.get(0).getUrl());
    }

    @Test
    @SmallTest
    public void testWithNonNullResponse() throws Throwable {
        final String fullIndexUrl = mWebServer.setResponse("/index.html", INDEX_HTML, null);
        final String fullSwUrl =
                mWebServer.setResponse("/sw.js", "fetch('https://google.gov');", null);
        mServiceWorkerClient =
                new TestAwServiceWorkerClient() {
                    @Override
                    public WebResourceResponseInfo shouldInterceptRequest(
                            AwWebResourceRequest request) {
                        super.shouldInterceptRequest(request);
                        return new WebResourceResponseInfo(
                                "text/plaintext",
                                "utf-8",
                                new ByteArrayInputStream(
                                        "SUCCESS".getBytes(Charset.defaultCharset())));
                    }
                };
        mActivityTestRule
                .getAwBrowserContext()
                .getServiceWorkerController()
                .setServiceWorkerClient(mServiceWorkerClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        TestAwServiceWorkerClient.ShouldInterceptRequestHelper helper =
                mServiceWorkerClient.getShouldInterceptRequestHelper();
        loadPage(fullIndexUrl, helper, 1, /* nullIntercept= */ false);
        // Check that the second service worker related callback is skipped due to
        // overriding top level response to a non-null value.
        List<AwWebResourceRequest> requests = helper.getAwWebResourceRequests();
        Assert.assertEquals(1, requests.size());
        Assert.assertEquals(fullSwUrl, requests.get(0).getUrl());
    }

    private void loadPage(
            final String fullIndexUrl,
            TestAwServiceWorkerClient.ShouldInterceptRequestHelper helper,
            int expectedInterceptRequestCount,
            boolean nullIntercept)
            throws Exception {
        int currentShouldInterceptRequestCount = helper.getCallCount();

        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, fullIndexUrl);
        Assert.assertEquals(fullIndexUrl, onPageFinishedHelper.getUrl());

        if (nullIntercept) {
            // Check that the service worker has been registered successfully.
            AwActivityTestRule.pollInstrumentationThread(() -> getSuccessFromJs() == 1);
        }

        helper.waitForCallback(currentShouldInterceptRequestCount, expectedInterceptRequestCount);
    }

    private int getSuccessFromJs() {
        int result = -1;
        try {
            result =
                    Integer.parseInt(
                            mActivityTestRule.executeJavaScriptAndWaitForResult(
                                    mAwContents, mContentsClient, "success"));
        } catch (Exception e) {
            throw new AssertionError("Unable to get success", e);
        }
        return result;
    }
}
