// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.SystemClock;
import android.view.KeyEvent;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.metrics.AwMetricsServiceClient;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.blink.mojom.WebFeature;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/** Integration test for PageLoadMetrics. */
@Batch(Batch.PER_CLASS)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwPageLoadMetricsTest extends AwParameterizedTest {
    private static final String MAIN_FRAME_FILE = "/main_frame.html";

    @Rule public AwActivityTestRule mRule;

    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private TestWebServer mWebServer;

    public AwPageLoadMetricsTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

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
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Blink.UseCounter.MainFrame.Features", WebFeature.PAGE_VISITS)
                        .expectIntRecord("Blink.UseCounter.Features", WebFeature.FORM_ELEMENT)
                        .allowExtraRecordsForHistogramsAbove()
                        .build();
        loadUrlSync(url);
        loadUrlSync("about:blank");
        histograms.assertExpected();
    }

    /** This test covers WebView heartbeat metrics from CorePageLoadMetrics. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @RequiresRestart(
            "NavigationToFirstPaint is only recorded once, making the test fail "
                    + "when run in a batch and being the first test.")
    public void testHeartbeatMetrics() throws Throwable {
        final String data = "<html><head></head><body><p>Hello World</p></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("PageLoad.PaintTiming.NavigationToFirstPaint")
                        .expectAnyRecord("PageLoad.PaintTiming.NavigationToFirstContentfulPaint")
                        .build();
        loadUrlSync(url);
        histograms.pollInstrumentationThreadUntilSatisfied();

        // Flush NavigationToLargestContentfulPaint.
        histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("PageLoad.PaintTiming.NavigationToLargestContentfulPaint2")
                        .build();
        loadUrlSync("about:blank");
        histograms.pollInstrumentationThreadUntilSatisfied();
    }

    /** This test covers WebView heartbeat metrics FirstInputDelay4. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFirstInputDelay4() throws Throwable {
        final String data =
                "<html><head></head><body>"
                        + "<p>Hello World</p><input type='text' id='text1'>"
                        + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);
        var firstInputDelay4Histogram =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("PageLoad.InteractiveTiming.FirstInputDelay4")
                        .build();
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");

        // On emulator, the page might not ready for accepting the input, multiple endeavor is
        // needed.
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    try {
                        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
                        return !"\"\""
                                .equals(
                                        executeJavaScriptAndWaitForResult(
                                                "document.getElementById('text1').value;"));
                    } catch (Throwable e) {
                        return false;
                    }
                });
        firstInputDelay4Histogram.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPageLoadMetricsProvider() throws Throwable {
        final String data = "<html><head></head><body><input type='text' id='text1'></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);
        var foregroundDurationHistogram =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("PageLoad.PageTiming.ForegroundDuration")
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwMetricsServiceClient.setConsentSetting(true);
                });
        loadUrlSync(url);
        // Remove the WebView from the container, to simulate app going to background.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRule.getActivity().removeAllViews();
                });
        foregroundDurationHistogram.pollInstrumentationThreadUntilSatisfied();
    }

    private String executeJavaScriptAndWaitForResult(String code) throws Throwable {
        return mRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(), mContentsClient, code);
    }

    private void dispatchDownAndUpKeyEvents(final int code) throws Throwable {
        dispatchKeyEvent(
                new KeyEvent(
                        SystemClock.uptimeMillis(),
                        SystemClock.uptimeMillis(),
                        KeyEvent.ACTION_DOWN,
                        code,
                        0));
        dispatchKeyEvent(
                new KeyEvent(
                        SystemClock.uptimeMillis(),
                        SystemClock.uptimeMillis(),
                        KeyEvent.ACTION_UP,
                        code,
                        0));
    }

    private boolean dispatchKeyEvent(final KeyEvent event) throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return mTestContainerView.dispatchKeyEvent(event);
                    }
                });
    }
}
