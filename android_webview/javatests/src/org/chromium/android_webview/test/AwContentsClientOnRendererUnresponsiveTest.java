// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.os.SystemClock;
import android.view.KeyEvent;
import android.webkit.JavascriptInterface;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.AwRenderProcessGoneDetail;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Tests for AwContentsClient.onRenderProcessGone callback. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsClientOnRendererUnresponsiveTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String TAG = "AwRendererUnresponsive";

    private static class JSBlocker {
        // The Blink thread waits on this in block(), until the test thread calls releaseBlock().
        private CountDownLatch mBlockingLatch;
        // The test thread waits on this in waitUntilBlocked(),
        // until the Blink thread calls block().
        private CountDownLatch mThreadWasBlockedLatch;

        JSBlocker() {
            mBlockingLatch = new CountDownLatch(1);
            mThreadWasBlockedLatch = new CountDownLatch(1);
        }

        public void releaseBlock() {
            mBlockingLatch.countDown();
        }

        @JavascriptInterface
        public void block() throws Exception {
            mThreadWasBlockedLatch.countDown();
            mBlockingLatch.await(AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public void waitUntilBlocked() throws Exception {
            mThreadWasBlockedLatch.await(
                    AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
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

        void transientlyBlockBlinkThread(final AwContents awContents) throws Exception {
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        awContents.evaluateJavaScript("blocker.block();", null);
                    });
            mBlocker.waitUntilBlocked();
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
        private JSBlocker mBlocker;

        public RendererUnresponsiveTestAwContentsClient() {
            mUnresponsiveCallbackHelper = new CallbackHelper();
            mTerminatedCallbackHelper = new CallbackHelper();
            mBlocker = new JSBlocker();
        }

        void permanentlyBlockBlinkThread(final AwContents awContents) throws Exception {
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        awContents.evaluateJavaScript("blocker.block();", null);
                    });
            mBlocker.waitUntilBlocked();
        }

        void awaitRendererTermination() throws Exception {
            mUnresponsiveCallbackHelper.waitForCallback(
                    0,
                    UNRESPONSIVE_CALLBACK_COUNT,
                    AwActivityTestRule.WAIT_TIMEOUT_MS,
                    TimeUnit.MILLISECONDS);
            Assert.assertEquals(
                    UNRESPONSIVE_CALLBACK_COUNT, mUnresponsiveCallbackHelper.getCallCount());

            mTerminatedCallbackHelper.waitForCallback(
                    0, 1, AwActivityTestRule.WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            Assert.assertEquals(1, mTerminatedCallbackHelper.getCallCount());
        }

        JSBlocker getBlocker() {
            return mBlocker;
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

    public AwContentsClientOnRendererUnresponsiveTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    private void sendInputEvent(final AwContents awContents) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    long eventTime = SystemClock.uptimeMillis();
                    awContents.dispatchKeyEvent(
                            new KeyEvent(
                                    eventTime,
                                    eventTime,
                                    KeyEvent.ACTION_DOWN,
                                    KeyEvent.KEYCODE_ENTER,
                                    0));
                });
    }

    private void addJsBlockerInterface(final AwContents awContents, final JSBlocker blocker)
            throws Exception {
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
        addJsBlockerInterface(awContents, contentsClient.getBlocker());
        mActivityTestRule.loadUrlSync(
                awContents,
                contentsClient.getOnPageFinishedHelper(),
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
        mActivityTestRule.loadUrlSync(
                awContents,
                contentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        contentsClient.transientlyBlockBlinkThread(awContents);
        sendInputEvent(awContents);
        contentsClient.awaitRecovery();
    }
}
