// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test suite for displaying and functioning of modal dialogs.
 */

package org.chromium.android_webview.test;

import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.JsPromptResultReceiver;
import org.chromium.android_webview.JsResultReceiver;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for callbacks implementing JS alerts and prompts.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class WebViewModalDialogOverrideTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static final String EMPTY_PAGE =
            "<!doctype html>"
            + "<title>Modal Dialog Test</title><p>Testcase.</p>";
    private static final String BEFORE_UNLOAD_URL =
            "<!doctype html>"
            + "<head><script>window.onbeforeunload=function() {"
            + "return 'Are you sure?';"
            + "};</script></head></body>";

    /*
     * Verify that when the AwContentsClient calls handleJsAlert.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverrideAlertHandling() throws Throwable {
        final String alertText = "Hello World!";

        final AtomicBoolean callbackCalled = new AtomicBoolean(false);
        // Returning true from the callback should not show a dialog.
        TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public void handleJsAlert(String url, String message, JsResultReceiver res) {
                callbackCalled.set(true);
                res.confirm();
                Assert.assertEquals(alertText, message);
            }
        };
        AwTestContainerView view = mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        mActivityTestRule.loadDataSync(
                awContents, client.getOnPageFinishedHelper(), EMPTY_PAGE, "text/html", false);
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                awContents, client, "alert('" + alertText + "')");
        Assert.assertTrue(callbackCalled.get());
    }

    /*
     * Verify that when the AwContentsClient calls handleJsPrompt.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverridePromptHandling() throws Throwable {
        final String promtText = "How do you like your eggs in the morning?";
        final String promptDefault = "Scrambled";
        final String promptResult = "I like mine with a kiss";

        final AtomicBoolean called = new AtomicBoolean(false);
        // Returning true from the callback should not show a dialog.
        final TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public void handleJsPrompt(String url, String message, String defaultValue,
                                      JsPromptResultReceiver res) {
                Assert.assertEquals(promtText, message);
                Assert.assertEquals(promptDefault, defaultValue);
                res.confirm(promptResult);
                called.set(true);
            }
        };
        AwTestContainerView view = mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        mActivityTestRule.loadDataSync(
                awContents, client.getOnPageFinishedHelper(), EMPTY_PAGE, "text/html", false);
        String result = mActivityTestRule.executeJavaScriptAndWaitForResult(
                awContents, client, "prompt('" + promtText + "','" + promptDefault + "')");
        Assert.assertTrue(called.get());
        Assert.assertEquals("\"" + promptResult + "\"", result);
    }

    /*
     * Verify that when the AwContentsClient calls handleJsConfirm and the client confirms.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverrideConfirmHandlingConfirmed() throws Throwable {
        final String confirmText = "Would you like a cookie?";

        final AtomicBoolean called = new AtomicBoolean(false);
        // Returning true from the callback should not show a dialog.
        TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public void handleJsConfirm(String url, String message, JsResultReceiver res) {
                Assert.assertEquals(confirmText, message);
                res.confirm();
                called.set(true);
            }
        };
        AwTestContainerView view = mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        mActivityTestRule.loadDataSync(
                awContents, client.getOnPageFinishedHelper(), EMPTY_PAGE, "text/html", false);
        String result = mActivityTestRule.executeJavaScriptAndWaitForResult(
                awContents, client, "confirm('" + confirmText + "')");
        Assert.assertTrue(called.get());
        Assert.assertEquals("true", result);
    }

    /*
     * Verify that when the AwContentsClient calls handleJsConfirm and the client cancels.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverrideConfirmHandlingCancelled() throws Throwable {
        final String confirmText = "Would you like a cookie?";

        final AtomicBoolean called = new AtomicBoolean(false);
        // Returning true from the callback should not show a dialog.
        TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public void handleJsConfirm(String url, String message, JsResultReceiver res) {
                Assert.assertEquals(confirmText, message);
                res.cancel();
                called.set(true);
            }
        };
        AwTestContainerView view = mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        mActivityTestRule.loadDataSync(
                awContents, client.getOnPageFinishedHelper(), EMPTY_PAGE, "text/html", false);
        String result = mActivityTestRule.executeJavaScriptAndWaitForResult(
                awContents, client, "confirm('" + confirmText + "')");
        Assert.assertTrue(called.get());
        Assert.assertEquals("false", result);
    }

    private static class TapGestureStateListener implements GestureStateListener {
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        public int getCallCount() {
            return mCallbackHelper.getCallCount();
        }

        public void waitForTap(int currentCallCount) throws Throwable {
            mCallbackHelper.waitForCallback(currentCallCount);
        }

        @Override
        public void onSingleTap(boolean consumed) {
            mCallbackHelper.notifyCalled();
        }
    }

    /**
     * Taps on a view and waits for a callback.
     */
    private void tapViewAndWait(AwTestContainerView view) throws Throwable {
        final TapGestureStateListener tapGestureStateListener = new TapGestureStateListener();
        int callCount = tapGestureStateListener.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            GestureListenerManager.fromWebContents(view.getWebContents())
                    .addListener(tapGestureStateListener);
        });

        AwTestTouchUtils.simulateTouchCenterOfView(view);
        tapGestureStateListener.waitForTap(callCount);
    }

    /*
     * Verify that when the AwContentsClient calls handleJsBeforeUnload
     */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOverrideBeforeUnloadHandling() throws Throwable {
        final CallbackHelper jsBeforeUnloadHelper = new CallbackHelper();
        TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public void handleJsBeforeUnload(String url, String message, JsResultReceiver res) {
                res.cancel();
                jsBeforeUnloadHelper.notifyCalled();
            }
        };
        AwTestContainerView view = mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        mActivityTestRule.loadDataSync(awContents, client.getOnPageFinishedHelper(),
                BEFORE_UNLOAD_URL, "text/html", false);
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        // JavaScript onbeforeunload dialogs require a user gesture.
        tapViewAndWait(view);

        // Don't wait synchronously because we don't leave the page.
        int currentCallCount = jsBeforeUnloadHelper.getCallCount();
        mActivityTestRule.loadDataAsync(awContents, EMPTY_PAGE, "text/html", false);
        jsBeforeUnloadHelper.waitForCallback(currentCallCount);
    }
}
