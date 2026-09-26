// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.os.SystemClock;
import android.view.MotionEvent;

import androidx.test.filters.SmallTest;

import com.android.webview.chromium.WebContent;
import com.android.webview.chromium.WebSurface;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwWebSurface;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;

/** Tests for {@link AwWebSurface}. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwWebSurfaceTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mContainerView;
    private AwContents mAwContents;

    public AwWebSurfaceTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mContainerView.getAwContents();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetAwContents() {
        TestAwContentsClient secondClient = new TestAwContentsClient();
        AwTestContainerView secondContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(secondClient);
        AwContents secondContents = secondContainerView.getAwContents();

        CallbackHelper invalidateHelper = new CallbackHelper();
        AwWebSurface surface = new AwWebSurface(invalidateHelper::notifyCalled);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int callCount = invalidateHelper.getCallCount();
                    surface.setAwContents(mAwContents);
                    assertEquals(callCount + 1, invalidateHelper.getCallCount());

                    surface.setAwContents(secondContents);
                    assertEquals(callCount + 2, invalidateHelper.getCallCount());

                    surface.setAwContents(null);
                    assertEquals(callCount + 3, invalidateHelper.getCallCount());

                    long now = SystemClock.uptimeMillis();
                    MotionEvent event =
                            MotionEvent.obtain(now, now, MotionEvent.ACTION_DOWN, 50.0f, 50.0f, 0);
                    assertFalse(surface.onTouchEvent(event));
                    event.recycle();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCompositorInvalidation() throws Throwable {
        CallbackHelper invalidateHelper = new CallbackHelper();
        AwWebSurface surface = new AwWebSurface(invalidateHelper::notifyCalled);

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(mAwContents));
        int callCount = invalidateHelper.getCallCount();

        mActivityTestRule.loadHtmlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html><body style='background-color: #00FF00;'>Test</body></html>");
        invalidateHelper.waitForCallback(callCount);

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(null));
        callCount = invalidateHelper.getCallCount();

        mActivityTestRule.loadHtmlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html><body style='background-color: #FF0000;'>Second</body></html>");
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        assertEquals(callCount, invalidateHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDraw() throws Throwable {
        AwWebSurface surface = new AwWebSurface(() -> {});

        final int green = Color.rgb(0, 255, 0);
        mActivityTestRule.loadHtmlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html><body style='margin:0; background-color: rgb(0, 255, 0);'></body></html>");
        mActivityTestRule.waitForVisualStateCallback(mAwContents);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    surface.setAwContents(mAwContents);
                    surface.setSize(100, 100);

                    Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
                    Canvas canvas = new Canvas(bitmap);

                    int saveCount = canvas.getSaveCount();
                    surface.draw(canvas);

                    assertEquals(saveCount, canvas.getSaveCount());
                    assertEquals(green, bitmap.getPixel(50, 50));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSwipe() throws Throwable {
        CallbackHelper invalidateHelper = new CallbackHelper();
        AwWebSurface surface = new AwWebSurface(invalidateHelper::notifyCalled);

        mActivityTestRule.loadHtmlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html><body style='height: 2000px;'></body></html>");
        mActivityTestRule.waitForVisualStateCallback(mAwContents);

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(mAwContents));

        int callCount = invalidateHelper.getCallCount();
        AwTestTouchUtils.dragCompleteView(mContainerView, 0, 0, 0, -200, 10);

        CriteriaHelper.pollInstrumentationThread(() -> mAwContents.getScrollY() > 0);
        invalidateHelper.waitForCallback(callCount);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
                    Canvas canvas = new Canvas(bitmap);
                    int saveCount = canvas.getSaveCount();
                    surface.draw(canvas);
                    assertEquals(saveCount, canvas.getSaveCount());
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetSize() throws Throwable {
        AwWebSurface surface = new AwWebSurface(() -> {});
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mActivityTestRule.loadHtmlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html><head><script>"
                        + "window.resizeCount = 0;"
                        + "window.addEventListener('resize', () => { window.resizeCount++; });"
                        + "</script></head><body></body></html>");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    surface.setAwContents(mAwContents);
                    surface.setSize(320, 480);
                });

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String count =
                                mActivityTestRule.executeJavaScriptAndWaitForResult(
                                        mAwContents, mContentsClient, "window.resizeCount");
                        return Integer.parseInt(count) > 0;
                    } catch (Exception e) {
                        return false;
                    }
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    surface.setAwContents(null);
                    surface.setSize(500, 500);
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWebContent_surfaceBindingMutualExclusion() {
        WebContent webContent = new WebContent();

        CallbackHelper listener1Helper = new CallbackHelper();
        CallbackHelper listener2Helper = new CallbackHelper();
        WebContent.SurfaceBindingListener listener1 = awContents -> listener1Helper.notifyCalled();
        WebContent.SurfaceBindingListener listener2 = awContents -> listener2Helper.notifyCalled();

        webContent.bindSurface(listener1);
        assertEquals(1, listener1Helper.getCallCount());

        webContent.bindSurface(listener2);
        assertEquals(2, listener1Helper.getCallCount());
        assertEquals(1, listener2Helper.getCallCount());

        webContent.bindSurface(null);
        assertEquals(2, listener2Helper.getCallCount());

        webContent.bindSurface(listener1);
        assertEquals(3, listener1Helper.getCallCount());

        webContent.destroy();
        assertEquals(4, listener1Helper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMultipleWebSurfaces_evictionTriggersInvalidation() {
        CallbackHelper invalidateHelper1 = new CallbackHelper();
        CallbackHelper invalidateHelper2 = new CallbackHelper();
        WebSurface surface1 = new WebSurface(invalidateHelper1::notifyCalled);
        WebSurface surface2 = new WebSurface(invalidateHelper2::notifyCalled);
        WebContent webContent = new WebContent();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    surface1.setWebContent(webContent);

                    int callCount1 = invalidateHelper1.getCallCount();
                    surface2.setWebContent(webContent);
                    assertEquals(callCount1 + 1, invalidateHelper1.getCallCount());

                    int callCount2 = invalidateHelper2.getCallCount();
                    surface1.setWebContent(webContent);
                    assertEquals(callCount2 + 1, invalidateHelper2.getCallCount());

                    surface1.setWebContent(null);
                    surface2.setWebContent(null);
                    webContent.destroy();
                });
    }
}
