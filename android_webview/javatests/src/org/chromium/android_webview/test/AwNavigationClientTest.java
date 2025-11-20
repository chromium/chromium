// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

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

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.List;

/** Tests for the AwNavigationClient class. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
public class AwNavigationClientTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private TestAwNavigationListener mNavigationListener;
    private AwTestContainerView mTestContainerView;
    private TestWebServer mWebServer;

    private static final String JS_OBJECT_NAME = "testListener";
    private static final String WEB_PERFORMANCE_METRICS_HTML =
            """
                <html>
                <head>
                <title>Hello, World!</title>
                <script>
                        // Start the overall measurement
                        performance.mark('mark0');

                        /**
                         * Simulates a heavy task
                         */
                        function runHeavyTask() {
                            performance.mark('mark1');
                            setTimeout(() => {
                                performance.mark('mark2');
                                let marks = performance.getEntriesByType("mark");
                                testListener.postMessage(JSON.stringify(marks));
                            }, 1000);
                        }

                        // Run the task once the DOM is ready
                        document.addEventListener('DOMContentLoaded', runHeavyTask);
                </script>
                </head>
                <body>
                Hello, World!
                </body>
                </html>
            """;

    public AwNavigationClientTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mNavigationListener = new TestAwNavigationListener();
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
    public void testPerformanceMark() throws Throwable {
        mActivityTestRule
                .getAwSettingsOnUiThread(mTestContainerView.getAwContents())
                .setJavaScriptEnabled(true);

        TestWebMessageListener listener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mTestContainerView.getAwContents(), JS_OBJECT_NAME, new String[] {"*"}, listener);

        String testPage =
                mWebServer.setResponse(
                        "/web_performance_metrics.html", WEB_PERFORMANCE_METRICS_HTML, null);

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
}
