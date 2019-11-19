// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.AwRenderProcessGoneDetail;
import org.chromium.android_webview.renderer_priority.RendererPriority;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.concurrent.TimeUnit;

/**
 * Tests for AwContentsClient.onRenderProcessGone callback.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsClientOnRenderProcessGoneTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static final String TAG = "AwRendererGone";
    private static class GetRenderProcessGoneHelper extends CallbackHelper {
        private AwRenderProcessGoneDetail mDetail;

        public AwRenderProcessGoneDetail getAwRenderProcessGoneDetail() {
            assert getCallCount() > 0;
            return mDetail;
        }

        public void notifyCalled(AwRenderProcessGoneDetail detail) {
            mDetail = detail;
            notifyCalled();
        }
    }

    private static class RenderProcessGoneTestAwContentsClient extends TestAwContentsClient {

        private GetRenderProcessGoneHelper mGetRenderProcessGoneHelper;

        public RenderProcessGoneTestAwContentsClient() {
            mGetRenderProcessGoneHelper = new GetRenderProcessGoneHelper();
        }

        public GetRenderProcessGoneHelper getGetRenderProcessGoneHelper() {
            return mGetRenderProcessGoneHelper;
        }

        @Override
        public boolean onRenderProcessGone(AwRenderProcessGoneDetail detail) {
            mGetRenderProcessGoneHelper.notifyCalled(detail);
            return true;
        }
    }

    interface Terminator {
        void terminate(AwContents awContents);
    }

    private AwRenderProcess createAndTerminateRenderProcess(
            Terminator terminator, boolean expectCrash) throws Throwable {
        RenderProcessGoneTestAwContentsClient contentsClient =
                new RenderProcessGoneTestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testView.getAwContents();
        GetRenderProcessGoneHelper helper = contentsClient.getGetRenderProcessGoneHelper();

        final AwRenderProcess renderProcess =
                TestThreadUtils.runOnUiThreadBlocking(() -> awContents.getRenderProcess());

        // Ensure that the renderer has started.
        mActivityTestRule.loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Terminate the renderer.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> terminator.terminate(awContents));

        // Assert that onRenderProcessGone is called once.
        int callCount = helper.getCallCount();
        helper.waitForCallback(callCount, 1, CallbackHelper.WAIT_TIMEOUT_SECONDS * 5,
                TimeUnit.SECONDS);
        Assert.assertEquals(callCount + 1, helper.getCallCount());
        Assert.assertEquals(helper.getAwRenderProcessGoneDetail().didCrash(), expectCrash);
        Assert.assertEquals(
                RendererPriority.HIGH, helper.getAwRenderProcessGoneDetail().rendererPriority());

        return renderProcess;
    }

    @Test
    @DisabledTest // http://crbug.com/689292
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testOnRenderProcessCrash() throws Throwable {
        createAndTerminateRenderProcess(
                (AwContents awContents) -> { awContents.loadUrl("chrome://crash"); }, true);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testOnRenderProcessKill() throws Throwable {
        createAndTerminateRenderProcess(
                (AwContents awContents) -> { awContents.loadUrl("chrome://kill"); }, false);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testRenderProcessTermination() throws Throwable {
        createAndTerminateRenderProcess(
                (AwContents awContents) -> { awContents.getRenderProcess().terminate(); }, false);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testRenderProcessDifferentAfterRestart() throws Throwable {
        AwRenderProcess renderProcess1 = createAndTerminateRenderProcess(
                (AwContents awContents) -> { awContents.getRenderProcess().terminate(); }, false);
        AwRenderProcess renderProcess2 = createAndTerminateRenderProcess(
                (AwContents awContents) -> { awContents.getRenderProcess().terminate(); }, false);
        Assert.assertNotEquals(renderProcess1, renderProcess2);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testRenderProcessCanNotTerminateBeforeStart() throws Throwable {
        RenderProcessGoneTestAwContentsClient contentsClient =
                new RenderProcessGoneTestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testView.getAwContents();

        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlocking(
                () -> awContents.getRenderProcess().terminate()));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testRenderProcessSameBeforeAndAfterStart() throws Throwable {
        RenderProcessGoneTestAwContentsClient contentsClient =
                new RenderProcessGoneTestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testView.getAwContents();

        AwRenderProcess renderProcessBeforeStart =
                TestThreadUtils.runOnUiThreadBlocking(() -> awContents.getRenderProcess());

        // Ensure that the renderer has started.
        mActivityTestRule.loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        AwRenderProcess renderProcessAfterStart =
                TestThreadUtils.runOnUiThreadBlocking(() -> awContents.getRenderProcess());

        Assert.assertEquals(renderProcessBeforeStart, renderProcessAfterStart);
    }
}
