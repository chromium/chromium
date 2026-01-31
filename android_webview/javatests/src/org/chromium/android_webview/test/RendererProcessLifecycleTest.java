// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.base.ChildBindingState;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentUrlConstants;

@RunWith(AwJUnit4ClassRunner.class)
@DoNotBatch(reason = "Renderer process state is sensitive to process restarts")
public class RendererProcessLifecycleTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS)
    @CommandLineFlags.Add("enable-features=WebViewRendererKeepAlive")
    public void testRendererKeptAliveAtWaivedPriorityAfterWebViewDestroy() throws Throwable {
        // Load a page to ensure renderer is started
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        AwRenderProcess renderProcess =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getRenderProcess());

        Assert.assertNotNull(renderProcess);
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(() -> renderProcess.isReadyForTesting()));

        // Check binding state before destruction (should be > WAIVED)
        int stateBefore =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> renderProcess.getEffectiveChildBindingStateForTesting());
        Assert.assertTrue(
                "Binding state should be > WAIVED but was " + stateBefore,
                stateBefore > ChildBindingState.WAIVED);

        // Destroy the WebView
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestContainerView.removeAllViews();
                    mAwContents.destroy();
                });

        // Verify that the renderer process is kept alive.
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(() -> renderProcess.isReadyForTesting()));

        // Verify binding state drops to WAIVED now that there are no active WebViews.
        mActivityTestRule.pollUiThread(
                () -> {
                    int state = renderProcess.getEffectiveChildBindingStateForTesting();
                    return state == ChildBindingState.WAIVED;
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS)
    @CommandLineFlags.Add("enable-features=WebViewRendererKeepAlive")
    public void testRendererReusedAndBindingStateRestored() throws Throwable {
        // Load a page to ensure renderer is started
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        AwRenderProcess renderProcess =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getRenderProcess());

        Assert.assertNotNull(renderProcess);

        // Destroy the WebView
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestContainerView.removeAllViews();
                    mAwContents.destroy();
                });

        // Verify binding state drops to WAIVED now that there are no active WebViews.
        mActivityTestRule.pollUiThread(
                () -> {
                    int state = renderProcess.getEffectiveChildBindingStateForTesting();
                    return state == ChildBindingState.WAIVED;
                });

        // Create a new WebView
        TestAwContentsClient newContentsClient = new TestAwContentsClient();
        AwTestContainerView newTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(newContentsClient);
        AwContents newAwContents = newTestContainerView.getAwContents();

        // Load a page in the new WebView
        mActivityTestRule.loadUrlSync(
                newAwContents,
                newContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        AwRenderProcess newRenderProcess =
                ThreadUtils.runOnUiThreadBlocking(() -> newAwContents.getRenderProcess());

        // Verify that the same renderer process is reused
        Assert.assertSame("Renderer process should be reused", renderProcess, newRenderProcess);

        // Verify binding state is restored (should be > WAIVED)
        mActivityTestRule.pollUiThread(
                () -> {
                    int state = renderProcess.getEffectiveChildBindingStateForTesting();
                    return state > ChildBindingState.WAIVED;
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS)
    @CommandLineFlags.Add(
            "enable-features=WebViewRendererKeepAlive:webview_renderer_keep_alive_duration/2s")
    public void testRendererKeepAliveDurationRespected() throws Throwable {
        // Load a page to ensure renderer is started
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        AwRenderProcess renderProcess =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getRenderProcess());

        Assert.assertNotNull(renderProcess);

        // Destroy the WebView
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestContainerView.removeAllViews();
                    mAwContents.destroy();
                });

        // Verify that the renderer process is kept alive initially.
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(() -> renderProcess.isReadyForTesting()));

        // Verify binding state drops to WAIVED now that there are no active WebViews.
        mActivityTestRule.pollUiThread(
                () -> {
                    int state = renderProcess.getEffectiveChildBindingStateForTesting();
                    return state == ChildBindingState.WAIVED;
                });

        // Wait for the renderer process to terminate after the duration.
        // pollUiThread waits up to 15s (WAIT_TIMEOUT_MS), which is sufficient for 2s duration.
        mActivityTestRule.pollUiThread(
                () -> {
                    // isReadyForTesting() returns false when the process has exited/died.
                    return !renderProcess.isReadyForTesting();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS)
    public void testRendererTerminatedThenWebViewDestroyed() throws Throwable {
        // Load a page to ensure renderer is started
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Trigger a renderer crash
        TestAwContentsClient.RenderProcessGoneHelper helper =
                mContentsClient.getRenderProcessGoneHelper();
        helper.setResponse(true); // Don't automatically kill the browser process.
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.loadUrl("chrome://kill"));

        // Load a page to ensure new renderer is started
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Re-creates a new AwRenderProcess instance.
        AwRenderProcess newRenderProcess =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getRenderProcess());

        // Destroy the WebView to check that aw_contents destructor doesn't trigger a crash.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestContainerView.removeAllViews();
                    mAwContents.destroy();
                });
    }
}
