// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.annotation.SuppressLint;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.settings.SpeculativeLoadingAllowedFlags;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.js_injection.mojom.DocumentInjectionTime;
import org.chromium.net.test.EmbeddedTestServer.ServerHTTPSSetting;
import org.chromium.net.test.ServerCertificate;

/** Tests for WebMessageListener interaction with Prerendering. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
public class WebMessageListenerPrerenderTest extends AwParameterizedTest {

    private static final String SERVER_DATA_DIR = "android_webview/test/data";
    private static final String INDEX_PATH = "/prerender-webmessage-index.html";
    private static final String PRERENDERED_PATH = "/prerender-webmessage-prerendered.html";
    private static final String JAVASCRIPT_WORLD_NAME = "app_world";

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwContents mAwContents;
    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private AwEmbeddedTestServer mTestServer;

    public WebMessageListenerPrerenderTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mAwContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mTestServer = new AwEmbeddedTestServer();
        // Prerendering requires pages to be loaded over HTTPS.
        mTestServer.initializeNative(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerHTTPSSetting.USE_HTTPS);
        mTestServer.addDefaultHandlers(SERVER_DATA_DIR);
        mTestServer.setSSLConfig(ServerCertificate.CERT_OK);
        mTestServer.start();
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    @SuppressLint("VisibleForTests")
    @Test
    @LargeTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testSharedPrerenderQueueMisroutesAcrossListeners() throws Throwable {
        final TestWebMessageListener listenerA = new TestWebMessageListener();
        final TestWebMessageListener listenerB = new TestWebMessageListener();
        final TestWebMessageListener listenerWorld = new TestWebMessageListener();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents
                            .getSettings()
                            .setSpeculativeLoadingAllowed(
                                    SpeculativeLoadingAllowedFlags.PRERENDER_ENABLED);
                    mAwContents.getWebContents().updateWebContentsVisibility(Visibility.VISIBLE);

                    mAwContents.addWebMessageListener("aListener", new String[] {"*"}, listenerA);
                    mAwContents.addWebMessageListener("bListener", new String[] {"*"}, listenerB);

                    mAwContents.registerJavaScriptWorld(JAVASCRIPT_WORLD_NAME);
                    mAwContents.addWebMessageListener(
                            "worldListener",
                            new String[] {"*"},
                            listenerWorld,
                            JAVASCRIPT_WORLD_NAME);

                    mAwContents.addJavaScriptOnEvent(
                            """
                            if (document.prerendering) {
                              worldListener.postMessage('msg-for-world');
                            }
                            """,
                            DocumentInjectionTime.DOCUMENT_START,
                            new String[] {"*"},
                            JAVASCRIPT_WORLD_NAME);
                });

        // Load a page that has speculation rules to prerender the PRERENDERED_PATH page.
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                mTestServer.getURL(INDEX_PATH));

        // Wait for both main and prerendered page to send a console message. This indicates they
        // are loaded and running JavaScript.
        mContentsClient.getAddMessageToConsoleHelper().waitForCallback(0, 2);

        // Activate the prerendered page. This should trigger all the messages in the queue to be
        // sent to the app.
        mActivityTestRule.loadUrlAsync(mAwContents, mTestServer.getURL(PRERENDERED_PATH));

        // Now check messages.
        TestWebMessageListener.Data dataA = listenerA.waitForOnPostMessage();
        Assert.assertEquals("msg-for-a", dataA.getAsString());
        Assert.assertTrue(listenerA.hasNoMoreOnPostMessage());

        TestWebMessageListener.Data dataB = listenerB.waitForOnPostMessage();
        Assert.assertEquals("msg-for-b", dataB.getAsString());
        Assert.assertTrue(listenerB.hasNoMoreOnPostMessage());

        TestWebMessageListener.Data dataWorld = listenerWorld.waitForOnPostMessage();
        Assert.assertEquals("msg-for-world", dataWorld.getAsString());
        Assert.assertTrue(listenerWorld.hasNoMoreOnPostMessage());
    }
}
