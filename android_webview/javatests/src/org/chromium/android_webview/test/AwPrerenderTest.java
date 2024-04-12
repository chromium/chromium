// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;

import android.net.Uri;
import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;

import com.google.common.util.concurrent.SettableFuture;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.TestAwContentsClient.OnLoadResourceHelper;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;

import java.io.FileInputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.Arrays;
import java.util.HashMap;
import java.util.concurrent.TimeUnit;

@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
public class AwPrerenderTest extends AwParameterizedTest {
    private static final String TAG = "AwPrerenderTest";

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();
    @Rule public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule public AwActivityTestRule mActivityTestRule;

    public AwPrerenderTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private static final String INITIAL_URL = "/android_webview/test/data/hello_world.html";
    private static final String PRERENDER_URL = "/android_webview/test/data/prerender.html";
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private AwEmbeddedTestServer mTestServer;
    private String mPageUrl;
    private String mPrerenderingUrl;

    private SettableFuture<Boolean> mActivationFuture;
    private SettableFuture<Boolean> mPostMessageFuture;

    private TestWebMessageListener mWebMessageListener;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startBrowserProcess();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mWebMessageListener = new TestWebMessageListener();

        // This future is used for waiting until the JS prerenderingchange event is fired on the
        // prerendered page. See //android_webview/test/data/prerender.html.
        mActivationFuture = SettableFuture.create();
        String name = "awActivationFuture";
        Object injectedObject =
                new Object() {
                    @JavascriptInterface
                    public void activated() {
                        mActivationFuture.set(true);
                    }
                };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(mAwContents, injectedObject, name);

        // This future is used for waiting until the prerendered page posts a message to Java.
        mPostMessageFuture = SettableFuture.create();
        Object injectedObjectForPostMessage =
                new Object() {
                    @JavascriptInterface
                    public void done() {
                        mPostMessageFuture.set(true);
                    }
                };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                mAwContents, injectedObjectForPostMessage, "awPostMessageFuture");

        mTestServer =
                AwEmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());

        mPageUrl = mTestServer.getURL(INITIAL_URL);
        mPrerenderingUrl = mTestServer.getURL(PRERENDER_URL);

        // Load an initial page that will be triggering speculation rules prerendering.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mPageUrl);

        // Wait for onPageStarted for the initial page load.
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        onPageStartedHelper.waitForCallback(0, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    // Injects speculation rules for `url`.
    private void injectSpeculationRules(String url) throws Exception {
        final String speculationRulesTemplate =
                """
                    {
                    const script = document.createElement('script');
                    script.type = 'speculationrules';
                    script.text = '{"prerender": [{"source": "list", "urls": ["%s"]}]}';
                    document.head.appendChild(script);
                    }
                """;
        final String speculationRules = String.format(speculationRulesTemplate, url);

        // Start prerendering from the initial page.
        mActivityTestRule.runOnUiThread(
                () -> {
                    mAwContents.evaluateJavaScript(speculationRules, null);
                });
    }

    // Injects speculation rules for `url` and then waits until a prerendering navigation request is
    // sent.
    private void injectSpeculationRulesAndWait(String url) throws Exception {
        final OnLoadResourceHelper onLoadResourceHelper = mContentsClient.getOnLoadResourceHelper();
        int currentOnLoadResourceCallCount = onLoadResourceHelper.getCallCount();

        injectSpeculationRules(url);

        // Wait for prerendering navigation. Monitor onLoadResource instead of onPageFinished as
        // onPageFinished is never called during prerendering (deferred until activation).
        onLoadResourceHelper.waitForCallback(
                currentOnLoadResourceCallCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onLoadResourceHelper.getLastLoadedResource(), url);
    }

    // Activate a prerendered page by navigating to `activateUrl`. `expectedActivatedUrl` indicates
    // a URL that should actually be activated. Generally, `expectedActivatedUrl` is the same as
    // `activateUrl`, but they are different when prerendering navigation is redirected.
    private void activatePage(String activateUrl, String expectedActivatedUrl) throws Exception {
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        int currentOnPageStartedCallCount = onPageStartedHelper.getCallCount();

        // Activate the prerendered page.
        mActivityTestRule.runOnUiThread(
                () -> {
                    final String activationScript =
                            String.format("location.href = `%s`;", activateUrl);
                    mAwContents.evaluateJavaScript(activationScript, null);
                });

        // Wait until the page is activated.
        onPageStartedHelper.waitForCallback(
                currentOnPageStartedCallCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onPageStartedHelper.getUrl(), expectedActivatedUrl);

        // Make sure the page was actually prerendered and then activated.
        Assert.assertEquals(
                true, mActivationFuture.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(
                "true",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "wasPrerendered"));
    }

    // Shorthand notation of `activatePage(activate_url, activate_url)`.
    private void activatePage(String activateUrl) throws Exception {
        activatePage(activateUrl, activateUrl);
    }

    private final String encodeUrl(String url) {
        try {
            return URLEncoder.encode(url, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            throw new AssertionError(e);
        }
    }

    private static void assertUrlHasOrigin(final String url, final Uri origin) {
        Assert.assertEquals("The origin URI must not contain a path", "", origin.getPath());
        Assert.assertEquals("The origin URI must not contain any queries", null, origin.getQuery());
        Assert.assertEquals(
                "The origin URI must not contain a fragment", null, origin.getFragment());

        Uri uriFromServer = Uri.parse(url);
        Assert.assertEquals(uriFromServer.getScheme(), origin.getScheme());
        Assert.assertEquals(uriFromServer.getHost(), origin.getHost());
        Assert.assertEquals(uriFromServer.getPort(), origin.getPort());
    }

    // Tests basic end-to-end behavior of speculation rules prerendering on WebView.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testSpeculationRulesPrerendering() throws Throwable {
        injectSpeculationRulesAndWait(mPrerenderingUrl);

        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        // onPageStarted should never be called for prerender initial navigation.
        Assert.assertEquals(onPageStartedHelper.getCallCount(), 1);
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);

        activatePage(mPrerenderingUrl);
    }

    // Tests FrameTree swap of AwContentsIoThreadClient by observing that callbacks are correctly
    // called after prerender activation.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testAwContentsIoThreadClientHandleFrameTreeSwapForward() throws Throwable {
        String url1 = mTestServer.getURL(INITIAL_URL.concat("?q=1"));
        String url2 = mTestServer.getURL(PRERENDER_URL);
        String url3 = mTestServer.getURL(INITIAL_URL.concat("?q=3"));

        final TestAwContentsClient.ShouldInterceptRequestHelper helper =
                mContentsClient.getShouldInterceptRequestHelper();
        int callCount = 0;

        helper.clearUrls();
        callCount = helper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url1);
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(url1));

        helper.clearUrls();
        callCount = helper.getCallCount();
        injectSpeculationRulesAndWait(url2);
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(url2));

        callCount = helper.getCallCount();
        // Prerender activation will trigger a FrameTree swap and a RenderFrameHostChanged call.
        activatePage(url2);
        Assert.assertEquals(
                "Prerender activation navigation doesn't trigger shouldInterceptRequest",
                helper.getCallCount(),
                callCount);

        // An IO thread associated with the previously-prerendered page (the same IO thread used by
        // the first page) should
        // receive callbacks. This checks handling of the FrameTree swap.
        helper.clearUrls();
        callCount = helper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url3);
        // If the FrameTree swap wasn't handled correctly, the shouldInterceptRequest callback for
        // this navigation wont get routed to this thread.
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(url3));
    }

    // Tests RenderFrameHostChanged without FrameTree swap of AwContentsIoThreadClient by observing
    // that callbacks are correctly called.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testAwContentsIoThreadClientHandleFrameTreeSwapBack() throws Throwable {
        String url1 = mTestServer.getURL(INITIAL_URL.concat("?q=1"));
        String url2 = mTestServer.getURL(PRERENDER_URL);
        String url4 = mTestServer.getURL(INITIAL_URL.concat("?q=4"));

        final TestAwContentsClient.ShouldInterceptRequestHelper helper =
                mContentsClient.getShouldInterceptRequestHelper();
        int callCount = 0;

        helper.clearUrls();
        callCount = helper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url1);
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(url1));

        helper.clearUrls();
        callCount = helper.getCallCount();
        injectSpeculationRulesAndWait(url2);
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(url2));

        callCount = helper.getCallCount();
        // Prerender activation will trigger a FrameTree swap and a RenderFrameHostChanged call.
        activatePage(url2);
        Assert.assertEquals(
                "Prerender activation navigation doesn't trigger shouldInterceptRequest",
                helper.getCallCount(),
                callCount);

        helper.clearUrls();
        callCount = helper.getCallCount();
        // RenderFrameHostChanged without FrameTree swap occurs here.
        mActivityTestRule.runOnUiThread(
                () -> {
                    mAwContents.evaluateJavaScript("history.back();", null);
                });
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(url1));

        // An IO thread associated with the third page (the same IO thread used by the first page)
        // should
        // receive callbacks. This checks handling of the RenderFrameHostChanged without FrameTree
        // swap.
        helper.clearUrls();
        callCount = helper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url4);
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(url4));
    }

    // Tests ShouldInterceptRequest interaction with prerendering.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingAndShouldInterceptRequest() throws Throwable {
        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        int currentShouldInterceptRequestCallCount = shouldInterceptRequestHelper.getCallCount();

        injectSpeculationRulesAndWait(mPrerenderingUrl);

        shouldInterceptRequestHelper.waitForCallback(currentShouldInterceptRequestCallCount);
        AwContentsClient.AwWebResourceRequest request =
                shouldInterceptRequestHelper.getRequestsForUrl(mPrerenderingUrl);
        Assert.assertNotNull(request);
        HashMap<String, String> requestHeaders = request.requestHeaders;
        Assert.assertNotNull(requestHeaders);
        Assert.assertEquals(requestHeaders.get("Sec-Purpose"), "prefetch;prerender");

        currentShouldInterceptRequestCallCount = shouldInterceptRequestHelper.getCallCount();
        activatePage(mPrerenderingUrl);
        Assert.assertEquals(
                "Prerender activation navigation doesn't trigger shouldInterceptRequest",
                shouldInterceptRequestHelper.getCallCount(),
                currentShouldInterceptRequestCallCount);
    }

    // Tests prerendering can succeed with a custom response served by ShouldInterceptRequest.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingWithCustomResponse() throws Throwable {
        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
                mContentsClient.getShouldInterceptRequestHelper();

        // This test will attempt to prerender a non-existent URL. Generally this should fail, but
        // in this test shouldInterceptRequestHelper will serve a custom response instead.
        final String nonExistentUrl = mTestServer.getURL("/non_existent.html");

        // Construct a custom response.
        FileInputStream body = new FileInputStream(UrlUtils.getIsolatedTestFilePath(PRERENDER_URL));
        WebResourceResponseInfo response = new WebResourceResponseInfo("text/html", "utf-8", body);
        shouldInterceptRequestHelper.setReturnValueForUrl(nonExistentUrl, response);

        int currentShouldInterceptRequestCallCount = shouldInterceptRequestHelper.getCallCount();

        // This doesn't wait for prerendering navigation as the waiting logic is implemented on top
        // of onLoadResource that is never called when a custom response is served.
        injectSpeculationRules(nonExistentUrl);

        // Ensure that ShouldInterceptRequest is called.
        shouldInterceptRequestHelper.waitForCallback(currentShouldInterceptRequestCallCount);
        AwContentsClient.AwWebResourceRequest request =
                shouldInterceptRequestHelper.getRequestsForUrl(nonExistentUrl);
        Assert.assertNotNull(request);

        // Activation with the non-existent URL should succeed.
        activatePage(nonExistentUrl);
    }

    // Tests ShouldOverrideUrlLoading interaction with prerendering.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingAndShouldOverrideUrlLoading() throws Throwable {
        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                mContentsClient.getShouldOverrideUrlLoadingHelper();
        int currentShouldOverrideUrlLoadingCallCount =
                shouldOverrideUrlLoadingHelper.getCallCount();

        injectSpeculationRulesAndWait(mPrerenderingUrl);

        shouldOverrideUrlLoadingHelper.waitForCallback(currentShouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(), mPrerenderingUrl);
        HashMap<String, String> requestHeadersOnShouldOverride =
                shouldOverrideUrlLoadingHelper.requestHeaders();
        Assert.assertNotNull(requestHeadersOnShouldOverride);
        Assert.assertEquals(
                requestHeadersOnShouldOverride.get("Sec-Purpose"), "prefetch;prerender");

        currentShouldOverrideUrlLoadingCallCount = shouldOverrideUrlLoadingHelper.getCallCount();
        activatePage(mPrerenderingUrl);
        shouldOverrideUrlLoadingHelper.waitForCallback(currentShouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(), mPrerenderingUrl);
        Assert.assertNull(
                "activation naivgation should have null requestHeaders.",
                shouldOverrideUrlLoadingHelper.requestHeaders());
    }

    // Tests ShouldOverrideUrlLoading interaction with prerendering that is redirected.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testRedirectedPrerenderingAndShouldOverrideUrlLoading() throws Throwable {
        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                mContentsClient.getShouldOverrideUrlLoadingHelper();
        int currentShouldOverrideUrlLoadingCallCount =
                shouldOverrideUrlLoadingHelper.getCallCount();

        // Construct an initial prerendering URL that is redirected to `mPrerenderingUrl`.
        final String initialPrerenderingUrl =
                mTestServer.getURL(
                        "/server-redirect-echoheader?url=" + encodeUrl(mPrerenderingUrl));

        injectSpeculationRules(initialPrerenderingUrl);

        // Check if the initial prerendering navigation is visible to shouldOverrideUrlLoading.
        shouldOverrideUrlLoadingHelper.waitForCallback(currentShouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(),
                initialPrerenderingUrl);
        Assert.assertFalse(shouldOverrideUrlLoadingHelper.isRedirect());
        HashMap<String, String> requestHeadersOnShouldOverride =
                shouldOverrideUrlLoadingHelper.requestHeaders();
        Assert.assertNotNull(requestHeadersOnShouldOverride);
        Assert.assertEquals(
                requestHeadersOnShouldOverride.get("Sec-Purpose"), "prefetch;prerender");

        // Check if the redirected prerendering navigation is also visible to
        // shouldOverrideUrlLoading.
        currentShouldOverrideUrlLoadingCallCount = shouldOverrideUrlLoadingHelper.getCallCount();
        shouldOverrideUrlLoadingHelper.waitForCallback(currentShouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(), mPrerenderingUrl);
        Assert.assertTrue(shouldOverrideUrlLoadingHelper.isRedirect());
        requestHeadersOnShouldOverride = shouldOverrideUrlLoadingHelper.requestHeaders();
        Assert.assertNotNull(requestHeadersOnShouldOverride);
        Assert.assertEquals(
                requestHeadersOnShouldOverride.get("Sec-Purpose"), "prefetch;prerender");

        currentShouldOverrideUrlLoadingCallCount = shouldOverrideUrlLoadingHelper.getCallCount();

        activatePage(initialPrerenderingUrl, mPrerenderingUrl);

        // Activation navigation should also be visible to shouldOverrideUrlLoading.
        shouldOverrideUrlLoadingHelper.waitForCallback(currentShouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(),
                initialPrerenderingUrl);
        Assert.assertNull(
                "activation naivgation should have null requestHeaders.",
                shouldOverrideUrlLoadingHelper.requestHeaders());
    }

    // Tests postMessage() from JS to Java during prerendering are deferred until activation.
    // TODO(crbug.com/41490450): Test postMessage() from iframes.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPostMessageDuringPrerendering() throws Throwable {
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "awMessagePort", new String[] {"*"}, mWebMessageListener);

        injectSpeculationRules(mPrerenderingUrl);

        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        // onPageStarted should never be called for prerender initial navigation.
        Assert.assertEquals(onPageStartedHelper.getCallCount(), 1);
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);

        // This future is notified after a message is posted. However, messages posted by
        // prerendered pages are deferred until prerender activation, so
        // `WebMessageListener.onPostMessage` would not be called yet.
        //
        // Note that these checks are not ideal because there is no strict message ordering
        // guarantee between the future and the posted message. For example, the message could be
        // delivered after the future is done but before activation happens. It would be great if we
        // could have a mechanism to make sure the deferral logic in a deterministic way.
        Assert.assertEquals(
                true, mPostMessageFuture.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertTrue(mWebMessageListener.hasNoMoreOnPostMessage());

        activatePage(mPrerenderingUrl);

        // The page is activated. Now the deferred messages should be delivered.
        TestWebMessageListener.Data data = mWebMessageListener.waitForOnPostMessage();

        assertUrlHasOrigin(mPrerenderingUrl, data.mTopLevelOrigin);
        assertUrlHasOrigin(mPrerenderingUrl, data.mSourceOrigin);
        Assert.assertEquals("Prerendered", data.getAsString());
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mWebMessageListener.hasNoMoreOnPostMessage());
    }
}
