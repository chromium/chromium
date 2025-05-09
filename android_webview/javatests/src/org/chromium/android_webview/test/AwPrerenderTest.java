// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;

import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.webkit.JavascriptInterface;

import androidx.annotation.Nullable;
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
import org.chromium.android_webview.AwFeatureMap;
import org.chromium.android_webview.AwNoVarySearchData;
import org.chromium.android_webview.AwPrefetchCallback;
import org.chromium.android_webview.AwPrefetchParameters;
import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.android_webview.ScriptHandler;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.settings.SpeculativeLoadingAllowedFlags;
import org.chromium.base.Callback;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.ServerCertificate;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

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

    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private static final String INITIAL_URL = "/android_webview/test/data/hello_world.html";
    private static final String PRERENDER_URL = "/android_webview/test/data/prerender.html";
    private static final String PRERENDER_SETUP_SCRIPT_URL =
            "/android_webview/test/data/prerender-test-setup.js";

    private static final String FINAL_STATUS_UMA =
            "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_WebView";

    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private AwEmbeddedTestServer mTestServer;
    private String mPageUrl;
    private String mPrerenderingUrl;

    private SettableFuture<Boolean> mActivationFuture;
    private SettableFuture<Boolean> mPostMessageFuture;

    private TestWebMessageListener mDeferredWebMessageListener;
    private TestWebMessageListener mPrerenderLifecycleWebMessageListener;

    private static class ActivationCallbackHelper extends CallbackHelper {
        public Callback<Void> getCallback() {
            return new Callback<Void>() {
                @Override
                public void onResult(Void result) {
                    notifyCalled();
                }
            };
        }
    }

    private static class PrerenderErrorCallbackHelper extends CallbackHelper {
        public Callback<Throwable> getCallback() {
            return new Callback<Throwable>() {
                @Override
                public void onResult(Throwable result) {
                    notifyCalled();
                }
            };
        }
    }

    private ActivationCallbackHelper mActivationCallbackHelper;
    private PrerenderErrorCallbackHelper mPrerenderErrorCallbackHelper;

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
                AwEmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_TEST_NAMES);

        mPageUrl = getUrl(INITIAL_URL);
        mPrerenderingUrl = getUrl(PRERENDER_URL);

        mActivationCallbackHelper = new ActivationCallbackHelper();
        mPrerenderErrorCallbackHelper = new PrerenderErrorCallbackHelper();
    }

    // Returns a URL. This requires ContentSwitches.HOST_RESOLVER_RULES.
    public String getUrl(final String relativeUrl) {
        return mTestServer.getURLWithHostName("a.test", relativeUrl);
    }

    // This is similar to getUrl() but returns a same-site cross-origin URL against getUrl().
    public String getSameSiteCrossOriginUrl(final String relativeUrl) {
        return mTestServer.getURLWithHostName("b.a.test", relativeUrl);
    }

    // This is similar to getUrl() but returns a cross-site URL against getUrl().
    public String getCrossSiteUrl(final String relativeUrl) {
        return mTestServer.getURLWithHostName("c.test", relativeUrl);
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
        // Java via `mPrerenderLifecycleWebMessageListener`. Note that this works only when the
        // pages are in the same origin for the restriction of the localStorage.
        final String channelScript =
                """
                    {
                      window.localStorage.clear();
                      window.addEventListener("storage", event => {
                        if (event.key === "pageStarted") {
                          window.localStorage.clear();
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
                    // Ensure that the view is visible, as prerendering cannot start in background.
                    mAwContents.getWebContents().updateWebContentsVisibility(Visibility.VISIBLE);
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

    // Triggers prerendering for `url`.
    private void startPrerendering(
            String url,
            AwPrefetchParameters prefetchParameters,
            CancellationSignal cancellationSignal,
            Callback<Void> activationCallback,
            Callback<Throwable> errorCallback)
            throws Exception {
        Executor callbackExecutor = (Runnable r) -> r.run();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Ensure that the view is visible, as prerendering cannot start in background.
                    mAwContents.getWebContents().updateWebContentsVisibility(Visibility.VISIBLE);
                    mAwContents.startPrerendering(
                            url,
                            prefetchParameters,
                            cancellationSignal,
                            callbackExecutor,
                            activationCallback,
                            errorCallback);
                });
    }

    // Triggers prerendering for `url` and then waits until a prerendered page starts running
    // JavaScript.
    private void startPrerenderingAndWait(
            String url,
            AwPrefetchParameters prefetchParameters,
            CancellationSignal cancellationSignal,
            Callback<Void> activationCallback,
            Callback<Throwable> errorCallback)
            throws Exception {
        startPrerendering(
                url, prefetchParameters, cancellationSignal, activationCallback, errorCallback);

        // Wait until the prerendered page starts running JavaScript.
        mPrerenderLifecycleWebMessageListener.waitForOnPostMessage();
    }

    // Triggers prefetching for `url` and then waits until response completion.
    private void startPrefetchingAndWait(String url, AwPrefetchParameters prefetchParameters)
            throws Exception {
        CallbackHelper prefetchCallbackHelper = new CallbackHelper();
        AwPrefetchCallback callback =
                new AwPrefetchCallback() {
                    @Override
                    public void onStatusUpdated(
                            @StatusCode int statusCode, @Nullable Bundle extras) {
                        switch (statusCode) {
                            case StatusCode.PREFETCH_RESPONSE_COMPLETED:
                                prefetchCallbackHelper.notifyCalled();
                                break;
                            default:
                                Assert.assertFalse(true);
                                prefetchCallbackHelper.notifyFailed("Failed");
                                break;
                        }
                    }

                    @Override
                    public void onError(Throwable e) {
                        prefetchCallbackHelper.notifyFailed(e.getMessage());
                    }
                };

        Executor callbackExecutor = (Runnable r) -> r.run();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getAwBrowserContext()
                            .getPrefetchManager()
                            .startPrefetchRequest(
                                    url, prefetchParameters, callback, callbackExecutor);
                });

        prefetchCallbackHelper.waitForNext();
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
            String activateUrl,
            String expectedActivatedUrl,
            ActivationBy activationBy,
            Map<String, String> extraHeaders)
            throws Exception {
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        int currentOnPageStartedCallCount = onPageStartedHelper.getCallCount();

        // Activate the prerendered page.
        switch (activationBy) {
            case LOAD_URL:
                mActivityTestRule.loadUrlAsync(mAwContents, activateUrl, extraHeaders);
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
        Assert.assertEquals(expectedActivatedUrl, onPageStartedHelper.getUrl());

        // Make sure the page was actually prerendered and then activated. These checks are
        // available only for a page served from PRERENDER_URL, as these depend on JavaScript code
        // injected there.
        if (expectedActivatedUrl.contains(PRERENDER_URL)) {
            Assert.assertEquals(
                    true, mActivationFuture.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
            Assert.assertEquals(
                    "true",
                    mActivityTestRule.executeJavaScriptAndWaitForResult(
                            mAwContents, mContentsClient, "wasPrerendered"));
        }
    }

    // Shorthand notation of `activatePage` without `extraHeaders`.
    private void activatePage(
            String activateUrl, String expectedActivatedUrl, ActivationBy activationBy)
            throws Exception {
        activatePage(activateUrl, expectedActivatedUrl, activationBy, /* extraHeaders= */ null);
    }

    // Shorthand notation of `activatePage(activateUrl, activateUrl)` without `extraHeaders`.
    private void activatePage(String activateUrl, ActivationBy activationBy) throws Exception {
        activatePage(activateUrl, activateUrl, activationBy);
    }

    private void setMaxPrerenders(int maxPrerenders) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getAwBrowserContext().setMaxPrerenders(maxPrerenders);
                });
    }

    private void testPrerenderingWithInvalidAdditionalHeaders(
            Map<String, String> invalidAdditionalHeaders) {
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(
                        invalidAdditionalHeaders,
                        /* expectedNoVarySearch= */ null,
                        /* isJavascriptEnabled= */ true);
        Executor callbackExecutor = (Runnable r) -> r.run();

        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            try {
                                mAwContents.startPrerendering(
                                        mPrerenderingUrl,
                                        prefetchParameters,
                                        /* cancellationSignal= */ null,
                                        callbackExecutor,
                                        mActivationCallbackHelper.getCallback(),
                                        mPrerenderErrorCallbackHelper.getCallback());
                                return false;
                            } catch (IllegalArgumentException e) {
                                return true;
                            }
                        }));
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

    private static HistogramWatcher createFinalStatusHistogramWatcher(int expectedStatus) {
        return createFinalStatusHistogramWatcher(new int[] {expectedStatus});
    }

    private static HistogramWatcher createFinalStatusHistogramWatcher(int[] expectedStatuses) {
        HistogramWatcher.Builder builder = HistogramWatcher.newBuilder();
        builder.expectIntRecords(FINAL_STATUS_UMA, expectedStatuses);
        return builder.build();
    }

    // Tests basic end-to-end behavior of speculation rules prerendering on WebView with
    // renderer-initiated activation.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testSpeculationRulesPrerenderingRendererInitiatedActivation() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        injectSpeculationRulesAndWait(mPrerenderingUrl);

        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        // onPageStarted should never be called for prerender initial navigation.
        Assert.assertEquals(1, onPageStartedHelper.getCallCount());
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);

        activatePage(mPrerenderingUrl, ActivationBy.JAVASCRIPT);
    }

    // Tests basic end-to-end behavior of speculation rules prerendering on WebView with
    // embedder-initiated activation.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testSpeculationRulesPrerenderingEmbedderInitiatedActivation() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        injectSpeculationRulesAndWait(mPrerenderingUrl);

        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        // onPageStarted should never be called for prerender initial navigation.
        Assert.assertEquals(1, onPageStartedHelper.getCallCount());
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);

        activatePage(mPrerenderingUrl, ActivationBy.LOAD_URL);
    }

    // Tests the case where speculation rules triggers prerendering and then loadUrl attempts to
    // activate it with additional headers. This activation seems to fail because speculation
    // rules doesn't send the additional headers, but actually the activation should succeed as an
    // exceptional case (see https://crbug.com/399478939 for details).
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testSpeculationRulesPrerenderingEmbedderInitiatedActivationWithAdditionalHeaders()
            throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
                                /*kActivated*/ 0)
                        .build();

        injectSpeculationRulesAndWait(mPrerenderingUrl);

        // Attempt to activate with additional headers.
        HashMap<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("Test-Header", "1");
        activatePage(mPrerenderingUrl, mPrerenderingUrl, ActivationBy.LOAD_URL, additionalHeaders);

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests basic end-to-end behavior of WebView prerendering trigger with
    // embedder-initiated activation.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrerenderingEmbedderInitiatedActivation() throws Throwable {
        loadInitialPage();

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kActivated*/ 0);

        int currentCallCount = mActivationCallbackHelper.getCallCount();

        var cancellationSignal = new CancellationSignal();

        startPrerenderingAndWait(
                mPrerenderingUrl,
                /* prefetchParameters= */ null,
                cancellationSignal,
                mActivationCallbackHelper.getCallback(),
                mPrerenderErrorCallbackHelper.getCallback());

        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        // onPageStarted should never be called for prerender initial navigation.
        Assert.assertEquals(1, onPageStartedHelper.getCallCount());
        Assert.assertEquals(onPageStartedHelper.getUrl(), mPageUrl);

        // Make sure that prerendering navigation has the Sec-Purpose header.
        HashMap<String, String> headers = mTestServer.getRequestHeadersForUrl(PRERENDER_URL);
        Assert.assertEquals("prefetch;prerender", headers.get("Sec-Purpose"));

        activatePage(mPrerenderingUrl, ActivationBy.LOAD_URL);

        // Wait until the navigation activates the prerendered page.
        mActivationCallbackHelper.waitForCallback(currentCallCount);
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        // Cancel after activation should not cause a runtime error.
        Assert.assertFalse(cancellationSignal.isCanceled());
        cancellationSignal.cancel();
        Assert.assertTrue(cancellationSignal.isCanceled());
    }

    // Tests the case where a user navigates to a page different from a prerendered page.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testNavigationToNonPrerenderedPage() throws Throwable {
        loadInitialPage();

        var histogramWatcher = createFinalStatusHistogramWatcher(/* kTriggerDestroyed */ 16);

        startPrerenderingAndWait(
                mPrerenderingUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                mActivationCallbackHelper.getCallback(),
                mPrerenderErrorCallbackHelper.getCallback());

        // Navigate to `prerender.html?b=42` that doesn't match the prerendered page. This should
        // cancel prerendering.
        String url = getUrl(PRERENDER_URL.concat("?b=42"));
        navigatePage(url);

        // Wait until prerendering is canceled.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        // The error callback should not be called in this case.
        Assert.assertEquals(0, mPrerenderErrorCallbackHelper.getCallCount());
    }

    // Tests prerendering navigation that is redirected to same-origin.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testSameOriginRedirection() throws Throwable {
        loadInitialPage();

        // Construct an initial prerendering URL that is redirected to `mPrerenderingUrl`.
        final String initialPrerenderingPath =
                "/server-redirect-echoheader?url=" + encodeUrl(PRERENDER_URL);
        final String initialPrerenderingUrl = getUrl(initialPrerenderingPath);

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kActivated*/ 0);

        int currentCallCount = mActivationCallbackHelper.getCallCount();

        startPrerenderingAndWait(
                initialPrerenderingUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                mActivationCallbackHelper.getCallback(),
                mPrerenderErrorCallbackHelper.getCallback());

        activatePage(initialPrerenderingUrl, mPrerenderingUrl, ActivationBy.LOAD_URL);

        // Wait until the navigation activates the prerendered page.
        mActivationCallbackHelper.waitForCallback(currentCallCount);
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        // Make sure that prerendering navigation has the Sec-Purpose header.
        HashMap<String, String> initialHeaders =
                mTestServer.getRequestHeadersForUrl(initialPrerenderingPath);
        Assert.assertEquals("prefetch;prerender", initialHeaders.get("Sec-Purpose"));
        HashMap<String, String> redirectedHeaders =
                mTestServer.getRequestHeadersForUrl(PRERENDER_URL);
        Assert.assertEquals("prefetch;prerender", redirectedHeaders.get("Sec-Purpose"));
    }

    // Tests prerendering navigation that is redirected to same-site cross-origin. As WebView API is
    // treated as an embedder trigger that doesn't have the initiator, same-site cross-origin
    // redirection should be allowed even without the "Supports-Loading-Mode" opt-in header.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testSameSiteCrossOriginRedirection() throws Throwable {
        loadInitialPage();

        // Construct an initial prerendering URL that is redirected to `mPrerenderingUrl`.
        final String initialPrerenderingPath =
                "/server-redirect-echoheader?url="
                        + encodeUrl(getSameSiteCrossOriginUrl(PRERENDER_URL));
        final String initialPrerenderingUrl = getUrl(initialPrerenderingPath);

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kActivated*/ 0);

        int currentCallCount = mActivationCallbackHelper.getCallCount();

        // Don't use startPrerenderingAndWait(), as the waiting logic requires that both the
        // initiator page and the prerendered page are in the same origin.
        startPrerendering(
                initialPrerenderingUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                mActivationCallbackHelper.getCallback(),
                mPrerenderErrorCallbackHelper.getCallback());

        activatePage(
                initialPrerenderingUrl,
                getSameSiteCrossOriginUrl(PRERENDER_URL),
                ActivationBy.LOAD_URL);

        // Wait until the navigation activates the prerendered page.
        mActivationCallbackHelper.waitForCallback(currentCallCount);
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        // Make sure that prerendering navigation has the Sec-Purpose header.
        HashMap<String, String> initialHeaders =
                mTestServer.getRequestHeadersForUrl(initialPrerenderingPath);
        Assert.assertEquals("prefetch;prerender", initialHeaders.get("Sec-Purpose"));
        HashMap<String, String> redirectedHeaders =
                mTestServer.getRequestHeadersForUrl(PRERENDER_URL);
        Assert.assertEquals("prefetch;prerender", redirectedHeaders.get("Sec-Purpose"));
    }

    // Tests prerendering navigation that is redirected to cross-site. Cross-site redirection should
    // cancel prerendering.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testCrossSiteRedirection() throws Throwable {
        loadInitialPage();

        // Construct an initial prerendering URL that is redirected to `mPrerenderingUrl`.
        final String initialPrerenderingPath =
                "/server-redirect-echoheader?url=" + encodeUrl(getCrossSiteUrl(PRERENDER_URL));
        final String initialPrerenderingUrl = getUrl(initialPrerenderingPath);

        int currentCallCount = mPrerenderErrorCallbackHelper.getCallCount();
        var histogramWatcher =
                createFinalStatusHistogramWatcher(/*kCrossSiteRedirectInInitialNavigation */ 44);

        // Don't use startPrerenderingAndWait(), as the waiting logic requires that both the
        // initiator page and the prerendered page are in the same origin.
        startPrerendering(
                initialPrerenderingUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                mActivationCallbackHelper.getCallback(),
                mPrerenderErrorCallbackHelper.getCallback());

        // Wait until prerendering is canceled, as cross-site prerendering is disallowed.
        mPrerenderErrorCallbackHelper.waitForCallback(currentCallCount);
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        // Make sure that prerendering navigation has the Sec-Purpose header.
        HashMap<String, String> initialHeaders =
                mTestServer.getRequestHeadersForUrl(initialPrerenderingPath);
        Assert.assertEquals("prefetch;prerender", initialHeaders.get("Sec-Purpose"));
        // On the other hand, the redirected request should not be sent to the server for the
        // cross-site restriction.
        Assert.assertEquals(0, mTestServer.getRequestCountForUrl(PRERENDER_URL));
    }

    // Tests additional request headers on WebView prerendering trigger.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testAdditionalHeaders() throws Throwable {
        loadInitialPage();

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kActivated*/ 0);

        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        int currentShouldInterceptRequestCallCount = shouldInterceptRequestHelper.getCallCount();

        HashMap<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("Test-Header1", "1");
        additionalHeaders.put("Test-Header2", "2");

        // Prerender with the additional headers.
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(
                        additionalHeaders,
                        /* expectedNoVarySearch= */ null,
                        /* isJavascriptEnabled= */ true);
        startPrerendering(
                mPrerenderingUrl,
                prefetchParameters,
                /* cancellationSignal= */ null,
                mActivationCallbackHelper.getCallback(),
                mPrerenderErrorCallbackHelper.getCallback());

        // shouldInterceptRequest should see the additional headers on prerendering navigation.
        shouldInterceptRequestHelper.waitForCallback(currentShouldInterceptRequestCallCount);
        AwWebResourceRequest mainRequest =
                shouldInterceptRequestHelper.getRequestsForUrl(mPrerenderingUrl);
        Assert.assertNotNull(mainRequest);
        Map<String, String> mainHeaders = mainRequest.getRequestHeaders();
        Assert.assertNotNull(mainHeaders);
        Assert.assertEquals("1", mainHeaders.get("Test-Header1"));
        Assert.assertEquals("2", mainHeaders.get("Test-Header2"));
        // But shouldInterceptRequest should not see the headers on subresource requests.
        shouldInterceptRequestHelper.waitForNext();
        String scriptUrl = getUrl(PRERENDER_SETUP_SCRIPT_URL);
        AwWebResourceRequest scriptRequest =
                shouldInterceptRequestHelper.getRequestsForUrl(scriptUrl);
        Assert.assertNotNull(scriptRequest);
        Map<String, String> scriptHeaders = scriptRequest.getRequestHeaders();
        Assert.assertNotNull(scriptHeaders);
        Assert.assertNull(scriptHeaders.get("Test-Header1"));
        Assert.assertNull(scriptHeaders.get("Test-Header2"));

        activatePage(mPrerenderingUrl, mPrerenderingUrl, ActivationBy.LOAD_URL, additionalHeaders);
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        // The server should also see the additional headers on prerendering navigation.
        mainHeaders = mTestServer.getRequestHeadersForUrl(PRERENDER_URL);
        Assert.assertFalse(mainHeaders.isEmpty());
        Assert.assertEquals("1", mainHeaders.get("Test-Header1"));
        Assert.assertEquals("2", mainHeaders.get("Test-Header2"));
        Assert.assertEquals("prefetch;prerender", mainHeaders.get("Sec-Purpose"));
        // But the server should not see the headers on subresource requests.
        scriptHeaders = mTestServer.getRequestHeadersForUrl(PRERENDER_SETUP_SCRIPT_URL);
        Assert.assertFalse(scriptHeaders.isEmpty());
        Assert.assertNull(scriptHeaders.get("Test-Header1"));
        Assert.assertNull(scriptHeaders.get("Test-Header2"));
        Assert.assertEquals("prefetch;prerender", scriptHeaders.get("Sec-Purpose"));
    }

    // Tests additional request headers that contain an invalid key or value on WebView prerendering
    // trigger.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testInvalidAdditionalHeaders() throws Throwable {
        loadInitialPage();

        final String[] invalids = {"null\u0000", "cr\r", "nl\n"};
        for (String invalid : invalids) {
            // try each invalid string as a key and a value
            testPrerenderingWithInvalidAdditionalHeaders(Map.of("foo", invalid));
            testPrerenderingWithInvalidAdditionalHeaders(Map.of(invalid, "foo"));
        }
    }

    // Tests X-* headers are ignored on header match during activation.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testIgnoreXHeadersOnHeaderMatch() throws Throwable {
        loadInitialPage();

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kActivated*/ 0);

        // Prerender with an "X-Hello" additional header.
        HashMap<String, String> additionalHeadersForPrerender = new HashMap<>();
        additionalHeadersForPrerender.put("X-Hello", "1");

        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(
                        additionalHeadersForPrerender,
                        /* expectedNoVarySearch= */ null,
                        /* isJavascriptEnabled= */ true);

        startPrerenderingAndWait(
                mPrerenderingUrl,
                prefetchParameters,
                /* cancellationSignal= */ null,
                mActivationCallbackHelper.getCallback(),
                mPrerenderErrorCallbackHelper.getCallback());

        // Activate with an "x-world" additional header.
        HashMap<String, String> additionalHeadersForActivation = new HashMap<>();
        additionalHeadersForActivation.put("x-world", "1");
        activatePage(
                mPrerenderingUrl,
                mPrerenderingUrl,
                ActivationBy.LOAD_URL,
                additionalHeadersForActivation);
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests speculation rules prerendering with No-Vary-Search header.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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
        String url = getUrl(PRERENDER_URL.concat("?a=42"));
        activatePage(url, ActivationBy.JAVASCRIPT);

        // Wait until the navigation activates the prerendered page.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests speculation rules prerendering with No-Vary-Search header with multiple params.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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
        final String prerenderingUrl = getUrl(path.concat("?a=1&b=2&c=3"));
        injectSpeculationRulesAndWait(prerenderingUrl);

        // Navigate to `?c=3&b=20&a=1`. This doesn't exactly match the prerendering URL but should
        // activate the prerendered page for the No-Vary-Search header.
        final String navigatingUrl = getUrl(path.concat("?c=3&b=20&a=1"));
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
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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
        String url = getUrl(PRERENDER_URL.concat("?b=42"));
        navigatePage(url);

        // Wait until prerendering is canceled for navigation to the URL whose search param is
        // unignorable.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests WebView prerendering trigger with No-Vary-Search hint and header.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testNoVarySearchHintAndHeader() throws Throwable {
        loadInitialPage();

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kActivated*/ 0);

        int currentCallCount = mActivationCallbackHelper.getCallCount();

        // Start prerendering `prerender.html`. This response will have
        // `No-Vary-Search: params=("a")` header, so specify a corresponding No-Vary-Search hint.
        String[] ignoredQueryParameters = {"a"};
        AwNoVarySearchData noVarySearchData =
                new AwNoVarySearchData(true, true, ignoredQueryParameters, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(
                        /* additionalHeaders= */ null,
                        noVarySearchData,
                        /* isJavascriptEnabled= */ true);
        startPrerendering(
                mPrerenderingUrl,
                prefetchParameters,
                /* cancellationSignal= */ null,
                mActivationCallbackHelper.getCallback(),
                mPrerenderErrorCallbackHelper.getCallback());

        // Navigate to `prerender.html?a=42` without waiting for completion of prerendering so that
        // activation match is conducted based on the No-Vary-Search hint (not the No-Vary-Search
        // header).
        //
        // Actually this test is not stable. The WebView may receive the No-Vary-Search header
        // before starting prerendering. To test the hint in a more reliable manner, this test
        // should suspend prerendering navigation before the header is sent, start prerendering,
        // confirm activation match is done with the hint, and then resume prerendering. Currently,
        // the infra to suspend prerendering is not available.
        // TODO(crbug.com/41490450): Implement the test infra to suspend prerendering and test the
        // No-Vary-Search hint using that.
        String url = getUrl(PRERENDER_URL.concat("?a=42"));
        activatePage(url, ActivationBy.LOAD_URL);

        // Wait until the navigation activates the prerendered page.
        mActivationCallbackHelper.waitForCallback(currentCallCount);
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests FrameTree swap of AwContentsIoThreadClient by observing that callbacks are correctly
    // called after prerender activation.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({
        BlinkFeatures.PRERENDER2_MEMORY_CONTROLS,
        "Prerender2FallbackPrefetchSpecRules"
    })
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testAwContentsIoThreadClientHandleFrameTreeSwapForward() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        String url1 = getUrl(INITIAL_URL.concat("?q=1"));
        String url2 = getUrl(PRERENDER_URL);
        String url3 = getUrl(INITIAL_URL.concat("?q=3"));
        String scriptUrl = getUrl(PRERENDER_SETUP_SCRIPT_URL);

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
    @Features.DisableFeatures({
        BlinkFeatures.PRERENDER2_MEMORY_CONTROLS,
        "Prerender2FallbackPrefetchSpecRules"
    })
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testAwContentsIoThreadClientHandleFrameTreeSwapBack() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        String url1 = getUrl(INITIAL_URL.concat("?q=1"));
        String url2 = getUrl(PRERENDER_URL);
        String url4 = getUrl(INITIAL_URL.concat("?q=4"));
        String scriptUrl = getUrl(PRERENDER_SETUP_SCRIPT_URL);

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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrerenderingAndShouldInterceptRequest() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        int currentShouldInterceptRequestCallCount = shouldInterceptRequestHelper.getCallCount();

        injectSpeculationRulesAndWait(mPrerenderingUrl);

        shouldInterceptRequestHelper.waitForCallback(currentShouldInterceptRequestCallCount);
        AwWebResourceRequest request =
                shouldInterceptRequestHelper.getRequestsForUrl(mPrerenderingUrl);
        Assert.assertNotNull(request);
        Map<String, String> requestHeaders = request.getRequestHeaders();
        Assert.assertNotNull(requestHeaders);
        Assert.assertEquals("prefetch;prerender", requestHeaders.get("Sec-Purpose"));

        currentShouldInterceptRequestCallCount = shouldInterceptRequestHelper.getCallCount();
        activatePage(mPrerenderingUrl, ActivationBy.JAVASCRIPT);
        Assert.assertEquals(
                "Prerender activation navigation doesn't trigger shouldInterceptRequest",
                shouldInterceptRequestHelper.getCallCount(),
                currentShouldInterceptRequestCallCount);
    }

    // Tests ShouldInterceptRequest interaction with subresource requests (sendBeacon) sent from a
    // prerendered page.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({
        BlinkFeatures.PRERENDER2_MEMORY_CONTROLS,
        "Prerender2FallbackPrefetchSpecRules"
    })
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrerenderingAndShouldInterceptRequestForSubresources() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        final String prerenderingUrl =
                getUrl("/android_webview/test/data/prerender-send-beacon.html");
        final String setupScriptUrl = getUrl(PRERENDER_SETUP_SCRIPT_URL);
        // Beacon to be sent during prerendering.
        final String beaconUrl = getUrl("/beacon");
        // Beacon to be sent during the prerenderingchange event (after activation).
        final String beaconUrl2 = getUrl("/beacon-prerenderingchange");

        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        int currentShouldInterceptRequestCallCount = shouldInterceptRequestHelper.getCallCount();
        shouldInterceptRequestHelper.clearUrls();

        injectSpeculationRulesAndWait(prerenderingUrl);

        // Wait for 3 requests: main resource, setup script, and first beacon.
        shouldInterceptRequestHelper.waitForCallback(currentShouldInterceptRequestCallCount, 3);
        Assert.assertEquals(
                shouldInterceptRequestHelper.getUrls(),
                Arrays.asList(prerenderingUrl, setupScriptUrl, beaconUrl));

        // Check if the main resource request was intercepted during prerendering.
        AwWebResourceRequest request =
                shouldInterceptRequestHelper.getRequestsForUrl(prerenderingUrl);
        Assert.assertNotNull(request);
        Map<String, String> requestHeaders = request.getRequestHeaders();
        Assert.assertNotNull(requestHeaders);
        Assert.assertEquals("prefetch;prerender", requestHeaders.get("Sec-Purpose"));

        // Check if the first subresource request (sendBeacon) was intercepted during prerendering.
        AwWebResourceRequest beaconRequest =
                shouldInterceptRequestHelper.getRequestsForUrl(beaconUrl);
        Assert.assertNotNull(beaconRequest);
        Map<String, String> beaconRequestHeaders = beaconRequest.getRequestHeaders();
        Assert.assertNotNull(beaconRequestHeaders);
        Assert.assertEquals("prefetch;prerender", beaconRequestHeaders.get("Sec-Purpose"));

        // Activate the page. This should not be intercepted.
        activatePage(prerenderingUrl, ActivationBy.JAVASCRIPT);

        // Wait for the second beacon request that is sent after activation.
        shouldInterceptRequestHelper.waitForNext();
        Assert.assertEquals(
                shouldInterceptRequestHelper.getUrls(),
                Arrays.asList(prerenderingUrl, setupScriptUrl, beaconUrl, beaconUrl2));

        // Check if the second subresource request (sendBeacon) was intercepted after activation.
        AwWebResourceRequest beaconRequest2 =
                shouldInterceptRequestHelper.getRequestsForUrl(beaconUrl2);
        Assert.assertNotNull(beaconRequest2);
        Map<String, String> beaconRequestHeaders2 = beaconRequest2.getRequestHeaders();
        Assert.assertNotNull(beaconRequestHeaders2);
        Assert.assertFalse(beaconRequestHeaders2.containsKey("Sec-Purpose"));
    }

    // Tests prerendering can succeed with a custom response served by ShouldInterceptRequest.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({
        BlinkFeatures.PRERENDER2_MEMORY_CONTROLS,
        "Prerender2FallbackPrefetchSpecRules"
    })
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrerenderingWithCustomResponse() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        final TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
                mContentsClient.getShouldInterceptRequestHelper();

        // This test will attempt to prerender a non-existent URL. Generally this should fail, but
        // in this test shouldInterceptRequestHelper will serve a custom response instead.
        final String nonExistentUrl = getUrl("/android_webview/test/data/non_existent.html");

        // Construct a custom response.
        shouldInterceptRequestHelper.enqueueResponseForUrlWithStream(
                nonExistentUrl, "text/html", "utf-8", getFileInputStreamSupplier(PRERENDER_URL));

        final String scriptUrl = getUrl(PRERENDER_SETUP_SCRIPT_URL);
        shouldInterceptRequestHelper.enqueueResponseForUrlWithStream(
                scriptUrl,
                "text/javascript",
                "utf-8",
                getFileInputStreamSupplier(PRERENDER_SETUP_SCRIPT_URL));

        int currentShouldInterceptRequestCallCount = shouldInterceptRequestHelper.getCallCount();

        // This doesn't wait for prerendering navigation as the waiting logic is implemented on top
        // of onLoadResource that is never called when a custom response is served.
        injectSpeculationRules(nonExistentUrl);

        // Ensure that ShouldInterceptRequest is called for the main resource and the setup script.
        shouldInterceptRequestHelper.waitForCallback(currentShouldInterceptRequestCallCount);
        AwWebResourceRequest request =
                shouldInterceptRequestHelper.getRequestsForUrl(nonExistentUrl);
        Assert.assertNotNull(request);

        shouldInterceptRequestHelper.waitForNext();
        AwWebResourceRequest scriptRequest =
                shouldInterceptRequestHelper.getRequestsForUrl(scriptUrl);
        Assert.assertNotNull(scriptRequest);

        // Activation with the non-existent URL should succeed.
        activatePage(nonExistentUrl, ActivationBy.JAVASCRIPT);
    }

    private static Supplier<InputStream> getFileInputStreamSupplier(
            String prerenderSetupScriptUrl) {
        Supplier<InputStream> data1 =
                () -> {
                    try {
                        return new FileInputStream(
                                UrlUtils.getIsolatedTestFilePath(prerenderSetupScriptUrl));
                    } catch (FileNotFoundException e) {
                        throw new RuntimeException(e);
                    }
                };
        return data1;
    }

    // Tests ShouldOverrideUrlLoading interaction with prerendering.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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
        Map<String, String> requestHeadersOnShouldOverride =
                shouldOverrideUrlLoadingHelper.requestHeaders();
        Assert.assertNotNull(requestHeadersOnShouldOverride);
        Assert.assertEquals(
                "prefetch;prerender", requestHeadersOnShouldOverride.get("Sec-Purpose"));

        currentShouldOverrideUrlLoadingCallCount = shouldOverrideUrlLoadingHelper.getCallCount();
        activatePage(mPrerenderingUrl, ActivationBy.JAVASCRIPT);
        shouldOverrideUrlLoadingHelper.waitForCallback(currentShouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(), mPrerenderingUrl);
        Assert.assertTrue(
                "activation navigation should have empty requestHeaders.",
                shouldOverrideUrlLoadingHelper.requestHeaders().isEmpty());
    }

    // Tests ShouldOverrideUrlLoading interaction with prerendering that is redirected.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({
        BlinkFeatures.PRERENDER2_MEMORY_CONTROLS,
        "Prerender2FallbackPrefetchSpecRules"
    })
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testRedirectedPrerenderingAndShouldOverrideUrlLoading() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                mContentsClient.getShouldOverrideUrlLoadingHelper();
        int currentShouldOverrideUrlLoadingCallCount =
                shouldOverrideUrlLoadingHelper.getCallCount();

        // Construct an initial prerendering URL that is redirected to `mPrerenderingUrl`.
        final String initialPrerenderingUrl =
                getUrl("/server-redirect-echoheader?url=" + encodeUrl(mPrerenderingUrl));

        injectSpeculationRules(initialPrerenderingUrl);

        // Check if the initial prerendering navigation is visible to shouldOverrideUrlLoading.
        shouldOverrideUrlLoadingHelper.waitForCallback(currentShouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(),
                initialPrerenderingUrl);
        Assert.assertFalse(shouldOverrideUrlLoadingHelper.isRedirect());
        Map<String, String> requestHeadersOnShouldOverride =
                shouldOverrideUrlLoadingHelper.requestHeaders();
        Assert.assertNotNull(requestHeadersOnShouldOverride);
        Assert.assertEquals(
                "prefetch;prerender", requestHeadersOnShouldOverride.get("Sec-Purpose"));

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
                "prefetch;prerender", requestHeadersOnShouldOverride.get("Sec-Purpose"));

        currentShouldOverrideUrlLoadingCallCount = shouldOverrideUrlLoadingHelper.getCallCount();

        activatePage(initialPrerenderingUrl, mPrerenderingUrl, ActivationBy.JAVASCRIPT);

        // Activation navigation should also be visible to shouldOverrideUrlLoading.
        shouldOverrideUrlLoadingHelper.waitForCallback(currentShouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(),
                initialPrerenderingUrl);
        Assert.assertTrue(
                "activation navigation should have empty requestHeaders.",
                shouldOverrideUrlLoadingHelper.requestHeaders().isEmpty());
    }

    // Tests that subframe navigation of prerendered page emits shouldInterceptRequest with
    // Sec-Purpose header.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({
        BlinkFeatures.PRERENDER2_MEMORY_CONTROLS,
        "Prerender2FallbackPrefetchSpecRules"
    })
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testSubframeOfPrerenderedPageAndShouldInterceptRequest() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        String subframeUrl1 = getUrl("/android_webview/test/data/hello_world.html?q=1");
        String subframeUrl2 = getUrl("/android_webview/test/data/hello_world.html?q=2");
        String prerenderUrl =
                getUrl("/android_webview/test/data/prerender.html?iframeSrc=".concat(subframeUrl1));
        String scriptUrl = getUrl(PRERENDER_SETUP_SCRIPT_URL);

        final TestAwContentsClient.ShouldInterceptRequestHelper helper =
                mContentsClient.getShouldInterceptRequestHelper();

        {
            helper.clearUrls();
            int callCount = helper.getCallCount();
            injectSpeculationRules(prerenderUrl);
            helper.waitForCallback(callCount);
            Assert.assertEquals(helper.getUrls(), Arrays.asList(prerenderUrl));
            AwWebResourceRequest request = helper.getRequestsForUrl(prerenderUrl);
            Assert.assertEquals(
                    "prefetch;prerender", request.getRequestHeaders().get("Sec-Purpose"));
        }

        {
            helper.clearUrls();
            int callCount = helper.getCallCount();
            helper.waitForCallback(callCount);
            Assert.assertEquals(helper.getUrls(), Arrays.asList(scriptUrl));
            AwWebResourceRequest request = helper.getRequestsForUrl(scriptUrl);
            // Subframe navigation of prerendered page also has a Sec-Purpose header.
            Assert.assertEquals(
                    "prefetch;prerender", request.getRequestHeaders().get("Sec-Purpose"));
        }

        {
            helper.clearUrls();
            int callCount = helper.getCallCount();
            helper.waitForCallback(callCount);
            Assert.assertEquals(helper.getUrls(), Arrays.asList(subframeUrl1));
            AwWebResourceRequest request = helper.getRequestsForUrl(subframeUrl1);
            // Subframe navigation of prerendered page also has a Sec-Purpose header.
            Assert.assertEquals(
                    "prefetch;prerender", request.getRequestHeaders().get("Sec-Purpose"));
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
            AwWebResourceRequest request = helper.getRequestsForUrl(subframeUrl2);
            // Subframe navigation of the activated page doesn't have a Sec-Purpose header.
            Assert.assertNotNull(request.getRequestHeaders());
            Assert.assertNull(request.getRequestHeaders().get("Sec-Purpose"));
        }
    }

    // Tests postMessage() from JS to Java during prerendering are deferred until activation.
    // TODO(crbug.com/41490450): Test postMessage() from iframes.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPostMessageDuringPrerendering() throws Throwable {
        setSpeculativeLoadingAllowed(SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
        loadInitialPage();

        injectSpeculationRules(mPrerenderingUrl);

        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        // onPageStarted should never be called for prerender initial navigation.
        Assert.assertEquals(1, onPageStartedHelper.getCallCount());
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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
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

    // Tests that prerendering can consume a prefetched response.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchAndPrerender() throws Throwable {
        loadInitialPage();

        // Start prefetching and wait until response completion.
        startPrefetchingAndWait(mPrerenderingUrl, /* prefetchParameters= */ null);

        // Prefetching should send a request.
        Assert.assertEquals(1, mTestServer.getRequestCountForUrl(PRERENDER_URL));
        // Make sure that the prefetch request has the Sec-Purpose header. The value should be
        // "prefetch", not "prefetch;prerender".
        HashMap<String, String> headers = mTestServer.getRequestHeadersForUrl(PRERENDER_URL);
        Assert.assertEquals("prefetch", headers.get("Sec-Purpose"));

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kActivated*/ 0);

        // Start prerendering and wait until completion. This should consume the prefetched
        // response.
        startPrerenderingAndWait(
                mPrerenderingUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                mActivationCallbackHelper.getCallback(),
                mPrerenderErrorCallbackHelper.getCallback());

        // Prerendering should consume the prefetched request. It shouldn't send a request.
        Assert.assertEquals(1, mTestServer.getRequestCountForUrl(PRERENDER_URL));

        activatePage(mPrerenderingUrl, ActivationBy.LOAD_URL);

        // Wait until the navigation activates the prerendered page.
        mActivationCallbackHelper.waitForNext();
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        // Activation shouldn't send a request.
        Assert.assertEquals(1, mTestServer.getRequestCountForUrl(PRERENDER_URL));
    }

    // Tests the following scenario:
    // 1) Prefetching sends a request to "?a=12". This response allows to ignore the parameter for
    //    the No-Vary-Search header.
    // 2) Prerendering starts for "?a=124". Thanks to the header, this should be able to consume the
    //    prefetched response, the parameter is different though.
    // 3) Navigation starts for "?a=91". Thanks to the header, this should be able to activate the
    //    prerendered page.
    //
    // In the end, a request should be sent only to "?a=12" one time.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchAndPrerenderWithNoVarySearchHeader() throws Throwable {
        loadInitialPage();

        // Start prefetching and wait until response completion. This response will have
        // `No-Vary-Search: params=("a")` header.
        String prefetchPath = PRERENDER_URL.concat("?a=12");
        String prefetchUrl = getUrl(prefetchPath);
        startPrefetchingAndWait(prefetchUrl, /* prefetchParameters= */ null);

        // Prefetching should send a request to "?a=12".
        Assert.assertEquals(1, mTestServer.getRequestCountForUrl(prefetchPath));

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kActivated*/ 0);

        // Start prerendering and wait until completion. This has the different query parameter but
        // should consume the prefetched response for the No-Vary-Search header.
        String prerenderPath = PRERENDER_URL.concat("?a=124");
        String prerenderUrl = getUrl(prerenderPath);
        startPrerenderingAndWait(
                prerenderUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                mActivationCallbackHelper.getCallback(),
                mPrerenderErrorCallbackHelper.getCallback());

        // Prerendering shouldn't send a request to "?a=12" or "?a=124".
        Assert.assertEquals(1, mTestServer.getRequestCountForUrl(prefetchPath));
        Assert.assertEquals(0, mTestServer.getRequestCountForUrl(prerenderPath));

        // Start navigation to the URL with yet another parameter. This should activate the
        // prerendered page.
        String navigationPath = PRERENDER_URL.concat("?a=91");
        String navigationUrl = getUrl(navigationPath);
        activatePage(navigationUrl, ActivationBy.LOAD_URL);

        // Wait until the navigation activates the prerendered page.
        mActivationCallbackHelper.waitForNext();
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        // Activation shouldn't send a request to "?a=12", "?a=124", or "?a=91".
        Assert.assertEquals(1, mTestServer.getRequestCountForUrl(prefetchPath));
        Assert.assertEquals(0, mTestServer.getRequestCountForUrl(prerenderPath));
        Assert.assertEquals(0, mTestServer.getRequestCountForUrl(navigationPath));
    }

    // Tests the case where prerendering is triggered for the same URL multiple times. Only one
    // prerendering navigation should happen.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testDuplicatePrerender_Success() throws Throwable {
        loadInitialPage();

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kActivated*/ 0);

        var activationCallbackHelper1 = new ActivationCallbackHelper();
        var activationCallbackHelper2 = new ActivationCallbackHelper();
        var errorCallbackHelper1 = new PrerenderErrorCallbackHelper();
        var errorCallbackHelper2 = new PrerenderErrorCallbackHelper();

        startPrerendering(
                mPrerenderingUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                activationCallbackHelper1.getCallback(),
                errorCallbackHelper1.getCallback());

        startPrerendering(
                mPrerenderingUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                activationCallbackHelper2.getCallback(),
                errorCallbackHelper2.getCallback());

        // Wait until the prerendered page is loaded.
        mPrerenderLifecycleWebMessageListener.waitForOnPostMessage();

        activatePage(mPrerenderingUrl, ActivationBy.LOAD_URL);

        // Wait until the navigation activates the prerendered page. Both the activation callbacks
        // should be called.
        activationCallbackHelper1.waitForNext();
        activationCallbackHelper2.waitForNext();
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        Assert.assertEquals(1, mTestServer.getRequestCountForUrl(PRERENDER_URL));
    }

    // Tests the case where prerendering is triggered for the same URL but different No-Vary-Search
    // hint. The first attempt should be canceled in favor of the second attempt, so the request
    // should be sent twice.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testDuplicatePrerender_SameUrlButDifferentNoVarySearchHint() throws Throwable {
        loadInitialPage();

        // Expect kActivated(0) and kTriggerDestroyed(16).
        var histogramWatcher = createFinalStatusHistogramWatcher(new int[] {0, 16});

        var activationCallbackHelper1 = new ActivationCallbackHelper();
        var activationCallbackHelper2 = new ActivationCallbackHelper();
        var errorCallbackHelper1 = new PrerenderErrorCallbackHelper();
        var errorCallbackHelper2 = new PrerenderErrorCallbackHelper();

        startPrerenderingAndWait(
                mPrerenderingUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                activationCallbackHelper1.getCallback(),
                errorCallbackHelper1.getCallback());

        String[] ignoredQueryParameters = {"a"};
        AwNoVarySearchData noVarySearchData =
                new AwNoVarySearchData(true, true, ignoredQueryParameters, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(
                        /* additionalHeaders= */ null,
                        noVarySearchData,
                        /* isJavascriptEnabled= */ true);

        startPrerenderingAndWait(
                mPrerenderingUrl,
                prefetchParameters,
                /* cancellationSignal= */ null,
                activationCallbackHelper2.getCallback(),
                errorCallbackHelper2.getCallback());

        activatePage(mPrerenderingUrl, ActivationBy.LOAD_URL);

        // Wait until the navigation activates the prerendered page. The second attempt should be
        // activated.
        activationCallbackHelper2.waitForNext();
        Assert.assertEquals(0, activationCallbackHelper1.getCallCount());

        // Both the attempts have the same URL but different No-Vary-Search hints, so they should
        // send the request separately.
        Assert.assertEquals(2, mTestServer.getRequestCountForUrl(PRERENDER_URL));

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests the case where prerendering is triggered for different URLs but the same No-Vary-Search
    // hint that can cover both the URLs. The second attempt should be deduped.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testDuplicatePrerender_DifferentUrlButSameNoVarySearchHint() throws Throwable {
        loadInitialPage();

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kActivate*/ 0);

        String url1 = getUrl(PRERENDER_URL.concat("?a=1"));
        String url2 = getUrl(PRERENDER_URL.concat("?a=2"));

        var activationCallbackHelper1 = new ActivationCallbackHelper();
        var activationCallbackHelper2 = new ActivationCallbackHelper();
        var errorCallbackHelper1 = new PrerenderErrorCallbackHelper();
        var errorCallbackHelper2 = new PrerenderErrorCallbackHelper();

        String[] ignoredQueryParameters = {"a"};
        AwNoVarySearchData noVarySearchData =
                new AwNoVarySearchData(true, true, ignoredQueryParameters, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(
                        /* additionalHeaders= */ null,
                        noVarySearchData,
                        /* isJavascriptEnabled= */ true);

        startPrerendering(
                url1,
                prefetchParameters,
                /* cancellationSignal= */ null,
                activationCallbackHelper1.getCallback(),
                errorCallbackHelper1.getCallback());

        startPrerendering(
                url2,
                prefetchParameters,
                /* cancellationSignal= */ null,
                activationCallbackHelper2.getCallback(),
                errorCallbackHelper2.getCallback());

        // Wait until the prerendered page is loaded.
        mPrerenderLifecycleWebMessageListener.waitForOnPostMessage();

        // Activate `mPrerenderingUrl` that matches both `url1` and `url2` with the No-Vary-Search
        // hint.
        activatePage(mPrerenderingUrl, ActivationBy.LOAD_URL);

        // Wait until the navigation activates the prerendered page. Both the activation callbacks
        // should be called.
        activationCallbackHelper1.waitForNext();
        activationCallbackHelper2.waitForNext();

        // The second attempt should be deduped, so the request should be sent only to `url1`.
        Assert.assertEquals(1, mTestServer.getRequestCountForUrl(PRERENDER_URL.concat("?a=1")));
        Assert.assertEquals(0, mTestServer.getRequestCountForUrl(PRERENDER_URL.concat("?a=2")));

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests the case where prerendering is triggered for the same URL multiple times and then
    // canceled by an unexpected error.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testDuplicatePrerender_UnexpectedError() throws Throwable {
        loadInitialPage();

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kAllPrerenderingCanceled*/ 81);

        var activationCallbackHelper1 = new ActivationCallbackHelper();
        var activationCallbackHelper2 = new ActivationCallbackHelper();
        var errorCallbackHelper1 = new PrerenderErrorCallbackHelper();
        var errorCallbackHelper2 = new PrerenderErrorCallbackHelper();

        startPrerendering(
                mPrerenderingUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                activationCallbackHelper1.getCallback(),
                errorCallbackHelper1.getCallback());

        startPrerendering(
                mPrerenderingUrl,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                activationCallbackHelper2.getCallback(),
                errorCallbackHelper2.getCallback());

        // Wait until the prerendered page is loaded.
        mPrerenderLifecycleWebMessageListener.waitForOnPostMessage();

        // Emulate cancellation by an unexpected error.
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.cancelAllPrerendering());

        // Wait until the prerendered page is canceled. Both the cancel callbacks should be called.
        errorCallbackHelper1.waitForNext();
        errorCallbackHelper2.waitForNext();
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests the case where prerendering is triggered for the same URL multiple times and then
    // canceled by CancellationSignal.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testDuplicatePrerender_CancellationSignal() throws Throwable {
        loadInitialPage();

        var histogramWatcher = createFinalStatusHistogramWatcher(/*kTriggerDestroyed*/ 16);

        var cancellationSignal1 = new CancellationSignal();
        var cancellationSignal2 = new CancellationSignal();
        var activationCallbackHelper1 = new ActivationCallbackHelper();
        var activationCallbackHelper2 = new ActivationCallbackHelper();
        var errorCallbackHelper1 = new PrerenderErrorCallbackHelper();
        var errorCallbackHelper2 = new PrerenderErrorCallbackHelper();

        startPrerendering(
                mPrerenderingUrl,
                /* prefetchParameters= */ null,
                cancellationSignal1,
                activationCallbackHelper1.getCallback(),
                errorCallbackHelper1.getCallback());

        startPrerendering(
                mPrerenderingUrl,
                /* prefetchParameters= */ null,
                cancellationSignal2,
                activationCallbackHelper2.getCallback(),
                errorCallbackHelper2.getCallback());

        Assert.assertFalse(cancellationSignal1.isCanceled());
        Assert.assertFalse(cancellationSignal2.isCanceled());

        // Cancel prerendering 2 using CancellationSignal. This is an intentional cancellation, so
        // the error callbacks should not be called.
        cancellationSignal2.cancel();

        Assert.assertFalse(cancellationSignal1.isCanceled());
        Assert.assertTrue(cancellationSignal2.isCanceled());

        // Wait until the prerendered page is canceled.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        // The error callbacks should not be called.
        Assert.assertEquals(0, errorCallbackHelper1.getCallCount());
        Assert.assertEquals(0, errorCallbackHelper2.getCallCount());

        // Calling the other cancellation signal after cancellation should not cause a runtime
        // error.
        cancellationSignal1.cancel();
        Assert.assertTrue(cancellationSignal1.isCanceled());
    }

    // Tests the case where prerendering is triggered for different URLs.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testMultiplePrerenderingSuccess() throws Throwable {
        loadInitialPage();

        // Expect kActivated(0) and kOtherPrerenderedPageActivated(84).
        var histogramWatcher = createFinalStatusHistogramWatcher(new int[] {0, 84});

        var activationCallbackHelper1 = new ActivationCallbackHelper();
        var activationCallbackHelper2 = new ActivationCallbackHelper();
        var errorCallbackHelper1 = new PrerenderErrorCallbackHelper();
        var errorCallbackHelper2 = new PrerenderErrorCallbackHelper();

        String prerenderingUrl1 = getUrl(PRERENDER_URL + "?b=12");
        String prerenderingUrl2 = getUrl(PRERENDER_URL + "?b=124");

        startPrerenderingAndWait(
                prerenderingUrl1,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                activationCallbackHelper1.getCallback(),
                errorCallbackHelper1.getCallback());

        startPrerenderingAndWait(
                prerenderingUrl2,
                /* prefetchParameters= */ null,
                /* cancellationSignal= */ null,
                activationCallbackHelper2.getCallback(),
                errorCallbackHelper2.getCallback());

        // Activate prerendering 1. Prerendering 2 should be canceled during activation.
        activatePage(prerenderingUrl1, ActivationBy.LOAD_URL);

        // Wait until the navigation activates the prerendered page.
        activationCallbackHelper1.waitForNext();

        // The error callback for prerendering 1 is never called in the case where other
        // prerendering is activated.
        Assert.assertEquals(0, errorCallbackHelper2.getCallCount());

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests the case where prerendering is triggered for different URLs with changing the max
    // prerender limit.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testMultiplePrerenderingLimit() throws Throwable {
        loadInitialPage();

        // This test will start prerendering 5 times.
        int numberOfPrerenders = 5;

        var activationCallbackHelpers = new ActivationCallbackHelper[numberOfPrerenders];
        for (int i = 0; i < numberOfPrerenders; ++i) {
            activationCallbackHelpers[i] = new ActivationCallbackHelper();
        }
        var errorCallbackHelpers = new PrerenderErrorCallbackHelper[numberOfPrerenders];
        for (int i = 0; i < numberOfPrerenders; ++i) {
            errorCallbackHelpers[i] = new PrerenderErrorCallbackHelper();
        }
        String[] prerenderingUrls = new String[numberOfPrerenders];
        for (int i = 0; i < numberOfPrerenders; ++i) {
            prerenderingUrls[i] = getUrl(PRERENDER_URL + "?b=" + i);
        }

        // Expect 1 kActivated(0), 3 kTriggerDestroyed(16), and 1 kOtherPrerenderedPageActivated(84)
        // in the end.
        var histogramWatcherAll =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(FINAL_STATUS_UMA, 0, 1)
                        .expectIntRecordTimes(FINAL_STATUS_UMA, 16, 3)
                        .expectIntRecordTimes(FINAL_STATUS_UMA, 84, 1)
                        .build();

        // Start with 2 slots for startPrerendering().
        setMaxPrerenders(2);

        // Current status:
        //   MaxPrerenders = 2
        //   Prerendered   = []
        //   Cancelled     = []

        {
            // Start prerendering from 0 to 2. Prerendering 0 should be canceled with
            // kTriggerDestroyed(16), as the current limit is 2.
            var histogramWatcher = createFinalStatusHistogramWatcher(16);
            for (int i = 0; i < 3; ++i) {
                startPrerendering(
                        prerenderingUrls[i],
                        /* prefetchParameters= */ null,
                        /* cancellationSignal= */ null,
                        activationCallbackHelpers[i].getCallback(),
                        errorCallbackHelpers[i].getCallback());
            }
            histogramWatcher.assertExpected();
        }

        // Current status:
        //   MaxPrerenders = 2
        //   Prerendered   = [1, 2]
        //   Cancelled     = [0]

        // Set max prerenders to 3. This allocates one more slot for startPrerendering().
        setMaxPrerenders(3);

        {
            // Start prerendering 3. No prerendering should be canceled.
            var histogramWatcher =
                    HistogramWatcher.newBuilder().expectNoRecords(FINAL_STATUS_UMA).build();
            startPrerendering(
                    prerenderingUrls[3],
                    /* prefetchParameters= */ null,
                    /* cancellationSignal= */ null,
                    activationCallbackHelpers[3].getCallback(),
                    errorCallbackHelpers[3].getCallback());
            histogramWatcher.assertExpected();
        }

        // Current status:
        //   MaxPrerenders = 3
        //   Prerendered   = [1, 2, 3]
        //   Cancelled     = [0]

        {
            // Set max prerenders to 2. This is smaller than the number of ongoing prerendering (3),
            // but it doesn't cancel them now and instead defers it until startPrerendering() is
            // called.
            var histogramWatcher =
                    HistogramWatcher.newBuilder().expectNoRecords(FINAL_STATUS_UMA).build();
            setMaxPrerenders(2);
            histogramWatcher.assertExpected();
        }

        // Current status:
        //   MaxPrerenders = 2
        //   Prerendered   = [1, 2, 3]
        //   Cancelled     = [0]

        {
            // Start prerendering 4. Prerendering 1 and 2 should be canceled with
            // kTriggerDestroyed(16), as the current limit is 2.
            var histogramWatcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecordTimes(FINAL_STATUS_UMA, 16, 2)
                            .build();
            startPrerendering(
                    prerenderingUrls[4],
                    /* prefetchParameters= */ null,
                    /* cancellationSignal= */ null,
                    activationCallbackHelpers[4].getCallback(),
                    errorCallbackHelpers[4].getCallback());
            histogramWatcher.assertExpected();
        }

        // Current status:
        //   MaxPrerenders = 2
        //   Prerendered   = [3, 4]
        //   Cancelled     = [0, 1, 2]

        {
            // Activate prerendering 3. Prerendering 4 should be canceled with
            // kOtherPrerenderedPageActivated(84) during activation.
            var histogramWatcher = createFinalStatusHistogramWatcher(new int[] {0, 84});
            activatePage(prerenderingUrls[3], ActivationBy.LOAD_URL);

            // Wait until the navigation activates the prerendered page.
            activationCallbackHelpers[3].waitForNext();

            // The error callbacks are never called in the case where prerendering is canceled for
            // the limit or other prerendering is activated.
            for (int i = 0; i < numberOfPrerenders; ++i) {
                Assert.assertEquals(0, errorCallbackHelpers[i].getCallCount());
            }
            histogramWatcher.assertExpected();
        }

        // Current status:
        //   MaxPrerenders = 2
        //   Prerendered  = []
        //   Cancelled    = [0, 1, 2] (kTriggerDestroyed = 16)
        //   NotActivated = [4]       (kOtherPrerenderedPageActivated = 84)
        //   Activated    = [3]       (kActivated = 0)

        histogramWatcherAll.assertExpected();
    }
}
