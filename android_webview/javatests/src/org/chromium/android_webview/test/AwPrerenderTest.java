// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;

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
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.net.test.EmbeddedTestServer;

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

    private static final String HELLO_WORLD_URL = "/android_webview/test/data/hello_world.html";
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private EmbeddedTestServer mTestServer;
    private String mPageUrl;
    private String mPrerenderingUrl;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startBrowserProcess();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());

        // Load an initial page that will be triggering speculation rules prerendering.
        mPageUrl = mTestServer.getURL(HELLO_WORLD_URL);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mPageUrl);

        // Prepare speculation rules script.
        mPrerenderingUrl = mPageUrl + "?prerender";

        // Wait for onPageStarted for the initial page load.
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        onPageStartedHelper.waitForCallback(0, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private void injectSpeculationRules(String url) throws Exception {
        final OnLoadResourceHelper onLoadResourceHelper = mContentsClient.getOnLoadResourceHelper();
        int currentOnLoadResourceCallCount = onLoadResourceHelper.getCallCount();

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

        // Wait for prerendering navigation. Monitor onLoadResource instead of
        // onPageFinished as onPageFinished is never called during prerendering
        // (deferred until activation).
        onLoadResourceHelper.waitForCallback(
                currentOnLoadResourceCallCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onLoadResourceHelper.getLastLoadedResource(), url);
    }

    private void activatePage(String url) throws Exception {
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        int currentOnPageStartedCallCount = onPageStartedHelper.getCallCount();

        // Activate the prerendered page.
        mActivityTestRule.runOnUiThread(
                () -> {
                    final String activationScript = String.format("location.href = `%s`;", url);
                    mAwContents.evaluateJavaScript(activationScript, null);
                });

        // Wait until the page is activated.
        onPageStartedHelper.waitForCallback(
                currentOnPageStartedCallCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onPageStartedHelper.getUrl(), url);
    }

    // Tests basic end-to-end behavior of speculation rules prerendering on WebView.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testSpeculationRulesPrerendering() throws Throwable {
        injectSpeculationRules(mPrerenderingUrl);

        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        // onPageStarted should never be called for prerender initial navigation.
        Assert.assertEquals(onPageStartedHelper.getCallCount(), 1);
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);

        activatePage(mPrerenderingUrl);
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

        injectSpeculationRules(mPrerenderingUrl);

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
                "Unexpected call to ShouldInterceptRequest on page activation",
                shouldInterceptRequestHelper.getCallCount(),
                currentShouldInterceptRequestCallCount);
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

        injectSpeculationRules(mPrerenderingUrl);

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
}
