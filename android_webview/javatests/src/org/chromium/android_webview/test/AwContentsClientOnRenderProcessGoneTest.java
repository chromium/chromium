// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.util.Pair;

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
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.renderer_priority.RendererPriority;
import org.chromium.base.BaseSwitches;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for AwContentsClient.onRenderProcessGone callback. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsClientOnRenderProcessGoneTest extends AwParameterizedTest {
    private static final String TAG = "AwRendererGone";
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestWebServer mWebServer;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestView;
    private AwContents mAwContents;

    public AwContentsClientOnRenderProcessGoneTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
        mContentsClient = new TestAwContentsClient();
        mTestView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestView.getAwContents();
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    private String addPageToTestServer(String httpPath, String html) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html"));
        headers.add(Pair.create("Cache-Control", "no-store"));
        return mWebServer.setResponse(httpPath, html, headers);
    }

    interface Terminator {
        void terminate();
    }

    private AwRenderProcess createAndTerminateRenderProcess(
            Terminator terminator, boolean expectCrash) throws Throwable {
        TestAwContentsClient.RenderProcessGoneHelper helper =
                mContentsClient.getRenderProcessGoneHelper();
        helper.setResponse(true); // Don't automatically kill the browser process.

        final AwRenderProcess renderProcess =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getRenderProcess());

        // Ensure that the renderer has started.
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Terminate the renderer.
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> terminator.terminate());

        // Assert that onRenderProcessGone is called once.
        int callCount = helper.getCallCount();
        helper.waitForCallback(
                callCount, 1, CallbackHelper.WAIT_TIMEOUT_SECONDS * 5, TimeUnit.SECONDS);
        Assert.assertEquals(callCount + 1, helper.getCallCount());
        Assert.assertEquals(helper.getAwRenderProcessGoneDetail().didCrash(), expectCrash);
        Assert.assertEquals(
                RendererPriority.HIGH, helper.getAwRenderProcessGoneDetail().rendererPriority());

        return renderProcess;
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testOnRenderProcessCrash() throws Throwable {
        createAndTerminateRenderProcess(
                () -> {
                    mAwContents.loadUrl("chrome://crash");
                },
                true);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testOnRenderProcessKill() throws Throwable {
        createAndTerminateRenderProcess(
                () -> {
                    mAwContents.loadUrl("chrome://kill");
                },
                false);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testRenderProcessTermination() throws Throwable {
        createAndTerminateRenderProcess(
                () -> {
                    mAwContents.getRenderProcess().terminate();
                },
                false);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testRenderProcessDifferentAfterRestart() throws Throwable {
        AwRenderProcess renderProcess1 =
                createAndTerminateRenderProcess(
                        () -> {
                            mAwContents.getRenderProcess().terminate();
                        },
                        false);
        AwRenderProcess renderProcess2 =
                createAndTerminateRenderProcess(
                        () -> {
                            mAwContents.getRenderProcess().terminate();
                        },
                        false);
        Assert.assertNotEquals(renderProcess1, renderProcess2);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testRenderProcessCanNotTerminateBeforeStart() throws Throwable {
        Assert.assertFalse(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mAwContents.getRenderProcess().terminate()));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testRenderProcessSameBeforeAndAfterStart() throws Throwable {
        AwRenderProcess renderProcessBeforeStart =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getRenderProcess());

        // Ensure that the renderer has started.
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        AwRenderProcess renderProcessAfterStart =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getRenderProcess());

        Assert.assertEquals(renderProcessBeforeStart, renderProcessAfterStart);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    // The RenderDocument feature has a "level" parameter. This enables the feature and sets the
    // default level to "crashed-frame" to enable replacing the RenderFrameHost of crashed frames
    // with a new RenderFrameHost (instead of resuing the old one). See
    // https://go/force-field-trials-docs for the syntax of these flags.
    @CommandLineFlags.Add({
        "enable-features=RenderDocument<RenderDocument",
        BaseSwitches.FORCE_FIELD_TRIALS + "=RenderDocument/Group1",
        BaseSwitches.FORCE_FIELD_TRIAL_PARAMS + "=RenderDocument.Group1:level/crashed-frame",
    })
    public void testNavigationAfterCrashAndJavaScript() throws Throwable {
        // In https://crbug.com/1006814, a crashed frame, reinitialized by running JS fails to
        // navigate.
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        // Crash the frame.
        createAndTerminateRenderProcess(
                () -> {
                    mAwContents.getRenderProcess().terminate();
                },
                false);
        // Run JS in the frame.
        Assert.assertEquals(
                "3",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "1+2"));
        // Navigate to somewhere else. This should not crash the frame.
        final String content = "some content";
        final String httpPathOnServer = addPageToTestServer("/page.html", content);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), httpPathOnServer);
        // Result is a string, so needs "".
        Assert.assertEquals(
                "\"" + content + "\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "document.body.textContent"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @CommandLineFlags.Add({
        "enable-features=CreateSpareRendererOnBrowserContextCreation"
                + ":create_spare_renderer_for_default_if_multi_profile/true"
    })
    public void testTerminateBeforeRenderProcessCreated() throws Throwable {
        AwRenderProcess process =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getRenderProcess());
        mActivityTestRule.pollUiThread(() -> process.isReadyForTesting());
        ThreadUtils.runOnUiThreadBlocking(() -> Assert.assertFalse(process.terminate()));

        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertSame(process, mAwContents.getRenderProcess()));

        // Terminating future renderers works as expected.
        createAndTerminateRenderProcess(
                () -> Assert.assertTrue(mAwContents.getRenderProcess().terminate()), false);
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @CommandLineFlags.Add({
        "enable-features=CreateSpareRendererOnBrowserContextCreation"
                + ":create_spare_renderer_for_default_if_multi_profile/true"
    })
    public void testSetNetworkAvailableAfterSpareRenderTerminate() throws Throwable {
        AwRenderProcess process =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getRenderProcess());
        mActivityTestRule.pollUiThread(() -> process.isReadyForTesting());
        ThreadUtils.runOnUiThreadBlocking(() -> Assert.assertFalse(process.terminate()));

        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        String script = "navigator.onLine";
        Assert.assertEquals(
                "true",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, script));

        AwActivityTestRule.setNetworkAvailableOnUiThread(mAwContents, false);
        Assert.assertEquals(
                "false",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, script));
    }
}
