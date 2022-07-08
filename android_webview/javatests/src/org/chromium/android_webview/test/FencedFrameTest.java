// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.support.test.InstrumentationRegistry;
import android.util.Pair;
import android.webkit.JavascriptInterface;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.JavascriptEventObserver;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Test for creating fenced frames in Android WebView.
 */
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add("webview-mparch-fenced-frames")
@RunWith(AwJUnit4ClassRunner.class)
public class FencedFrameTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

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
                "<html><body><fencedframe src=" + fencedFrameUrl + "></fencedframe></body></html>";
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

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            fencedFrameObserver.register(mTestView.getWebContents(), fencedFrameObserverName);
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
        String fencedFrameSource = "<script>"
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

        mActivityTestRule.runOnUiThread(
                () -> { mAwContents.evaluateJavaScript("testObserver.setString('SET');", null); });
        testObserver.waitForEvent();
    }
}
