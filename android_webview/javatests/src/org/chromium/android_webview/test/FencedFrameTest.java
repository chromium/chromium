// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.graphics.Color;
import android.util.Pair;
import android.webkit.JavascriptInterface;
import android.webkit.WebView.HitTestResult;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.android_webview.test.util.JavascriptEventObserver;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Test for creating fenced frames in Android WebView. */
@DoNotBatch(reason = "Test instrumentation only supports one hardware compositing view.")
@CommandLineFlags.Add(AwSwitches.WEBVIEW_FENCED_FRAMES)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class FencedFrameTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    public FencedFrameTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestView.getAwContents();
        mWebServer = TestWebServer.startSsl();
    }

    @After
    public void tearDown() {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    /**
     * Allocate an URL from the webserver that stores a main document and a fenced frame
     * resource to be returned. The result should then be loaded in the WebContents.
     */
    private String generateFencedFrame(String fencedFrameHtml) {
        String path = "/fenced_frame.html";
        final List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(new Pair("Supports-Loading-Mode", " fenced-frame"));
        String fencedFrameUrl = mWebServer.setResponse(path, fencedFrameHtml, headers);

        String mainPath = "/main_document.html";
        String mainResponseStr =
                "<html><body><fencedframe style='width: 100%; height: 100%'></fencedframe>"
                        + "<script>const url = new URL(\""
                        + fencedFrameUrl
                        + "\");document.querySelector(\"fencedframe\").config = new"
                        + " FencedFrameConfig(url);</script></body></html>";
        return mWebServer.setResponse(mainPath, mainResponseStr, null);
    }

    /**
     * Test that a java object is mirrored in a fenced frame.
     **/
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testInjectedObjectPresentInFencedFrame() throws Throwable {
        String fencedFrameSource = "<script>fencedFrameObserver.notifyJava();</script>";
        String mainUrl = generateFencedFrame(fencedFrameSource);

        final JavascriptEventObserver fencedFrameObserver = new JavascriptEventObserver();
        final String fencedFrameObserverName = "fencedFrameObserver";
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            fencedFrameObserver.register(
                                    mTestView.getWebContents(), fencedFrameObserverName);
                        });
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mainUrl);
        Assert.assertTrue(fencedFrameObserver.waitForEvent(WAIT_TIMEOUT_MS));
    }

    /**
     * Test that an object that is mutated in the main frame can be observed in
     * the fenced frame.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCommunicationBetweenFrames() throws Throwable {
        String fencedFrameSource =
                "<script>"
                        + "  function step() {"
                        + "    if (testObserver.getValue() == 'SET') {"
                        + "      testObserver.notifyJava(); "
                        + "    } else {"
                        + "      requestAnimationFrame(step);"
                        + "    }"
                        + "  }"
                        + "  requestAnimationFrame(step);"
                        + "</script>";
        String mainUrl = generateFencedFrame(fencedFrameSource);

        class TestObserver {
            private String mValue = "UNSET";
            private CallbackHelper mCallbackHelper = new CallbackHelper();

            @JavascriptInterface
            public String getValue() {
                return mValue;
            }

            @JavascriptInterface
            public void setString(String value) {
                mValue = value;
            }

            @JavascriptInterface
            public void notifyJava() {
                mCallbackHelper.notifyCalled();
            }

            public void waitForEvent() throws TimeoutException {
                mCallbackHelper.waitForNext();
            }
        }
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestObserver testObserver = new TestObserver();
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                mAwContents, testObserver, "testObserver");

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mainUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.evaluateJavaScript("testObserver.setString('SET');", null);
                });
        testObserver.waitForEvent();
    }

    /** Test that a hit test in a fenced frame produces the correct results on the WebView API. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void hitTestFencedFrame() throws Throwable {
        String fencedFrameSource =
                CommonResources.makeHtmlPageFrom(
                        "",
                        "<a href='http://foo/' class='full_view' onclick='return false;'>Test</a>"
                                + "<script>fencedFrameObserver.notifyJava();</script>");
        String mainUrl = generateFencedFrame(fencedFrameSource);

        final JavascriptEventObserver fencedFrameObserver = new JavascriptEventObserver();
        final String fencedFrameObserverName = "fencedFrameObserver";
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            fencedFrameObserver.register(
                                    mTestView.getWebContents(), fencedFrameObserverName);
                        });
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mainUrl);

        // We need to wait for the fenced frame to load because loadUrlSync only waits
        // for the outermost main frame.
        Assert.assertTrue(fencedFrameObserver.waitForEvent(WAIT_TIMEOUT_MS));

        mActivityTestRule.pollUiThread(
                () -> {
                    // The hit testing regions may not be available on the first calls and there is
                    // no way of knowing when they are ready and it is safe to send input, so we
                    // send it every iteration.
                    AwTestTouchUtils.simulateTouchCenterOfView(mTestView);

                    AwContents.HitTestData data = mAwContents.getLastHitTestResult();
                    return HitTestResult.SRC_ANCHOR_TYPE == data.hitTestResultType
                            && "http://foo/".equals(data.hitTestResultExtraData);
                });
    }

    /**
     * Test that a fenced frame is rastered correctly.
     **/
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void fencedFrameDrawingSmokeTest() throws Throwable {
        String fencedFrameSource =
                "<html>"
                        + "  <body style=\""
                        + "       padding: 0;"
                        + "       margin: 0;"
                        + "       display: grid;"
                        + "       display: grid;"
                        + "       grid-template-columns: 50% 50%;"
                        + "       grid-template-rows: 50% 50%;\">"
                        + "   <div style=\"background-color: rgb(255, 0, 0);\"></div>"
                        + "   <div style=\"background-color: rgb(0, 255, 0);\"></div>"
                        + "   <div style=\"background-color: rgb(0, 0, 255);\"></div>"
                        + "   <div style=\"background-color: rgb(128, 128, 128);\"></div>"
                        + "   <script>fencedFrameObserver.notifyJava();</script>"
                        + "  </body>"
                        + "</html>";
        String mainUrl = generateFencedFrame(fencedFrameSource);

        final JavascriptEventObserver fencedFrameObserver = new JavascriptEventObserver();
        final String fencedFrameObserverName = "fencedFrameObserver";
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            fencedFrameObserver.register(
                                    mTestView.getWebContents(), fencedFrameObserverName);
                        });

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mainUrl);
        // We need to wait for the fenced frame to load because loadUrlSync only waits
        // for the outermost main frame.
        Assert.assertTrue(fencedFrameObserver.waitForEvent(WAIT_TIMEOUT_MS));
        mActivityTestRule.waitForVisualStateCallback(mAwContents);

        int expectedQuadrantColors[] = {
            Color.rgb(255, 0, 0),
            Color.rgb(0, 255, 0),
            Color.rgb(0, 0, 255),
            Color.rgb(128, 128, 128)
        };

        GraphicsTestUtils.pollForQuadrantColors(mTestView, expectedQuadrantColors);
    }
}
