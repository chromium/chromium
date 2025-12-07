// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserContextStore;
import org.chromium.android_webview.AwContents;
import org.chromium.base.ChildBindingState;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.ChildProcessUtils;
import org.chromium.content_public.browser.test.util.RenderProcessHostUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;

@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests focus on manipulation of global profile state")
public class SpareRendererTest extends AwParameterizedTest {
    @Rule public MultiProfileTestRule mRule;

    private final TestAwContentsClient mContentsClient;

    public SpareRendererTest(AwSettingsMutation param) {
        this.mRule = new MultiProfileTestRule(param.getMutation());
        this.mContentsClient = mRule.getContentsClient();
    }

    @Test
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSpareProcessUsed() throws Throwable {
        mRule.startBrowserProcess();
        assertEquals(0, RenderProcessHostUtils.getCurrentRenderProcessCount());
        assertEquals(0, RenderProcessHostUtils.getSpareRenderProcessHostCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> AwBrowserContext.getDefault().warmUpSpareRenderer());

        assertEquals(0, RenderProcessHostUtils.getCurrentRenderProcessCount());
        assertEquals(1, RenderProcessHostUtils.getSpareRenderProcessHostCount());

        final AwContents firstAwContents = mRule.createAwContents();
        TestWebServer webServer = TestWebServer.start();
        String path = "/test.html";
        String responseStr = "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
        String url = webServer.setResponse(path, responseStr, new ArrayList<>());
        mRule.loadUrlSync(firstAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());
        assertEquals(0, RenderProcessHostUtils.getSpareRenderProcessHostCount());

        webServer.shutdown();
    }

    @Test
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testProcessCountLimit() throws Throwable {
        mRule.startBrowserProcess();
        final AwContents firstAwContents = mRule.createAwContents();
        TestWebServer webServer = TestWebServer.start();
        String path = "/test.html";
        String responseStr = "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
        String url = webServer.setResponse(path, responseStr, new ArrayList<>());
        mRule.loadUrlSync(firstAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());
        assertEquals(0, RenderProcessHostUtils.getSpareRenderProcessHostCount());

        // The call should not start a new process as we are currently limited to one renderer per
        // browser context.
        ThreadUtils.runOnUiThreadBlocking(
                () -> AwBrowserContext.getDefault().warmUpSpareRenderer());
        assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());
        assertEquals(0, RenderProcessHostUtils.getSpareRenderProcessHostCount());

        webServer.shutdown();
    }

    @Test
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "enable-features=SpareRendererProcessPriority:not-perceptible-binding/true"
    })
    public void testProcessBindingState() throws Throwable {
        mRule.startBrowserProcess();
        assertEquals(0, RenderProcessHostUtils.getCurrentRenderProcessCount());
        assertEquals(0, RenderProcessHostUtils.getSpareRenderProcessHostCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> AwBrowserContext.getDefault().warmUpSpareRenderer());
        assertEquals(0, RenderProcessHostUtils.getCurrentRenderProcessCount());
        assertEquals(1, RenderProcessHostUtils.getSpareRenderProcessHostCount());

        // The renderer process is started asynchronously. We need to wait until the process is
        // ready to check the binding state.
        AwActivityTestRule.pollInstrumentationThread(
                () -> RenderProcessHostUtils.isSpareRenderReady());

        // The binding state is recalculated multiple times after the renderer is launched. Wait
        // for one second for the binding state to settle down.
        Thread.sleep(1100);
        assertEquals(
                ChildBindingState.NOT_PERCEPTIBLE,
                RenderProcessHostUtils.getSpareRenderBindingState());
    }

    @Test
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testChildConnectionUsedBySpareRenderer() throws Throwable {
        // We start a child connection during browser initialization.
        mRule.startBrowserProcess();
        assertEquals(1, ChildProcessUtils.getConnectedSandboxedServicesCount());
        // Creating a non-default profile creates a spare renderer that is expected to reuse the
        // pre-warmed child connection.
        ThreadUtils.runOnUiThreadBlocking(
                () -> AwBrowserContextStore.getNamedContext("NonDefault", true));
        assertEquals(1, ChildProcessUtils.getConnectedSandboxedServicesCount());
    }
}
