// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.support.test.filters.LargeTest;
import android.view.KeyEvent;
import android.webkit.JavascriptInterface;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.AwRenderProcessGoneDetail;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests for AwContentsClient.onRenderProcessGone callback.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsClientOnRendererUnresponsiveTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static final String TAG = "AwRendererUnresponsive";

    private static class JSBlocker {
        private CountDownLatch mLatch;
        JSBlocker() {
            mLatch = new CountDownLatch(1);
        }

        public void releaseBlock() {
            mLatch.countDown();
        }

        @JavascriptInterface
        public void block() throws Exception {
            mLatch.await(AwActivityTestRule.WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }
    }

    private static class RendererTransientlyUnresponsiveTestAwContentsClient
            extends TestAwContentsClient {
        private CallbackHelper mUnresponsiveCallbackHelper;
        private CallbackHelper mResponsiveCallbackHelper;
        private JSBlocker mBlocker;

        public RendererTransientlyUnresponsiveTestAwContentsClient() {
            mUnresponsiveCallbackHelper = new CallbackHelper();
            mResponsiveCallbackHelper = new CallbackHelper();
            mBlocker = new JSBlocker();
        }

        void transientlyBlockBlinkThread(final AwContents awContents) {
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                    () -> { awContents.evaluateJavaScript("blocker.block();", null); });
        }

        void awaitRecovery() throws Exception {
            mUnresponsiveCallbackHelper.waitForCallback(
                    0, 1, AwActivityTestRule.WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            Assert.assertEquals(1, mUnresponsiveCallbackHelper.getCallCount());
            mResponsiveCallbackHelper.waitForCallback(
                    0, 1, AwActivityTestRule.WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            Assert.assertEquals(1, mResponsiveCallbackHelper.getCallCount());
        }

        JSBlocker getBlocker() {
            return mBlocker;
        }

        @Override
        public void onRendererResponsive(AwRenderProcess process) {
            mResponsiveCallbackHelper.notifyCalled();
        }

        @Override
        public void onRendererUnresponsive(AwRenderProcess process) {
            // onRendererResponsive should not have been called yet.
            Assert.assertEquals(0, mResponsiveCallbackHelper.getCallCount());
            mUnresponsiveCallbackHelper.notifyCalled();
            mBlocker.releaseBlock();
        }
    }

    private static class RendererUnresponsiveTestAwContentsClient extends TestAwContentsClient {
        // The renderer unresponsive callback should be called repeatedly. We will wait for two
        // callbacks.
        static final int UNRESPONSIVE_CALLBACK_COUNT = 2;

        private CallbackHelper mUnresponsiveCallbackHelper;
        private CallbackHelper mTerminatedCallbackHelper;

        public RendererUnresponsiveTestAwContentsClient() {
            mUnresponsiveCallbackHelper = new CallbackHelper();
            mTerminatedCallbackHelper = new CallbackHelper();
        }

        void permanentlyBlockBlinkThread(final AwContents awContents) {
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                    () -> { awContents.evaluateJavaScript("blocker.block();", null); });
        }

        void awaitRendererTermination() throws Exception {
            mUnresponsiveCallbackHelper.waitForCallback(0, UNRESPONSIVE_CALLBACK_COUNT,
                    AwActivityTestRule.WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            Assert.assertEquals(
                    UNRESPONSIVE_CALLBACK_COUNT, mUnresponsiveCallbackHelper.getCallCount());

            mTerminatedCallbackHelper.waitForCallback(
                    0, 1, AwActivityTestRule.WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            Assert.assertEquals(1, mTerminatedCallbackHelper.getCallCount());
        }

        @Override
        public boolean onRenderProcessGone(AwRenderProcessGoneDetail detail) {
            mTerminatedCallbackHelper.notifyCalled();
            return true;
        }

        @Override
        public void onRendererUnresponsive(AwRenderProcess process) {
            mUnresponsiveCallbackHelper.notifyCalled();
            if (mUnresponsiveCallbackHelper.getCallCount() == UNRESPONSIVE_CALLBACK_COUNT) {
                process.terminate();
            }
        }
    }

    private void sendInputEvent(final AwContents awContents) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            awContents.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        });
    }

    private void addJsBlockerInterface(final AwContents awContents, final JSBlocker blocker) {
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(awContents, blocker, "blocker");
    }

    // This test requires the ability to terminate the renderer in order to recover from a
    // permanently stuck blink main thread, so it can only run in multiprocess.
    @Test
    @Feature({"AndroidWebView"})
    @LargeTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testOnRendererUnresponsive() throws Throwable {
        RendererUnresponsiveTestAwContentsClient contentsClient =
                new RendererUnresponsiveTestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testView.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        addJsBlockerInterface(awContents, new JSBlocker());
        mActivityTestRule.loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        contentsClient.permanentlyBlockBlinkThread(awContents);
        // Sending a key event while the renderer is unresponsive will cause onRendererUnresponsive
        // to be called.
        sendInputEvent(awContents);
        contentsClient.awaitRendererTermination();
    }

    @Test
    @Feature({"AndroidWebView"})
    @LargeTest
    public void testTransientUnresponsiveness() throws Throwable {
        RendererTransientlyUnresponsiveTestAwContentsClient contentsClient =
                new RendererTransientlyUnresponsiveTestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testView.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        addJsBlockerInterface(awContents, contentsClient.getBlocker());
        mActivityTestRule.loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        contentsClient.transientlyBlockBlinkThread(awContents);
        sendInputEvent(awContents);
        contentsClient.awaitRecovery();
    }
}
