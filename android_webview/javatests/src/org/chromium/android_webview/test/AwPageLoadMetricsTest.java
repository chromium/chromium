// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;

import android.os.SystemClock;
import android.view.KeyEvent;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.metrics.AwMetricsServiceClient;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.blink.mojom.WebFeature;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/**
 * Integration test for PageLoadMetrics.
 */
@Batch(Batch.PER_CLASS)
public class AwPageLoadMetricsTest {
    private static final String MAIN_FRAME_FILE = "/main_frame.html";

    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents mAwContents = mTestContainerView.getAwContents();
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
                "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2");
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
                ()
                        -> (1 + navigationToLargestContentfulPaint
                                == RecordHistogram.getHistogramTotalCountForTesting(
                                        "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2")));
    }

    /**
     * This test covers WebView heartbeat metrics FirstInputDelay4.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFirstInputDelay4() throws Throwable {
        final String data = "<html><head></head><body>"
                + "<p>Hello World</p><input type='text' id='text1'>"
                + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);
        int firstInputDelay4 = RecordHistogram.getHistogramTotalCountForTesting(
                "PageLoad.InteractiveTiming.FirstInputDelay4");
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");

        // On emulator, the page might not ready for accepting the input, multiple endeavor is
        // needed.
        AwActivityTestRule.pollInstrumentationThread(() -> {
            try {
                dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
                return !"\"\"".equals(executeJavaScriptAndWaitForResult(
                        "document.getElementById('text1').value;"));
            } catch (Throwable e) {
                return false;
            }
        });
        AwActivityTestRule.pollInstrumentationThread(
                () -> (1 + firstInputDelay4
                        == RecordHistogram.getHistogramTotalCountForTesting(
                                "PageLoad.InteractiveTiming.FirstInputDelay4")));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPageLoadMetricsProvider() throws Throwable {
        final String data = "<html><head></head><body><input type='text' id='text1'></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);
        int foregroundDuration = RecordHistogram.getHistogramTotalCountForTesting(
                "PageLoad.PageTiming.ForegroundDuration");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { AwMetricsServiceClient.setConsentSetting(true); });
        loadUrlSync(url);
        // Remove the WebView from the container, to simulate app going to background.
        TestThreadUtils.runOnUiThreadBlocking(() -> { mRule.getActivity().removeAllViews(); });
        AwActivityTestRule.pollInstrumentationThread(
                () -> (1 + foregroundDuration
                        == RecordHistogram.getHistogramTotalCountForTesting(
                                "PageLoad.PageTiming.ForegroundDuration")));
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
