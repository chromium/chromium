// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.os.Handler;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.rule.ActivityTestRule;
import android.webkit.WebView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.webview_shell.WebPlatformTestsActivity;

import java.util.ArrayList;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests to ensure that system webview shell can handle WPT tests correctly.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebPlatformTestsActivityTest {
    private static final int CHILD_LAYOUT_ADDED = 1;
    private static final int CHILD_LAYOUT_REMOVED = 2;

    private static final long TEST_TIMEOUT_IN_SECONDS = scaleTimeout(3);
    private static final long ON_CREATE_WINDOW_DELAY_MS = 100;

    private static final String OPEN_CLOSE_TEST_WINDOW_SCRIPT =
            "<html><head><script>function ensure_test_window() {"
            + "  if (!this.test_window || this.test_window.location === null) {"
            + "    this.test_window = window.open('about:blank', 800, 600);"
            // Adding delay due to https://crbug.com/1002727
            + "    setTimeout(function() { this.test_window.close(); }, "
            + ON_CREATE_WINDOW_DELAY_MS + ");"
            + "  }"
            + "};"
            + "ensure_test_window();"
            + "</script></head><body>TestRunner Window</body></html>";

    private static final String MULTIPLE_OPEN_CLOSE_TEST_WINDOW_SCRIPT =
            "<html><head><script>function ensure_test_window() {"
            + "  if (!this.test_window || this.test_window.location === null) {"
            + "    this.test_window = window.open('about:blank', 800, 600);"
            + "  }"
            + "};"
            + "ensure_test_window();"
            + "</script></head><body>TestRunner Window</body></html>";

    private WebPlatformTestsActivity mTestActivity;

    @Rule
    public ActivityTestRule<WebPlatformTestsActivity> mActivityTestRule =
            new ActivityTestRule<>(WebPlatformTestsActivity.class, false, true);

    @Before
    public void setUp() {
        mTestActivity = mActivityTestRule.getActivity();
    }

    @Test
    @MediumTest
    public void testOpenCloseWindow() throws Exception {
        final BlockingQueue<Integer> queue = new LinkedBlockingQueue<>();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            mTestActivity.setTestCallback(new WebPlatformTestsActivity.TestCallback() {
                @Override
                public void onChildLayoutAdded(WebView webView) {
                    queue.add(CHILD_LAYOUT_ADDED);
                }

                @Override
                public void onChildLayoutRemoved() {
                    queue.add(CHILD_LAYOUT_REMOVED);
                }
            });
            WebView webView = mTestActivity.getTestRunnerWebView();
            webView.loadDataWithBaseURL("https://some.domain.test/", OPEN_CLOSE_TEST_WINDOW_SCRIPT,
                    "text/html", null, null);
        });
        assertNextElementFromQueue("Child window should be added.", CHILD_LAYOUT_ADDED, queue);
        assertNextElementFromQueue("Child window should be removed.", CHILD_LAYOUT_REMOVED, queue);
    }

    @Test
    @MediumTest
    public void testNestedOpensAndCloses() throws Exception {
        final BlockingQueue<Integer> queue = new LinkedBlockingQueue<>();
        final int depthToTest = 3;
        ArrayList<WebView> webViewList = new ArrayList<>();

        // Open 'depthToTest' number of windows.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            mTestActivity.setTestCallback(new WebPlatformTestsActivity.TestCallback() {
                private int mDepthCounter = 1; // one popup window is already opened by script.

                @Override
                public void onChildLayoutAdded(WebView webView) {
                    // This logic should be moved to onPageFinished() once https://crbug.com/1002727
                    // is fixed.
                    if (mDepthCounter < depthToTest) {
                        // Open another popup. Call evaluateJavascript later since loading may not
                        // have completed. This delay is also needed in opening new popup window
                        // due to https://crbug.com/1002727.
                        new Handler().postDelayed(
                                ()
                                        -> webView.evaluateJavascript(
                                                "window.open('about:blank', '_blank');", null),
                                ON_CREATE_WINDOW_DELAY_MS);
                        mDepthCounter++;
                    }
                    webViewList.add(webView);
                    queue.add(CHILD_LAYOUT_ADDED);
                }

                @Override
                public void onChildLayoutRemoved() {
                    queue.add(CHILD_LAYOUT_REMOVED);
                }
            });
            WebView webView = mTestActivity.getTestRunnerWebView();
            webView.loadDataWithBaseURL("https://some.domain.test/",
                    MULTIPLE_OPEN_CLOSE_TEST_WINDOW_SCRIPT, "text/html", null, null);
        });
        // Wait until the last creation has been finished.
        for (int i = 0; i < depthToTest; ++i) {
            assertNextElementFromQueue(
                    i + "-th child window should be added.", CHILD_LAYOUT_ADDED, queue);
        }
        // Close the windows in reverse order.
        for (int i = depthToTest - 1; i >= 0; --i) {
            WebView webView = webViewList.get(i);
            // Add a delay here due to https://crbug.com/1002727.
            InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
                new Handler().postDelayed(
                        ()
                                -> webView.evaluateJavascript("window.close();", null),
                        ON_CREATE_WINDOW_DELAY_MS);
            });
            assertNextElementFromQueue(
                    i + "-th child window should be removed.", CHILD_LAYOUT_REMOVED, queue);
        }
    }

    private void assertNextElementFromQueue(String msg, int expected, BlockingQueue<Integer> queue)
            throws Exception {
        Integer element = queue.poll(TEST_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS);
        if (element == null) throw new TimeoutException("Timeout while asserting: " + msg);
        Assert.assertEquals(msg, Integer.valueOf(expected), element);
    }
}