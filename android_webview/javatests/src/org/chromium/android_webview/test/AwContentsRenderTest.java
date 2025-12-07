// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.VisualStateCallback;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** AwContents rendering / pixel tests. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwContentsRenderTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private AwTestContainerView mContainerView;

    public AwContentsRenderTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mContainerView.getAwContents();
    }

    void setBackgroundColorOnUiThread(final int c) {
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.setBackgroundColor(c));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetGetBackgroundColor() throws Throwable {
        setBackgroundColorOnUiThread(Color.MAGENTA);
        GraphicsTestUtils.pollForBackgroundColor(mAwContents, Color.MAGENTA);

        setBackgroundColorOnUiThread(Color.CYAN);
        GraphicsTestUtils.pollForBackgroundColor(mAwContents, Color.CYAN);

        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        GraphicsTestUtils.pollForBackgroundColor(mAwContents, Color.CYAN);

        setBackgroundColorOnUiThread(Color.YELLOW);
        GraphicsTestUtils.pollForBackgroundColor(mAwContents, Color.YELLOW);

        final String html_meta = "<html><head><meta name=color-scheme content=dark></head></html>";
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "data:text/html," + html_meta);
        final int dark_scheme_color = 0xFF121212;
        GraphicsTestUtils.pollForBackgroundColor(mAwContents, dark_scheme_color);

        final String html =
                "<html><head><style>body {background-color:#227788}</style></head>"
                        + "<body></body></html>";
        // Loading the html via a data URI requires us to encode '#' symbols as '%23'.
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "data:text/html," + html.replace("#", "%23"));
        final int teal = 0xFF227788;
        GraphicsTestUtils.pollForBackgroundColor(mAwContents, teal);

        // Changing the base background should not override CSS background.
        setBackgroundColorOnUiThread(Color.MAGENTA);
        Assert.assertEquals(teal, GraphicsTestUtils.sampleBackgroundColorOnUiThread(mAwContents));
        // ...setting the background is asynchronous, so pause a bit and retest just to be sure.
        Thread.sleep(500);
        Assert.assertEquals(teal, GraphicsTestUtils.sampleBackgroundColorOnUiThread(mAwContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPictureListener() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.enableOnNewPicture(true, true));

        int pictureCount = mContentsClient.getPictureListenerHelper().getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        mContentsClient.getPictureListenerHelper().waitForCallback(pictureCount, 1);
        // Invalidation only, so picture should be null.
        Assert.assertNull(mContentsClient.getPictureListenerHelper().getPicture());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testForceDrawWhenInvisible() throws Throwable {
        final String html =
                "<html><head><style>body {background-color:#227788}</style></head>"
                        + "<body>Hello world!</body></html>";
        // Loading the html via a data URI requires us to encode '#' symbols as '%23'.
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "data:text/html," + html.replace("#", "%23"));

        Bitmap visibleBitmap = null;
        Bitmap invisibleBitmap = null;
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            final long requestId1 = 1;
                            mAwContents.insertVisualStateCallback(
                                    requestId1,
                                    new VisualStateCallback() {
                                        @Override
                                        public void onComplete(long id) {
                                            Assert.assertEquals(requestId1, id);
                                            latch.countDown();
                                        }
                                    });
                        });
        Assert.assertTrue(
                latch.await(AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));

        final int width = ThreadUtils.runOnUiThreadBlocking(() -> mContainerView.getWidth());
        final int height = ThreadUtils.runOnUiThreadBlocking(() -> mContainerView.getHeight());
        visibleBitmap = GraphicsTestUtils.drawAwContentsOnUiThread(mAwContents, width, height);

        // Things that affect DOM page visibility:
        // 1. isPaused
        // 2. window's visibility, if the webview is attached to a window.
        // Note android.view.View's visibility does not affect DOM page visibility.
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mContainerView.setVisibility(View.INVISIBLE);
                            Assert.assertTrue(mAwContents.isPageVisible());

                            mAwContents.onPause();
                            Assert.assertFalse(mAwContents.isPageVisible());

                            mAwContents.onResume();
                            Assert.assertTrue(mAwContents.isPageVisible());

                            // Simulate a window visibility change. WebView test app can't
                            // manipulate the window visibility directly.
                            mAwContents.getViewMethods().onWindowVisibilityChanged(View.INVISIBLE);
                            Assert.assertFalse(mAwContents.isPageVisible());
                        });

        // VisualStateCallback#onComplete won't be called when WebView is
        // invisible. So there is no reliable way to tell if View#setVisibility
        // has taken effect. Just sleep the test thread for 500ms.
        Thread.sleep(500);
        invisibleBitmap = GraphicsTestUtils.drawAwContentsOnUiThread(mAwContents, width, height);
        Assert.assertNotNull(invisibleBitmap);
        Assert.assertTrue(invisibleBitmap.sameAs(visibleBitmap));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSoftwareCanvas() throws Throwable {
        mAwContents.getSettings().setAllowFileAccess(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getViewMethods().setLayerType(View.LAYER_TYPE_SOFTWARE, null));

        String testFile = "android_webview/test/data/green_canvas.html";
        String url = UrlUtils.getIsolatedTestFileUrl(testFile);
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        mActivityTestRule.waitForVisualStateCallback(mAwContents);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Bitmap bitmap =
                            GraphicsTestUtils.drawAwContentsOnUiThread(mAwContents, 500, 500);
                    return Color.GREEN == bitmap.getPixel(250, 250);
                });
    }

    private static class TitleUpdatedHelper extends CallbackHelper {
        private String mTitle;

        public String getTitle() {
            return mTitle;
        }

        public void setTitle(String title) {
            mTitle = title;
            notifyCalled();
        }
    }

    private static final String CALL_RAF = "window.requestAnimationFrame(window.onAnimationFrame);";

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @RequiresRestart("Tests pause feature which will prevent rendering")
    public void testPausePreventsRAF() throws Throwable {
        final String html =
                "<html><head><style>body {background-color:#227788}</style></head>"
                        + "<body>"
                        + "<script>"
                        + "    var raf_count = 0;"
                        + "    window.onAnimationFrame = function(timestamp) {"
                        + "        document.title=++raf_count;"
                        + "    };"
                        + "</script>"
                        + " Hello world!</body></html>";
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        // Loading the html via a data URI requires us to encode '#' symbols as '%23'.
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "data:text/html," + html.replace("#", "%23"));

        final TitleUpdatedHelper onTitleUpdatedHelper = new TitleUpdatedHelper();
        final WebContentsObserver web_contents_observer =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new WebContentsObserver(mAwContents.getWebContents()) {
                                    @Override
                                    public void titleWasSet(String title) {
                                        onTitleUpdatedHelper.setTitle(title);
                                    }
                                });

        int callCount = onTitleUpdatedHelper.getCallCount();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(mAwContents.getWebContents(), CALL_RAF);
        onTitleUpdatedHelper.waitForCallback(callCount);
        Assert.assertEquals("1", onTitleUpdatedHelper.getTitle());
        callCount = onTitleUpdatedHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.onPause();
                    Assert.assertFalse(mAwContents.isPageVisible());
                });

        // VisualStateCallback#onComplete won't be called when WebView is
        // invisible. So there is no reliable way to tell if View#setVisibility
        // has taken effect. Instead we will continue to run rAF until either frame production stops
        // or the polling times out in 1.5s. This timeout will emit a test failure.
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    int callCount2 = onTitleUpdatedHelper.getCallCount();
                    // Even though we are hidden, the JS should successfully run to request the rAF.
                    // However frame production should be disabled, so the actual frame should not
                    // run, nor update the title.
                    JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            mAwContents.getWebContents(), CALL_RAF);

                    try {
                        onTitleUpdatedHelper.waitForCallback(
                                callCount2, 1, 500, TimeUnit.MILLISECONDS);
                        // If we produced a frame fail this run of the polling. Polling will re-run
                        // until success or timeout.
                        return false;
                    } catch (TimeoutException e) {
                        // Timeout is expected.
                        return true;
                    }
                });
        ThreadUtils.runOnUiThreadBlocking(() -> web_contents_observer.observe(null));
    }
}
