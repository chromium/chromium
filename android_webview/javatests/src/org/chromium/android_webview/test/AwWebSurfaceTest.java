// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.os.Build;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.ViewConfiguration;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwViewAndroidDelegate;
import org.chromium.android_webview.AwWebContent;
import org.chromium.android_webview.AwWebSurface;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.net.test.util.TestWebServer;

/** Tests for {@link AwWebSurface}. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
// Test drags are sent at once, so latency compensation could drop their scroll updates.
@CommandLineFlags.Add({"disable-features=CompensateGestureScrollUpdateLatency"})
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

        TestEventListener listener = new TestEventListener();
        AwWebSurface surface = new AwWebSurface(listener);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int callCount = listener.mInvalidateHelper.getCallCount();
                    surface.setAwContents(mAwContents);
                    assertEquals(callCount + 1, listener.mInvalidateHelper.getCallCount());

                    surface.setAwContents(secondContents);
                    assertEquals(callCount + 2, listener.mInvalidateHelper.getCallCount());

                    surface.setAwContents(null);
                    assertEquals(callCount + 3, listener.mInvalidateHelper.getCallCount());

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
        TestEventListener listener = new TestEventListener();
        AwWebSurface surface = new AwWebSurface(listener);

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(mAwContents));
        int callCount = listener.mInvalidateHelper.getCallCount();

        mActivityTestRule.loadHtmlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html><body style='background-color: #00FF00;'>Test</body></html>");
        listener.mInvalidateHelper.waitForCallback(callCount);

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(null));
        callCount = listener.mInvalidateHelper.getCallCount();

        mActivityTestRule.loadHtmlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html><body style='background-color: #FF0000;'>Second</body></html>");
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        assertEquals(callCount, listener.mInvalidateHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDraw() throws Throwable {
        AwWebSurface surface = new AwWebSurface(new TestEventListener());

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
        TestEventListener listener =
                new TestEventListener() {
                    @Override
                    public void onInvalidate() {
                        super.onInvalidate();
                        mContainerView.invalidate();
                    }
                };
        AwWebSurface surface = new AwWebSurface(listener);

        mActivityTestRule.loadHtmlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html><body style='height: 2000px;'></body></html>");
        mActivityTestRule.waitForVisualStateCallback(mAwContents);

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(mAwContents));

        int callCount = listener.mInvalidateHelper.getCallCount();
        AwTestTouchUtils.dragCompleteView(mContainerView, 0, 0, 0, -200, 10);

        CriteriaHelper.pollInstrumentationThread(() -> mAwContents.getScrollY() > 0);
        listener.mInvalidateHelper.waitForCallback(callCount);

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
    public void testOverscroll() throws Throwable {
        CallbackHelper overScrollHelper = new CallbackHelper();
        float[] firstOverScrollDelta = new float[2];
        AwWebSurface surface =
                new AwWebSurface(
                        new TestEventListener() {
                            @Override
                            public void onInvalidate() {
                                super.onInvalidate();
                                mContainerView.invalidate();
                            }

                            @Override
                            public void onOverScroll(float dx, float dy, float vx, float vy) {
                                if (overScrollHelper.getCallCount() > 0) return;
                                firstOverScrollDelta[0] = dx;
                                firstOverScrollDelta[1] = dy;
                                overScrollHelper.notifyCalled();
                            }
                        });

        mActivityTestRule.loadHtmlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html><body style='margin:0; height: 100px;'></body></html>");
        mActivityTestRule.waitForVisualStateCallback(mAwContents);

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(mAwContents));

        // Drag down in a single move, so everything beyond the touch slop overscrolls at once.
        long downTime = SystemClock.uptimeMillis();
        sendTouchEvent(surface, downTime, MotionEvent.ACTION_DOWN, 50, 50);
        sendTouchEvent(surface, downTime, MotionEvent.ACTION_MOVE, 50, 250);
        overScrollHelper.waitForCallback(0);
        sendTouchEvent(surface, downTime, MotionEvent.ACTION_UP, 50, 250);

        // The overscroll is in physical pixels and follows the finger, like the drag.
        int touchSlop = ViewConfiguration.get(mContainerView.getContext()).getScaledTouchSlop();
        assertEquals(0f, firstOverScrollDelta[0], 0f);
        assertEquals(200 - touchSlop, firstOverScrollDelta[1], 2f);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverscrollFromFlingReportsVelocity() throws Throwable {
        CallbackHelper flingOverScrollHelper = new CallbackHelper();
        float[] lastVelocity = new float[2];
        AwWebSurface surface =
                new AwWebSurface(
                        new TestEventListener() {
                            @Override
                            public void onInvalidate() {
                                super.onInvalidate();
                                mContainerView.invalidate();
                            }

                            @Override
                            public void onOverScroll(float dx, float dy, float vx, float vy) {
                                if (vx == 0 && vy == 0) return;
                                lastVelocity[0] = vx;
                                lastVelocity[1] = vy;
                                flingOverScrollHelper.notifyCalled();
                            }
                        });

        mActivityTestRule.loadHtmlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html><body style='margin:0; height: 100px;'></body></html>");
        mActivityTestRule.waitForVisualStateCallback(mAwContents);

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(mAwContents));

        int callCount = flingOverScrollHelper.getCallCount();

        // Fling up (towards the top edge) on a page that cannot scroll.
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.flingScroll(0, -1000));
        flingOverScrollHelper.waitForCallback(callCount);
        assertEquals(0f, lastVelocity[0], 0f);
        assertTrue(lastVelocity[1] > 0);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetSize() {
        AwWebSurface surface = new AwWebSurface(new TestEventListener());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // AwContents reports its height as the vertical scroll extent.
                    surface.setAwContents(mAwContents);
                    surface.setSize(320, 480);
                    assertEquals(480, mAwContents.getViewMethods().computeVerticalScrollExtent());

                    surface.setAwContents(null);
                    surface.setSize(500, 500);
                    assertEquals(480, mAwContents.getViewMethods().computeVerticalScrollExtent());
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFullscreenDisabledWhileBound() throws Throwable {
        mAwContents.getSettings().setJavaScriptEnabled(true);
        mAwContents.getSettings().setFullscreenSupported(true);
        mActivityTestRule.loadHtmlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), "<html></html>");

        AwWebSurface surface = new AwWebSurface(new TestEventListener());
        assertEquals(
                "true",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "document.fullscreenEnabled"));

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(mAwContents));
        assertEquals(
                "false",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "document.fullscreenEnabled"));

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(null));
        assertEquals(
                "true",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "document.fullscreenEnabled"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    public void testNoViewportInsetBelowWindowWhileBound() {
        AwViewAndroidDelegate viewAndroidDelegate = mAwContents.getViewAndroidDelegateForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Make the container view extend below the window, as it does for a tall
                    // WebSurface in a scroll container. ScrollView measures its child with an
                    // unspecified height, so fix the container view's height inside a FrameLayout.
                    mActivityTestRule.getActivity().removeAllViews();
                    int width = mActivityTestRule.getActivity().getRootLayoutWidth();
                    FrameLayout content = new FrameLayout(mActivityTestRule.getActivity());
                    content.addView(mContainerView, new FrameLayout.LayoutParams(width, 6000));
                    ScrollView scrollView = new ScrollView(mActivityTestRule.getActivity());
                    scrollView.addView(content, new FrameLayout.LayoutParams(width, 6000));
                    mActivityTestRule.getActivity().addView(scrollView);
                });
        // Without a WebSurface, the part below the window is inset like the IME. Recalculate on
        // each poll, since the view only gets its new size on a later layout pass.
        CriteriaHelper.pollUiThread(
                () -> {
                    mAwContents.getDisplayCutoutController().recalculateBottomImeInset();
                    return viewAndroidDelegate.getViewportInsetBottom() > 0;
                });

        AwWebSurface surface = new AwWebSurface(new TestEventListener());
        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(mAwContents));
        CriteriaHelper.pollUiThread(
                () -> viewAndroidDelegate.getViewportInsetBottom() == 0,
                "The part of the WebSurface below the window was inset.");

        ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(null));
        CriteriaHelper.pollUiThread(
                () -> viewAndroidDelegate.getViewportInsetBottom() > 0,
                "The inset wasn't restored after unbinding the WebSurface.");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWebContent_surfaceBindingMutualExclusion() {
        AwWebContent webContent = new AwWebContent();

        CallbackHelper listener1Helper = new CallbackHelper();
        CallbackHelper listener2Helper = new CallbackHelper();
        AwWebContent.SurfaceBindingListener listener1 =
                awContents -> listener1Helper.notifyCalled();
        AwWebContent.SurfaceBindingListener listener2 =
                awContents -> listener2Helper.notifyCalled();

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
        TestEventListener listener1 = new TestEventListener();
        TestEventListener listener2 = new TestEventListener();
        AwWebSurface surface1 = new AwWebSurface(listener1);
        AwWebSurface surface2 = new AwWebSurface(listener2);
        AwWebContent webContent = new AwWebContent();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    surface1.setWebContent(webContent);

                    int callCount1 = listener1.mInvalidateHelper.getCallCount();
                    surface2.setWebContent(webContent);
                    assertEquals(callCount1 + 1, listener1.mInvalidateHelper.getCallCount());

                    int callCount2 = listener2.mInvalidateHelper.getCallCount();
                    surface1.setWebContent(webContent);
                    assertEquals(callCount2 + 1, listener2.mInvalidateHelper.getCallCount());

                    surface1.setWebContent(null);
                    surface2.setWebContent(null);
                    webContent.destroy();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPopupContentsRebindSurface() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mActivityTestRule.triggerPopup(
                    mAwContents,
                    mContentsClient,
                    webServer,
                    "<html></html>",
                    "<html><head><title>Popup</title></head>"
                            + "<body style='margin:0; height: 100px;'></body></html>",
                    "/popup.html",
                    "window.open('/popup.html')");

            // Bind the surface before the popup WebContents is supplied, like AwWebSurface does.
            AwActivityTestRule.PopupInfo popupInfo =
                    mActivityTestRule.createPopupContents(mAwContents);
            CallbackHelper overScrollHelper = new CallbackHelper();
            AwWebSurface surface =
                    new AwWebSurface(
                            new TestEventListener() {
                                @Override
                                public void onInvalidate() {
                                    popupInfo.popupContainerView.invalidate();
                                }

                                @Override
                                public void onOverScroll(float dx, float dy, float vx, float vy) {
                                    overScrollHelper.notifyCalled();
                                }
                            });
            ThreadUtils.runOnUiThreadBlocking(() -> surface.setAwContents(popupInfo.popupContents));
            mActivityTestRule.loadPopupContents(mAwContents, popupInfo, null);
            mActivityTestRule.waitForVisualStateCallback(popupInfo.popupContents);

            // Overscroll is reported by the native peer, which must follow the popup WebContents.
            ThreadUtils.runOnUiThreadBlocking(() -> popupInfo.popupContents.flingScroll(0, -1000));
            overScrollHelper.waitForCallback(0);
        } finally {
            webServer.shutdown();
        }
    }

    /** Records invalidations and leaves all scrolling to the page. */
    private static class TestEventListener implements AwWebSurface.EventListener {
        final CallbackHelper mInvalidateHelper = new CallbackHelper();

        @Override
        public void onInvalidate() {
            mInvalidateHelper.notifyCalled();
        }

        @Override
        public void onOverScroll(float deltaX, float deltaY, float velocityX, float velocityY) {}
    }

    private static void sendTouchEvent(
            AwWebSurface surface, long downTime, int action, float x, float y) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MotionEvent event =
                            MotionEvent.obtain(
                                    downTime, SystemClock.uptimeMillis(), action, x, y, 0);
                    surface.onTouchEvent(event);
                    event.recycle();
                });
    }
}
