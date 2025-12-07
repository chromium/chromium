// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.LargeTest;
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
import org.chromium.android_webview.AwPrefetchManager;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.ServerCertificate;

import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests for shouldInterceptRequest behavior with various prefetch mechanisms. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
public class AwPrefetchInterceptionTest extends AwParameterizedTest {

    @Rule public AwActivityTestRule mActivityTestRule;

    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private AwContents mAwContents;
    private AwEmbeddedTestServer mTestServer;
    private AwBrowserContext mBrowserContext;

    private static final String INITIAL_URL = "/android_webview/test/data/hello_world.html";
    private static final String PREFETCH_URL = "/android_webview/test/data/prefetch.html";

    private String mInitialUrl;
    private String mPrefetchUrl;

    public AwPrefetchInterceptionTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mBrowserContext = mActivityTestRule.getAwBrowserContext();

        mTestServer =
                AwEmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_TEST_NAMES);

        mInitialUrl = getURL(INITIAL_URL);
        mPrefetchUrl = getURL(PREFETCH_URL);
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    // Returns a URL. This requires ContentSwitches.HOST_RESOLVER_RULES.
    private String getURL(final String relativeUrl) {
        return mTestServer.getURLWithHostName("a.test", relativeUrl);
    }

    private void loadInitialPage() throws Exception {
        // Load an initial page that will be triggering the speculation rules prefetch.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);

        // Wait for onPageStarted for initial page load.
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        onPageStartedHelper.waitForCallback(
                0, 1, AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onPageStartedHelper.getUrl(), mInitialUrl);
    }

    // Triggers a speculation rules prefetch.
    private void triggerSpeculationRulesPrefetchAndWait(String targetUrl) {
        final String speculationRulesTemplate =
                """
                    {
                    const script = document.createElement('script');
                    script.type = 'speculationrules';
                    script.text = '{"prefetch": [{"urls": ["%s"]}]}';
                    document.head.appendChild(script);
                    }
                """;
        final String speculationRules = String.format(speculationRulesTemplate, targetUrl);

        // Start prefetching from the initial page.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.evaluateJavaScript(speculationRules, null);
                });

        // We need to wait in order to give time for the prefetch request
        // to execute.
        try {
            Thread.sleep(500);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    // Triggers a <link rel="prefetch"> prefetch.
    private void triggerLinkRelPrefetchAndWait(String targetUrl) {
        // Inject the prefetch link into the loaded page.
        final String script =
                "var link = document.createElement('link'); "
                        + "link.rel = 'prefetch'; "
                        + "link.href = '"
                        + targetUrl
                        + "'; "
                        + "document.head.appendChild(link);";
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.evaluateJavaScript(script, null));

        // We need to wait in order to give time for the prefetch request
        // to execute.
        try {
            Thread.sleep(500);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    private void triggerEmbedderPrefetchAndWait(String targetUrl) throws Exception {
        AwPrefetchTest.TestAwPrefetchCallback callback =
                new AwPrefetchTest.TestAwPrefetchCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwPrefetchManager prefetchManager = mBrowserContext.getPrefetchManager();
                    Executor directExecutor = Runnable::run;
                    int prefetchKey =
                            prefetchManager.startPrefetchRequest(
                                    targetUrl, null, callback, directExecutor);
                    callback.setPrefetchKey(prefetchKey);
                });
        callback.getOnStatusUpdatedHelper().waitForNext();
    }

    private void assertShouldInterceptRequestCalled(
            TestAwContentsClient.ShouldInterceptRequestHelper helper,
            String targetUrl,
            int previousCallCountForUrl)
            throws TimeoutException {
        helper.waitForCallback(previousCallCountForUrl);
        Assert.assertEquals(
                "shouldInterceptRequest was not called for " + targetUrl,
                previousCallCountForUrl + 1,
                helper.getRequestCountForUrl(targetUrl));
    }

    private void assertShouldInterceptRequestNotCalled(
            TestAwContentsClient.ShouldInterceptRequestHelper helper, String targetUrl) {
        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        Assert.assertEquals(
                "shouldInterceptRequest WAS called for " + targetUrl + " but was NOT expected.",
                0,
                helper.getRequestCountForUrl(targetUrl));
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures(AwFeatures.WEBVIEW_SKIP_INTERCEPTS_FOR_PREFETCH)
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testLinkRelPrefetch_InterceptionEnabled_FeatureDisabled() throws Throwable {
        loadInitialPage();
        TestAwContentsClient.ShouldInterceptRequestHelper interceptHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        int previousCountForUrl = interceptHelper.getRequestCountForUrl(mPrefetchUrl);
        triggerLinkRelPrefetchAndWait(mPrefetchUrl);
        assertShouldInterceptRequestCalled(interceptHelper, mPrefetchUrl, previousCountForUrl);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures(AwFeatures.WEBVIEW_SKIP_INTERCEPTS_FOR_PREFETCH)
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testLinkRelPrefetch_InterceptionEnabled_FeatureEnabled() throws Throwable {
        loadInitialPage();
        TestAwContentsClient.ShouldInterceptRequestHelper interceptHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        int previousCountForUrl = interceptHelper.getRequestCountForUrl(mPrefetchUrl);
        triggerLinkRelPrefetchAndWait(mPrefetchUrl);
        assertShouldInterceptRequestCalled(interceptHelper, mPrefetchUrl, previousCountForUrl);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures(AwFeatures.WEBVIEW_SKIP_INTERCEPTS_FOR_PREFETCH)
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testSpeculationRulesPrefetch_InterceptionEnabled_FeatureDisabled()
            throws Throwable {
        loadInitialPage();
        TestAwContentsClient.ShouldInterceptRequestHelper interceptHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        int previousCountForUrl = interceptHelper.getRequestCountForUrl(mPrefetchUrl);
        triggerSpeculationRulesPrefetchAndWait(mPrefetchUrl);
        assertShouldInterceptRequestCalled(interceptHelper, mPrefetchUrl, previousCountForUrl);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures(AwFeatures.WEBVIEW_SKIP_INTERCEPTS_FOR_PREFETCH)
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testSpeculationRulesPrefetch_InterceptionEnabled_FeatureEnabled() throws Throwable {
        loadInitialPage();
        TestAwContentsClient.ShouldInterceptRequestHelper interceptHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        int previousCountForUrl = interceptHelper.getRequestCountForUrl(mPrefetchUrl);
        triggerSpeculationRulesPrefetchAndWait(mPrefetchUrl);
        assertShouldInterceptRequestCalled(interceptHelper, mPrefetchUrl, previousCountForUrl);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testEmbedderPrefetch_InterceptionSkipped() throws Throwable {
        loadInitialPage();
        TestAwContentsClient.ShouldInterceptRequestHelper interceptHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        triggerEmbedderPrefetchAndWait(mPrefetchUrl);
        assertShouldInterceptRequestNotCalled(interceptHelper, mPrefetchUrl);
    }
}
