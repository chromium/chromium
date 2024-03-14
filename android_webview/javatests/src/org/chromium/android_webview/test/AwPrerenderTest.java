// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.TestAwContentsClient.OnLoadResourceHelper;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.net.test.EmbeddedTestServer;

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

    // Tests basic end-to-end behavior of speculation rules prerendering on WebView.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({AwFeatures.WEBVIEW_PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testSpeculationRulesPrerendering() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());

        // Load an initial page that will be triggering speculation rules prerendering.
        final String pageUrl = testServer.getURL(HELLO_WORLD_URL);
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

        // Wait for onPageStarted for the initial page load.
        final OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        onPageStartedHelper.waitForCallback(0, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onPageStartedHelper.getUrl(), pageUrl);

        // Prepare speculation rules script.
        final String prerenderingUrl = pageUrl + "?prerender";
        final String speculationRulesTemplate =
                """
            {
              const script = document.createElement('script');
              script.type = 'speculationrules';
              script.text = '{"prerender": [{"source": "list", "urls": ["%s"]}]}';
              document.head.appendChild(script);
            }
        """;
        final String speculationRules = String.format(speculationRulesTemplate, prerenderingUrl);

        final OnLoadResourceHelper onLoadResourceHelper = mContentsClient.getOnLoadResourceHelper();
        int currentOnLoadResourceCallCount = onLoadResourceHelper.getCallCount();

        // Start prerendering from the initial page.
        mActivityTestRule.runOnUiThread(
                () -> {
                    awContents.evaluateJavaScript(speculationRules, null);
                });

        // Wait for prerendering navigation. Monitor onLoadResource instead of onPageFinished as
        // onPageFinished is never called during prerendering (deferred until activation).
        onLoadResourceHelper.waitForCallback(
                currentOnLoadResourceCallCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onLoadResourceHelper.getLastLoadedResource(), prerenderingUrl);

        // onPageStarted should never be called for prerender initial navigation.
        Assert.assertEquals(onPageStartedHelper.getCallCount(), 1);
        Assert.assertEquals(onPageStartedHelper.getUrl(), pageUrl);

        // Activate the prerendered page.
        mActivityTestRule.runOnUiThread(
                () -> {
                    final String activationScript =
                            String.format("location.href = `%s`;", prerenderingUrl);
                    awContents.evaluateJavaScript(activationScript, null);
                });

        // Wait until the page is activated. The expected call count of onPageStarted should be 2 in
        // total for the initial page load and the prerender activation.
        onPageStartedHelper.waitForCallback(1, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(onPageStartedHelper.getUrl(), prerenderingUrl);
    }
}
