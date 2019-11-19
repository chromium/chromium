// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;

import android.os.SystemClock;
import android.support.test.filters.SmallTest;
import android.view.KeyEvent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import org.chromium.android_webview.AwContents;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.blink.mojom.WebFeature;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/**
 * Integration test for PageLoadMetrics.
 */
public class AwPageLoadMetricsTest {
    private static final String MAIN_FRAME_FILE = "/main_frame.html";

    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestAwContentsClient mContentsClient;
    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    private void loadUrlSync(String url) throws Exception {
        mRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
    }

    /**
     * This test doesn't intent to cover all UseCounter metrics, and just test WebView integration
     * works
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUseCounterMetrics() throws Throwable {
        final String data = "<html><head></head><body><form></form></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);
        MetricsUtils.HistogramDelta delta = new MetricsUtils.HistogramDelta(
                "Blink.UseCounter.MainFrame.Features", WebFeature.PAGE_VISITS);
        MetricsUtils.HistogramDelta form = new MetricsUtils.HistogramDelta(
                "Blink.UseCounter.Features", WebFeature.FORM_ELEMENT);
        loadUrlSync(url);
        loadUrlSync("about:blank");
        assertEquals(1, delta.getDelta());
        assertEquals(1, form.getDelta());
    }

    /**
     * This test covers WebView heartbeat metrics from CorePageLoadMetrics.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHeartbeatMetrics() throws Throwable {
        final String data = "<html><head></head><body><p>Hello World</p></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);
        int navigationToFirstPaint = RecordHistogram.getHistogramTotalCountForTesting(
                "PageLoad.PaintTiming.NavigationToFirstPaint");
        int navigationToFirstContentfulPaint = RecordHistogram.getHistogramTotalCountForTesting(
                "PageLoad.PaintTiming.NavigationToFirstContentfulPaint");
        int navigationToLargestContentfulPaint = RecordHistogram.getHistogramTotalCountForTesting(
                "PageLoad.PaintTiming.NavigationToLargestContentfulPaint");
        loadUrlSync(url);
        AwActivityTestRule.pollInstrumentationThread(
                () -> (1 + navigationToFirstPaint
                        == RecordHistogram.getHistogramTotalCountForTesting(
                                "PageLoad.PaintTiming.NavigationToFirstPaint")));
        AwActivityTestRule.pollInstrumentationThread(
                () -> (1 + navigationToFirstContentfulPaint
                        == RecordHistogram.getHistogramTotalCountForTesting(
                                "PageLoad.PaintTiming.NavigationToFirstContentfulPaint")));
        // Flush NavigationToLargestContentfulPaint.
        loadUrlSync("about:blank");
        AwActivityTestRule.pollInstrumentationThread(
                () -> (1 + navigationToLargestContentfulPaint
                        == RecordHistogram.getHistogramTotalCountForTesting(
                                "PageLoad.PaintTiming.NavigationToLargestContentfulPaint")));
    }

    /**
     * This test covers WebView heartbeat metrics FirstInputDelay4.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFirstInputDelay4() throws Throwable {
        final String data = "<html><head></head><body><input type='text' id='text1'></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);
        int firstInputDelay4 = RecordHistogram.getHistogramTotalCountForTesting(
                "PageLoad.InteractiveTiming.FirstInputDelay4");
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        AwActivityTestRule.pollInstrumentationThread(
                () -> (1 + firstInputDelay4
                        == RecordHistogram.getHistogramTotalCountForTesting(
                                "PageLoad.InteractiveTiming.FirstInputDelay4")));
    }

    private String executeJavaScriptAndWaitForResult(String code) throws Throwable {
        return mRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(), mContentsClient, code);
    }

    private void dispatchDownAndUpKeyEvents(final int code) throws Throwable {
        dispatchKeyEvent(new KeyEvent(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(),
                KeyEvent.ACTION_DOWN, code, 0));
        dispatchKeyEvent(new KeyEvent(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(),
                KeyEvent.ACTION_UP, code, 0));
    }

    private boolean dispatchKeyEvent(final KeyEvent event) throws Throwable {
        return TestThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mTestContainerView.dispatchKeyEvent(event);
            }
        });
    }
}
