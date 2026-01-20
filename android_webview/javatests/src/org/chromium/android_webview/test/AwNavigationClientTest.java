// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwPage;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.HistoryUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.List;

/** Tests for the AwNavigationClient class. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
@CommandLineFlags.Add("enable-features=WebViewWebPerformanceMetricsReporting")
public class AwNavigationClientTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private TestAwNavigationListener mNavigationListener;
    private CallbackHelper mCallbackHelper;
    private AwTestContainerView mTestContainerView;
    private TestWebServer mWebServer;

    private static final String JS_OBJECT_NAME = "testListener";
    private static final String WEB_PERFORMANCE_METRICS_HTML =
            """
                <html>
                <head>
                <title>Test page</title>
                <script>%s</script>
                </head>
                <body>
                        <div style="font-size: 0.5em">First LCP Trigger</div>
                        <div id="second-lcp" style="font-size: 1.5em"></div>
                </body>
                </html>
            """;
    private static final String SIMPLE_PAGE_HTML =
            """
                <html>
                <body>
                        <div>Hello</div>
                </body>
                </html>
            """;
    private static final String WEB_PERFORMANCE_FCP_JS =
            """
                const observer = new PerformanceObserver((list) => {
                        testListener.postMessage(JSON.stringify(list.getEntries()));
                });
                observer.observe({entryTypes: ["paint"]});
            """;
    private static final String WEB_PERFORMANCE_LCP_JS =
            """
                setTimeout(() => {
                        document.getElementById('second-lcp').innerHTML = "Second LCP Trigger";
                        setTimeout(() => {
                                const observer = new PerformanceObserver((list) => {
                                        testListener.postMessage(JSON.stringify(list.getEntries()));
                                });
                                observer.observe({ type: "largest-contentful-paint", buffered: true });
                        }, 1000);
                }, 1000);
            """;
    private static final String WEB_PERFORMANCE_MARK_JS =
            """
                // Start the overall measurement
                performance.mark('mark0');

                function runHeavyTask() {
                        performance.mark('mark1');
                        setTimeout(() => {
                        performance.mark('mark2');
                        let marks = performance.getEntriesByType('mark');
                        testListener.postMessage(JSON.stringify(marks));
                        }, 1000);
                }

                // Run the task once the DOM is ready
                document.addEventListener('DOMContentLoaded', runHeavyTask);
            """;
    private static final String METRICS_BF_CACHE_JS =
            """
                let count = 0;
                window.addEventListener('pageshow', (event) => {
                        performance.mark('mark' + count);
                        count++;
                        testListener.postMessage("page shown");
                });
            """;

    public AwNavigationClientTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mCallbackHelper = new CallbackHelper();
        mNavigationListener = new TestAwNavigationListener(mCallbackHelper);
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mTestContainerView.getAwContents().getNavigationClient().addListener(mNavigationListener);
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFirstContentfulPaint() throws Throwable {
        mActivityTestRule
                .getAwSettingsOnUiThread(mTestContainerView.getAwContents())
                .setJavaScriptEnabled(true);

        TestWebMessageListener listener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mTestContainerView.getAwContents(), JS_OBJECT_NAME, new String[] {"*"}, listener);

        String testPage =
                mWebServer.setResponse(
                        "/web_performance_metrics.html",
                        String.format(WEB_PERFORMANCE_METRICS_HTML, WEB_PERFORMANCE_FCP_JS),
                        null);

        int callBackCount = mCallbackHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                testPage);

        // Wait for paint event to occur and js fcp load time to be returned via postmessage
        TestWebMessageListener.Data data = listener.waitForOnPostMessage();

        JSONObject jsFCPTimeData = new JSONArray(data.getAsString()).getJSONObject(1);
        long jsFCP = jsFCPTimeData.getLong("startTime");

        // Wait for FCP callback on the listener
        mCallbackHelper.waitForCallback(callBackCount, 1);
        Long navigationFCP = mNavigationListener.getLastFirstContentfulPaintLoadTime();
        Assert.assertNotNull(navigationFCP);

        // Note: The two time values may differ slightly. This is primarily due to
        // coarsening for security reasons. We check here for a difference of 5 milliseconds
        // as at a minimum we need to account for paint timing coarsening to the next multiple of
        // 4 milliseconds, or coarser, when cross-origin isolated capability is false.
        // See: https://w3c.github.io/paint-timing/#mark-paint-timing
        // and https://developer.mozilla.org/en-US/docs/Web/API/DOMHighResTimeStamp
        // and
        // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/paint/timing/paint_timing.cc
        long diffFCP = jsFCP - navigationFCP;
        assertThat(diffFCP, lessThan(5L));
        assertThat(diffFCP, greaterThan(-5L));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLargestContentfulPaint() throws Throwable {
        mActivityTestRule
                .getAwSettingsOnUiThread(mTestContainerView.getAwContents())
                .setJavaScriptEnabled(true);

        TestWebMessageListener listener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mTestContainerView.getAwContents(), JS_OBJECT_NAME, new String[] {"*"}, listener);

        String testPage =
                mWebServer.setResponse(
                        "/web_performance_metrics.html",
                        String.format(WEB_PERFORMANCE_METRICS_HTML, WEB_PERFORMANCE_LCP_JS),
                        null);

        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                testPage);

        // Wait for largest-contentful-paint data to be returned via postmessage
        TestWebMessageListener.Data data = listener.waitForOnPostMessage();
        JSONArray jsLCPs = new JSONArray(data.getAsString());

        List<Long> listenerLCPs = mNavigationListener.getLastLargestContentfulPaintLoadTimes();

        int expectedNumLCPs = 2;
        Assert.assertEquals(
                "Number of lcp events observered via js is incorrect",
                expectedNumLCPs,
                jsLCPs.length());
        Assert.assertEquals(
                "Number of lcp events observered via listener is incorrect",
                expectedNumLCPs,
                listenerLCPs.size());

        // Note: The two time values may differ slightly. This is primarily due to
        // coarsening for security reasons. We check here for a difference of 5 milliseconds
        // as at a minimum we need to account for paint timing coarsening to the next multiple of
        // 4 milliseconds, or coarser, when cross-origin isolated capability is false.
        // See: https://w3c.github.io/paint-timing/#mark-paint-timing
        // and https://developer.mozilla.org/en-US/docs/Web/API/DOMHighResTimeStamp
        // and
        // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/paint/timing/paint_timing.cc
        for (int i = 0; i < expectedNumLCPs; i++) {
            JSONObject jsLCP = jsLCPs.getJSONObject(i);
            Long listenerLCP = listenerLCPs.get(i);
            long diffLCP = jsLCP.getLong("startTime") - listenerLCP;
            assertThat(diffLCP, lessThan(5L));
            assertThat(diffLCP, greaterThan(-5L));
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPerformanceMark() throws Throwable {
        mActivityTestRule
                .getAwSettingsOnUiThread(mTestContainerView.getAwContents())
                .setJavaScriptEnabled(true);

        TestWebMessageListener listener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mTestContainerView.getAwContents(), JS_OBJECT_NAME, new String[] {"*"}, listener);

        String testPage =
                mWebServer.setResponse(
                        "/web_performance_metrics.html",
                        String.format(WEB_PERFORMANCE_METRICS_HTML, WEB_PERFORMANCE_MARK_JS),
                        null);

        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                testPage);

        // Wait for performance marks data to be returned via postmessage
        TestWebMessageListener.Data data = listener.waitForOnPostMessage();
        JSONArray jsPerformanceMarks = new JSONArray(data.getAsString());
        List<TestAwNavigationListener.PerformanceMark> listenerPerformanceMarks =
                mNavigationListener.getPerformanceMarks();

        int expectedNumMarks = 3;
        Assert.assertEquals(
                "Number of marks observered via js is incorrect",
                expectedNumMarks,
                jsPerformanceMarks.length());
        Assert.assertEquals(
                "Number of marks observered via listener is incorrect",
                expectedNumMarks,
                listenerPerformanceMarks.size());

        for (int i = 0; i < expectedNumMarks; i++) {
            JSONObject jsMark = jsPerformanceMarks.getJSONObject(i);
            TestAwNavigationListener.PerformanceMark listenerMark = listenerPerformanceMarks.get(i);
            String expectedMarkName = "mark" + i;
            Assert.assertEquals(
                    "Name of mark observered via js is incorrect",
                    expectedMarkName,
                    jsMark.getString("name"));
            Assert.assertEquals(
                    "Name of mark observered via listener is incorrect",
                    expectedMarkName,
                    listenerMark.markName);
            Assert.assertEquals(
                    "Time of mark differs between js and listener",
                    jsMark.getLong("startTime"),
                    listenerMark.markTimeMs);
        }
    }

    // Test that web performance metrics are still recorded after a page is
    // restored from the BFCache. For simplicity we test using performance mark only.
    // See @link (AwWebPerformanceMetricsObserver::OnEnterBackForwardCache)
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testMetricsBFCache() throws Exception, Throwable {
        AwContents awContents = mTestContainerView.getAwContents();
        awContents.getSettings().setBackForwardCacheEnabled(true);

        mActivityTestRule.getAwSettingsOnUiThread(awContents).setJavaScriptEnabled(true);

        TestWebMessageListener listener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                awContents, JS_OBJECT_NAME, new String[] {"*"}, listener);

        // Load initial page
        String testPageInitial =
                mWebServer.setResponse(
                        "/web_performance_metrics_initial.html",
                        String.format(WEB_PERFORMANCE_METRICS_HTML, METRICS_BF_CACHE_JS),
                        null);
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), testPageInitial);

        // Wait for post message to verify page has been shown and mark should have been set
        listener.waitForOnPostMessage();

        // Check we obseve the first performance mark
        List<TestAwNavigationListener.PerformanceMark> listenerPerformanceMarks =
                mNavigationListener.getPerformanceMarks();
        Assert.assertEquals(
                "Initial load - number of marks observered via listener is incorrect",
                1,
                listenerPerformanceMarks.size());

        // Navigate forward
        String testPageForward =
                mWebServer.setResponse(
                        "/web_performance_metrics_forward.html",
                        WEB_PERFORMANCE_METRICS_HTML,
                        null);
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), testPageForward);

        // Navigate back to initial page
        HistoryUtils.goBackSync(
                InstrumentationRegistry.getInstrumentation(),
                awContents.getWebContents(),
                mContentsClient.getOnPageStartedHelper());

        // Wait for post message to verify page has been shown again and mark should have been set
        listener.waitForOnPostMessage();

        // Check that we received another mark after the page was restored from the cache
        listenerPerformanceMarks = mNavigationListener.getPerformanceMarks();
        Assert.assertEquals(
                "After restore - number of marks observered via listener is incorrect",
                2,
                listenerPerformanceMarks.size());
        for (int i = 0; i < 2; i++) {
            TestAwNavigationListener.PerformanceMark listenerMark = listenerPerformanceMarks.get(i);
            String expectedMarkName = "mark" + i;
            Assert.assertEquals(
                    "Name of mark observered via listener is incorrect",
                    expectedMarkName,
                    listenerMark.markName);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPageGetUrl() throws Throwable {
        final String url = mWebServer.setResponse("/page.html", SIMPLE_PAGE_HTML, null);

        // Load the page
        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);

        // Verify the page URL
        AwPage page = mNavigationListener.getLastPageWithLoadEventFired();
        Assert.assertNotNull(page);
        Assert.assertEquals(url, page.getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPageGetUrlSameDocument() throws Throwable {
        final String url = mWebServer.setResponse("/page.html", SIMPLE_PAGE_HTML, null);
        final String fragmentUrl = url + "#ref";

        // Load the page
        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
        AwPage page = mNavigationListener.getLastPageWithLoadEventFired();
        Assert.assertNotNull(page);

        // Verify the page URL
        Assert.assertEquals(url, page.getUrl());

        // Load the page with a fragment
        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                fragmentUrl);

        // Verify the page URL is the fragment URL
        Assert.assertEquals(fragmentUrl, page.getUrl());
    }
}
