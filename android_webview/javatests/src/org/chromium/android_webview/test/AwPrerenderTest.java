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

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwFeatureMap;
import org.chromium.android_webview.ScriptHandler;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.settings.SpeculativeLoadingAllowedFlags;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
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
    private static enum ActivationBy {
        LOAD_URL,
        JAVASCRIPT,
    };

    private static final String TAG = "AwPrerenderTest";

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Rule public AwActivityTestRule mActivityTestRule;

    public AwPrerenderTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private static final String INITIAL_URL = "/android_webview/test/data/hello_world.html";
    private static final String PRERENDER_URL = "/android_webview/test/data/prerender.html";
    private static final String PRERENDER_SETUP_SCRIPT_URL =
            "/android_webview/test/data/prerender-test-setup.js";
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private AwEmbeddedTestServer mTestServer;
    private String mPageUrl;
    private String mPrerenderingUrl;

    private SettableFuture<Boolean> mActivationFuture;
    private SettableFuture<Boolean> mPostMessageFuture;

    private TestWebMessageListener mDeferredWebMessageListener;
    private TestWebMessageListener mPrerenderLifecycleWebMessageListener;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startBrowserProcess();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        // Enable localStorage that is used as communication channel between the primary page and
        // prerendered pages. See `channelScript` below for details.
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setDomStorageEnabled(true);

        // This message listener is used for making sure messages posted by prerendered pages are
        // deferred until prerender activation.
        mDeferredWebMessageListener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents,
                "awDeferredMessagePort",
                new String[] {"*"},
                mDeferredWebMessageListener);

        // This message listener is used for notifying Java of lifecycle events on prerendered
        // pages.
        mPrerenderLifecycleWebMessageListener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents,
                "awPrerenderLifecycleMessagePort",
                new String[] {"*"},
                mPrerenderLifecycleWebMessageListener);

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
    }

    public void setSpeculativeLoadingAllowed(@SpeculativeLoadingAllowedFlags int allowed) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getSettings().setSpeculativeLoadingAllowed(allowed));
    }

    public void loadInitialPage() throws Exception {
        // Load an initial page that will be triggering speculation rules prerendering.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mPageUrl);

        // Wait for onPageStarted for the initial page load.
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        onPageStartedHelper.waitForCallback(0, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);

        // Set up the communication channel between the primary page (initial page) and prerendered
        // pages. This script waits until a prerendered page notifies the primary page of lifecycle
        // events via `window.localStorage`. Then, the primary page forwards the notification to
        // Java via `mPrerenderLifecycleWebMessageListener`.
        final String channelScript =
                """
                    {
                      window.localStorage.clear();
                      window.addEventListener("storage", event => {
                        if (event.key === "pageStarted") {
                          awPrerenderLifecycleMessagePort.postMessage(event.newValue);
                        }
                      });
                    }
                """;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.evaluateJavaScript(channelScript, null);
                });
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.evaluateJavaScript(speculationRules, null);
                });
    }

    // Injects speculation rules for `url` and then waits until a prerendered page starts running
    // JavaScript.
    private void injectSpeculationRulesAndWait(String url) throws Exception {
        // Start prerendering.
        injectSpeculationRules(url);

        // Wait until the prerendered page starts running JavaScript.
        TestWebMessageListener.Data data =
                mPrerenderLifecycleWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals(url, data.getAsString());
    }

    // Navigates the primary page to `url` by client side redirection.
    private void navigatePage(String url) throws Exception {
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        int currentOnPageStartedCallCount = onPageStartedHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final String navigationScript = String.format("location.href = `%s`;", url);
                    mAwContents.evaluateJavaScript(navigationScript, null);
                });
        onPageStartedHelper.waitForCallback(
                currentOnPageStartedCallCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onPageStartedHelper.getUrl(), url);
    }

    // Activates a prerendered page by navigating to `activateUrl`. `expectedActivatedUrl` indicates
    // a URL that should actually be activated. Generally, `expectedActivatedUrl` is the same as
    // `activateUrl`, but they are different when prerendering navigation is redirected.
    private void activatePage(
            String activateUrl, String expectedActivatedUrl, ActivationBy activationBy)
            throws Exception {
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        int currentOnPageStartedCallCount = onPageStartedHelper.getCallCount();

        // Activate the prerendered page.
        switch (activationBy) {
            case LOAD_URL:
                mActivityTestRule.loadUrlAsync(mAwContents, activateUrl);
                break;
            case JAVASCRIPT:
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            final String activationScript =
                                    String.format("location.href = `%s`;", activateUrl);
                            mAwContents.evaluateJavaScript(activationScript, null);
                        });
                break;
        }

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
    private void activatePage(String activateUrl, ActivationBy activationBy) throws Exception {
        activatePage(activateUrl, activateUrl, activationBy);
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

    // Tests basic end-to-end behavior of speculation rules prerendering on WebView with
    // renderer-initiated activation.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testSpeculationRulesPrerenderingRendererInitiatedActivation() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        injectSpeculationRulesAndWait(mPrerenderingUrl);

        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        // onPageStarted should never be called for prerender initial navigation.
        Assert.assertEquals(onPageStartedHelper.getCallCount(), 1);
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);

        activatePage(mPrerenderingUrl, ActivationBy.JAVASCRIPT);
    }

    // Tests basic end-to-end behavior of speculation rules prerendering on WebView with
    // embedder-initiated activation.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testSpeculationRulesPrerenderingEmbedderInitiatedActivation() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        injectSpeculationRulesAndWait(mPrerenderingUrl);

        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        // onPageStarted should never be called for prerender initial navigation.
        Assert.assertEquals(onPageStartedHelper.getCallCount(), 1);
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);

        activatePage(mPrerenderingUrl, ActivationBy.LOAD_URL);
    }

    // Tests speculation rules prerendering with No-Vary-Search header.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({BlinkFeatures.PRERENDER2_NO_VARY_SEARCH})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testNoVarySearchHeader() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /*kActivated*/ 0)
                        .build();

        // Start prerendering `prerender.html`. This response will have
        // `No-Vary-Search: params=("a")` header.
        injectSpeculationRulesAndWait(mPrerenderingUrl);

        // Navigate to `prerender.html?a=42`. This doesn't exactly match the prerendering URL but
        // should activate the prerendered page for the No-Vary-Search header.
        String url = mTestServer.getURL(PRERENDER_URL.concat("?a=42"));
        activatePage(url, ActivationBy.JAVASCRIPT);

        // Wait until the navigation activates the prerendered page.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests speculation rules prerendering with No-Vary-Search header with multiple params.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({BlinkFeatures.PRERENDER2_NO_VARY_SEARCH})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testNoVarySearchHeaderMultipleParams() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /*kActivated*/ 0)
                        .build();

        final String path =
                "/android_webview/test/data/prerender-no-vary-search-multiple-params.html";

        // Start prerendering `?a=1&b=2&c=3`. This response will have
        // `No-Vary-Search: key-order, params, except=("a" "c")` header.
        final String prerenderingUrl = mTestServer.getURL(path.concat("?a=1&b=2&c=3"));
        injectSpeculationRulesAndWait(prerenderingUrl);

        // Navigate to `?c=3&b=20&a=1`. This doesn't exactly match the prerendering URL but should
        // activate the prerendered page for the No-Vary-Search header.
        final String navigatingUrl = mTestServer.getURL(path.concat("?c=3&b=20&a=1"));
        activatePage(navigatingUrl, ActivationBy.JAVASCRIPT);

        // Wait until the navigation activates the prerendered page.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests speculation rules prerendering with No-Vary-Search header. This is similar to the
    // previous test but navigates to a URL whose search param is different from the No-Vary-Search
    // header. This should not activate the prerendered page.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({BlinkFeatures.PRERENDER2_NO_VARY_SEARCH})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testNoVarySearchHeaderUnignorableSearchParam() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /*kTriggerDestroyed*/ 16)
                        .build();

        // Start prerendering `prerender.html`. This response will have
        // `No-Vary-Search: params=("a")` header.
        injectSpeculationRulesAndWait(mPrerenderingUrl);

        // Navigate to `prerender.html?b=42`. This doesn't match even with the No-Vary-Search
        // header.
        String url = mTestServer.getURL(PRERENDER_URL.concat("?b=42"));
        navigatePage(url);

        // Wait until prerendering is canceled for navigation to the URL whose search param is
        // unignorable.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests FrameTree swap of AwContentsIoThreadClient by observing that callbacks are correctly
    // called after prerender activation.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testAwContentsIoThreadClientHandleFrameTreeSwapForward() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        String url1 = mTestServer.getURL(INITIAL_URL.concat("?q=1"));
        String url2 = mTestServer.getURL(PRERENDER_URL);
        String url3 = mTestServer.getURL(INITIAL_URL.concat("?q=3"));
        String scriptUrl = mTestServer.getURL(PRERENDER_SETUP_SCRIPT_URL);

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
        injectSpeculationRules(url2);
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(url2));

        helper.clearUrls();
        callCount = helper.getCallCount();
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(scriptUrl));

        callCount = helper.getCallCount();
        // Prerender activation will trigger a FrameTree swap and a RenderFrameHostChanged call.
        activatePage(url2, ActivationBy.JAVASCRIPT);
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
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testAwContentsIoThreadClientHandleFrameTreeSwapBack() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        String url1 = mTestServer.getURL(INITIAL_URL.concat("?q=1"));
        String url2 = mTestServer.getURL(PRERENDER_URL);
        String url4 = mTestServer.getURL(INITIAL_URL.concat("?q=4"));
        String scriptUrl = mTestServer.getURL(PRERENDER_SETUP_SCRIPT_URL);

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
        injectSpeculationRules(url2);
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(url2));

        helper.clearUrls();
        callCount = helper.getCallCount();
        helper.waitForCallback(callCount);
        Assert.assertEquals(helper.getUrls(), Arrays.asList(scriptUrl));

        callCount = helper.getCallCount();
        // Prerender activation will trigger a FrameTree swap and a RenderFrameHostChanged call.
        activatePage(url2, ActivationBy.JAVASCRIPT);
        Assert.assertEquals(
                "Prerender activation navigation doesn't trigger shouldInterceptRequest",
                helper.getCallCount(),
                callCount);

        helper.clearUrls();
        callCount = helper.getCallCount();
        // RenderFrameHostChanged without FrameTree swap occurs here.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.evaluateJavaScript("history.back();", null);
                });
        // If BfCache is enabled, the original page restore will not trigger
        // ShouldInterceptRequest. However, the prerender page will get loaded
        // since the injected speculation rule also gets restored.
        if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_BACK_FORWARD_CACHE)) {
            // Wait for loading of the prerendered page and the resource.
            helper.waitForCallback(callCount, 2);
            Assert.assertEquals(helper.getUrls(), Arrays.asList(url2, scriptUrl));
        } else {
            helper.waitForCallback(callCount);
            Assert.assertEquals(helper.getUrls(), Arrays.asList(url1));
        }

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
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingAndShouldInterceptRequest() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

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
        activatePage(mPrerenderingUrl, ActivationBy.JAVASCRIPT);
        Assert.assertEquals(
                "Prerender activation navigation doesn't trigger shouldInterceptRequest",
                shouldInterceptRequestHelper.getCallCount(),
                currentShouldInterceptRequestCallCount);
    }

    // Tests prerendering can succeed with a custom response served by ShouldInterceptRequest.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingWithCustomResponse() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
                mContentsClient.getShouldInterceptRequestHelper();

        // This test will attempt to prerender a non-existent URL. Generally this should fail, but
        // in this test shouldInterceptRequestHelper will serve a custom response instead.
        final String nonExistentUrl =
                mTestServer.getURL("/android_webview/test/data/non_existent.html");

        // Construct a custom response.
        FileInputStream body = new FileInputStream(UrlUtils.getIsolatedTestFilePath(PRERENDER_URL));
        WebResourceResponseInfo response = new WebResourceResponseInfo("text/html", "utf-8", body);
        shouldInterceptRequestHelper.setReturnValueForUrl(nonExistentUrl, response);

        final String scriptUrl = mTestServer.getURL(PRERENDER_SETUP_SCRIPT_URL);
        FileInputStream scriptBody =
                new FileInputStream(UrlUtils.getIsolatedTestFilePath(PRERENDER_SETUP_SCRIPT_URL));
        WebResourceResponseInfo scriptResponse =
                new WebResourceResponseInfo("text/javascript", "utf-8", scriptBody);
        shouldInterceptRequestHelper.setReturnValueForUrl(scriptUrl, scriptResponse);

        int currentShouldInterceptRequestCallCount = shouldInterceptRequestHelper.getCallCount();

        // This doesn't wait for prerendering navigation as the waiting logic is implemented on top
        // of onLoadResource that is never called when a custom response is served.
        injectSpeculationRules(nonExistentUrl);

        // Ensure that ShouldInterceptRequest is called for the main resource and the setup script.
        shouldInterceptRequestHelper.waitForCallback(currentShouldInterceptRequestCallCount);
        AwContentsClient.AwWebResourceRequest request =
                shouldInterceptRequestHelper.getRequestsForUrl(nonExistentUrl);
        Assert.assertNotNull(request);

        shouldInterceptRequestHelper.waitForNext();
        AwContentsClient.AwWebResourceRequest scriptRequest =
                shouldInterceptRequestHelper.getRequestsForUrl(scriptUrl);
        Assert.assertNotNull(scriptRequest);

        // Activation with the non-existent URL should succeed.
        activatePage(nonExistentUrl, ActivationBy.JAVASCRIPT);
    }

    // Tests ShouldOverrideUrlLoading interaction with prerendering.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingAndShouldOverrideUrlLoading() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

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
        activatePage(mPrerenderingUrl, ActivationBy.JAVASCRIPT);
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
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testRedirectedPrerenderingAndShouldOverrideUrlLoading() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

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

        activatePage(initialPrerenderingUrl, mPrerenderingUrl, ActivationBy.JAVASCRIPT);

        // Activation navigation should also be visible to shouldOverrideUrlLoading.
        shouldOverrideUrlLoadingHelper.waitForCallback(currentShouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(),
                initialPrerenderingUrl);
        Assert.assertNull(
                "activation naivgation should have null requestHeaders.",
                shouldOverrideUrlLoadingHelper.requestHeaders());
    }

    // Tests that subframe navigation of prerendered page emits shouldInterceptRequest with
    // Sec-Purpose header.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testSubframeOfPrerenderedPageAndShouldInterceptRequest() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        String subframeUrl1 = mTestServer.getURL("/android_webview/test/data/hello_world.html?q=1");
        String subframeUrl2 = mTestServer.getURL("/android_webview/test/data/hello_world.html?q=2");
        String prerenderUrl =
                mTestServer.getURL(
                        "/android_webview/test/data/prerender.html?iframeSrc="
                                .concat(subframeUrl1));
        String scriptUrl = mTestServer.getURL(PRERENDER_SETUP_SCRIPT_URL);

        final TestAwContentsClient.ShouldInterceptRequestHelper helper =
                mContentsClient.getShouldInterceptRequestHelper();

        {
            helper.clearUrls();
            int callCount = helper.getCallCount();
            injectSpeculationRules(prerenderUrl);
            helper.waitForCallback(callCount);
            Assert.assertEquals(helper.getUrls(), Arrays.asList(prerenderUrl));
            AwContentsClient.AwWebResourceRequest request = helper.getRequestsForUrl(prerenderUrl);
            Assert.assertEquals(request.requestHeaders.get("Sec-Purpose"), "prefetch;prerender");
        }

        {
            helper.clearUrls();
            int callCount = helper.getCallCount();
            helper.waitForCallback(callCount);
            Assert.assertEquals(helper.getUrls(), Arrays.asList(scriptUrl));
            AwContentsClient.AwWebResourceRequest request = helper.getRequestsForUrl(scriptUrl);
            // Subframe navigation of prerendered page also has a Sec-Purpose header.
            Assert.assertEquals(request.requestHeaders.get("Sec-Purpose"), "prefetch;prerender");
        }

        {
            helper.clearUrls();
            int callCount = helper.getCallCount();
            helper.waitForCallback(callCount);
            Assert.assertEquals(helper.getUrls(), Arrays.asList(subframeUrl1));
            AwContentsClient.AwWebResourceRequest request = helper.getRequestsForUrl(subframeUrl1);
            // Subframe navigation of prerendered page also has a Sec-Purpose header.
            Assert.assertEquals(request.requestHeaders.get("Sec-Purpose"), "prefetch;prerender");
        }

        {
            int callCount = helper.getCallCount();
            activatePage(prerenderUrl, ActivationBy.JAVASCRIPT);
            Assert.assertEquals(
                    "Prerender activation navigation doesn't trigger shouldInterceptRequest",
                    helper.getCallCount(),
                    callCount);
        }

        {
            helper.clearUrls();
            int callCount = helper.getCallCount();
            final String script = String.format("createIframe('%s');", subframeUrl2);
            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, script);
            helper.waitForCallback(callCount);
            Assert.assertEquals(helper.getUrls(), Arrays.asList(subframeUrl2));
            AwContentsClient.AwWebResourceRequest request = helper.getRequestsForUrl(subframeUrl2);
            // Subframe navigation of the activated page doesn't have a Sec-Purpose header.
            Assert.assertNotNull(request.requestHeaders);
            Assert.assertNull(request.requestHeaders.get("Sec-Purpose"));
        }
    }

    // Tests postMessage() from JS to Java during prerendering are deferred until activation.
    // TODO(crbug.com/41490450): Test postMessage() from iframes.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPostMessageDuringPrerendering() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

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
        Assert.assertTrue(mDeferredWebMessageListener.hasNoMoreOnPostMessage());

        activatePage(mPrerenderingUrl, ActivationBy.JAVASCRIPT);

        // The page is activated. Now the deferred messages should be delivered.
        TestWebMessageListener.Data data = mDeferredWebMessageListener.waitForOnPostMessage();

        assertUrlHasOrigin(mPrerenderingUrl, data.mTopLevelOrigin);
        assertUrlHasOrigin(mPrerenderingUrl, data.mSourceOrigin);
        Assert.assertEquals("Prerendered", data.getAsString());
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mDeferredWebMessageListener.hasNoMoreOnPostMessage());
    }

    // Tests that WebView.addJavascriptInterface() cancels prerendered pages.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingCanceledWhenAddingJSInterface() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /*kJavaScriptInterfaceAdded*/ 79)
                        .build();

        // Start prerendering.
        injectSpeculationRulesAndWait(mPrerenderingUrl);

        // Inject a JavaScript object. This should cancel prerendering.
        Object testInjectedObject =
                new Object() {
                    @JavascriptInterface
                    public void mock() {}
                };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                mAwContents, testInjectedObject, "testInjectedObject");

        // Wait until prerendering is canceled for the interface addition.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests that WebView.removeJavascriptInterface() cancels prerendered pages.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingCanceledWhenRemovingJSInterface() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /*kJavaScriptInterfaceRemoved*/ 80)
                        .build();

        // Inject a JavaScript object.
        Object testInjectedObject =
                new Object() {
                    @JavascriptInterface
                    public void mock() {}
                };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                mAwContents, testInjectedObject, "testInjectedObject");

        // Start prerendering.
        injectSpeculationRulesAndWait(mPrerenderingUrl);

        // Remove the JavaScript object. This should cancel prerendering.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.removeJavascriptInterface("testInjectedObject"));

        // Wait until prerendering is canceled for the interface removal.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests that WebViewCompat.addWebMessageListener() cancels prerendered pages.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingCanceledWhenAddingWebMessageListener() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /*kAllPrerenderingCanceled*/ 81)
                        .build();

        // Start prerendering.
        injectSpeculationRulesAndWait(mPrerenderingUrl);

        // Add a WebMessageListener. This should cancel prerendering.
        TestWebMessageListener listener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "awMessagePort", new String[] {"*"}, listener);

        // Wait until prerendering is canceled for the listener addition.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests that WebViewCompat.addDocumentStartJavascript() cancels prerendered pages.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingCanceledWhenAddingDocumentStartJavascript() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /*kAllPrerenderingCanceled*/ 81)
                        .build();

        // Start prerendering.
        injectSpeculationRulesAndWait(mPrerenderingUrl);

        // Add a document start javascript. This should cancel prerendering.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.addDocumentStartJavaScript(
                                "console.log(\"hello world\");", new String[] {"*"}));

        // Wait until prerendering is canceled for the start script addition.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests that removing document start javascript cancels prerendered pages.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingCanceledWhenRemovingDocumentStartJavascript() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        // Add a document start javascript. This should cancel prerendering.
        ScriptHandler handler =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mAwContents.addDocumentStartJavaScript(
                                        "console.log(\"hello world\");", new String[] {"*"}));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /*kAllPrerenderingCanceled*/ 81)
                        .build();

        // Start prerendering.
        injectSpeculationRulesAndWait(mPrerenderingUrl);

        // Remove the document start javascript. This should cancel prerendering.
        ThreadUtils.runOnUiThreadBlocking(() -> handler.remove());

        // Wait until prerendering is canceled for the start script addition.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests manually cancelling the prerendered pages.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testPrerenderingManuallyCancelled() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /*kAllPrerenderingCanceled*/ 81)
                        .build();

        // Start prerendering.
        injectSpeculationRulesAndWait(mPrerenderingUrl);
        // Manually cancel the prerendered pages.
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.cancelAllPrerendering());

        // Wait until prerendering is canceled.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testSpeculativeLoadingDisabled() throws Throwable {
        // Do not `setSpeculativeLoadingAllowed()`.
        loadInitialPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /* kPreloadingUnsupportedByWebContents */ 62)
                        .build();

        // Start prerendering.
        injectSpeculationRules(mPrerenderingUrl);

        // Wait until prerendering is canceled for the listener addition.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }
}
