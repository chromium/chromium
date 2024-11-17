// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestThreadUtils;

import java.util.Locale;

/** A test suite for zooming-related methods and settings. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwZoomTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private AwTestContainerView mContainerView;
    private static final float MAXIMUM_SCALE = 2.0f;
    private static final float EPSILON = 0.00001f;

    public AwZoomTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mContainerView.getAwContents();
    }

    private String getZoomableHtml(float scale) {
        final int divWidthPercent = (int) (100.0f / scale);
        return String.format(
                Locale.US,
                """
            <html>
              <head>
                <meta name="viewport" content="width=device-width,
                      minimum-scale=%f, maximum-scale=%f, initial-scale=%f"/>
              </head>
              <body style='margin:0'>
                <div style='width:%d%%;height:100px;border:1px solid black'>Zoomable</div>
              </body>
            </html>
            """,
                scale,
                MAXIMUM_SCALE,
                scale,
                divWidthPercent);
    }

    private String getNonZoomableHtml() {
        // This page can't be zoomed because its viewport fully occupies
        // view area and is explicitly made non user-scalable.
        return """
            <html>
              <head>
                <meta name="viewport" content="width=device-width,height=device-height,
                      initial-scale=1,maximum-scale=1,user-scalable=no">
              </head>
              <body>
                Non-zoomable
              </body>
            </html>
            """;
    }

    private boolean isMultiTouchZoomSupportedOnUiThread() throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.isMultiTouchZoomSupported());
    }

    private int getVisibilityOnUiThread(final View view) throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(() -> view.getVisibility());
    }

    private View getZoomControlsViewOnUiThread() throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getZoomControlsViewForTest());
    }

    private boolean canZoomInUsingZoomControls() throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getZoomControlsForTest().canZoomIn());
    }

    private boolean canZoomOutUsingZoomControls() throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getZoomControlsForTest().canZoomOut());
    }

    private void invokeZoomPickerOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.invokeZoomPicker());
        // Zoom picker is updated asynchronously.
        TestThreadUtils.flushNonDelayedLooperTasks();
    }

    private void zoomInOnUiThreadAndWait() throws Throwable {
        final float previousScale = mActivityTestRule.getPixelScaleOnUiThread(mAwContents);
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.zoomIn()));
        // The zoom level is updated asynchronously.
        waitForScaleChange(previousScale);
    }

    private void zoomOutOnUiThreadAndWait() throws Throwable {
        final float previousScale = mActivityTestRule.getPixelScaleOnUiThread(mAwContents);
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.zoomOut()));
        // The zoom level is updated asynchronously.
        waitForScaleChange(previousScale);
    }

    private void zoomByOnUiThreadAndWait(final float delta) throws Throwable {
        final float previousScale = mActivityTestRule.getPixelScaleOnUiThread(mAwContents);
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.zoomBy(delta));
        // The zoom level is updated asynchronously.
        waitForScaleChange(previousScale);
    }

    private void waitForScaleChange(final float previousScale) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> previousScale != mActivityTestRule.getPixelScaleOnUiThread(mAwContents));
    }

    private void waitForScaleToBecome(final float expectedScale) {
        AwActivityTestRule.pollInstrumentationThread(
                () ->
                        Math.abs(expectedScale - mActivityTestRule.getScaleOnUiThread(mAwContents))
                                < EPSILON);
    }

    private void waitUntilCanNotZoom() {
        AwActivityTestRule.pollInstrumentationThread(
                () ->
                        !mActivityTestRule.canZoomInOnUiThread(mAwContents)
                                && !mActivityTestRule.canZoomOutOnUiThread(mAwContents));
    }

    private void runMagnificationTest() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setUseWideViewPort(true);
        Assert.assertFalse(
                "Should not be able to zoom in",
                mActivityTestRule.canZoomInOnUiThread(mAwContents));
        final float pageMinimumScale = 0.5f;
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(pageMinimumScale),
                "text/html",
                false);
        waitForScaleToBecome(pageMinimumScale);
        Assert.assertTrue(
                "Should be able to zoom in", mActivityTestRule.canZoomInOnUiThread(mAwContents));
        Assert.assertFalse(
                "Should not be able to zoom out",
                mActivityTestRule.canZoomOutOnUiThread(mAwContents));

        while (mActivityTestRule.canZoomInOnUiThread(mAwContents)) {
            zoomInOnUiThreadAndWait();
        }
        Assert.assertTrue(
                "Should be able to zoom out", mActivityTestRule.canZoomOutOnUiThread(mAwContents));

        while (mActivityTestRule.canZoomOutOnUiThread(mAwContents)) {
            zoomOutOnUiThreadAndWait();
        }
        Assert.assertTrue(
                "Should be able to zoom in", mActivityTestRule.canZoomInOnUiThread(mAwContents));

        zoomByOnUiThreadAndWait(4.0f);
        waitForScaleToBecome(MAXIMUM_SCALE);

        zoomByOnUiThreadAndWait(0.5f);
        waitForScaleToBecome(MAXIMUM_SCALE * 0.5f);

        zoomByOnUiThreadAndWait(0.01f);
        waitForScaleToBecome(pageMinimumScale);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMagnification() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSupportZoom(true);
        runMagnificationTest();
    }

    // According to Android CTS test, zoomIn/Out must work
    // even if supportZoom is turned off.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMagnificationWithZoomSupportOff() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSupportZoom(false);
        runMagnificationTest();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setSupportZoom(true)")
    public void testZoomUsingMultiTouch() throws Throwable {
        AwSettings webSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(0.5f),
                "text/html",
                false);

        Assert.assertTrue(webSettings.supportZoom());
        Assert.assertFalse(webSettings.getBuiltInZoomControls());
        Assert.assertFalse(isMultiTouchZoomSupportedOnUiThread());

        webSettings.setBuiltInZoomControls(true);
        Assert.assertTrue(isMultiTouchZoomSupportedOnUiThread());

        webSettings.setSupportZoom(false);
        Assert.assertFalse(isMultiTouchZoomSupportedOnUiThread());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setSupportZoom(true)")
    public void testZoomControls() throws Throwable {
        AwSettings webSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        webSettings.setUseWideViewPort(true);
        Assert.assertFalse(
                "Should not be able to zoom in",
                mActivityTestRule.canZoomInOnUiThread(mAwContents));
        final float pageMinimumScale = 0.5f;
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(pageMinimumScale),
                "text/html",
                false);
        waitForScaleToBecome(pageMinimumScale);
        // It must be possible to zoom in (or zoom out) for zoom controls to be shown
        Assert.assertTrue(
                "Should be able to zoom in", mActivityTestRule.canZoomInOnUiThread(mAwContents));

        Assert.assertTrue(webSettings.supportZoom());
        webSettings.setBuiltInZoomControls(true);
        webSettings.setDisplayZoomControls(false);

        // With DisplayZoomControls set to false, attempts to display zoom
        // controls must be ignored.
        Assert.assertNull(getZoomControlsViewOnUiThread());
        invokeZoomPickerOnUiThread();
        Assert.assertNull(getZoomControlsViewOnUiThread());

        webSettings.setDisplayZoomControls(true);
        Assert.assertNull(getZoomControlsViewOnUiThread());
        invokeZoomPickerOnUiThread();
        View zoomControls = getZoomControlsViewOnUiThread();
        Assert.assertEquals(View.VISIBLE, getVisibilityOnUiThread(zoomControls));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setSupportZoom(true)")
    public void testZoomControlsUiIsUpdatedOnChanges() throws Throwable {
        AwSettings webSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        webSettings.setDisplayZoomControls(true);
        webSettings.setBuiltInZoomControls(true);
        webSettings.setUseWideViewPort(true);
        Assert.assertFalse(canZoomInUsingZoomControls());
        Assert.assertFalse(canZoomOutUsingZoomControls());
        final float pageMinimumScale = 0.5f;
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(pageMinimumScale),
                "text/html",
                false);
        waitForScaleToBecome(pageMinimumScale);
        Assert.assertTrue(canZoomInUsingZoomControls());
        Assert.assertFalse(canZoomOutUsingZoomControls());

        zoomInOnUiThreadAndWait();
        Assert.assertTrue(canZoomInUsingZoomControls());
        Assert.assertTrue(canZoomOutUsingZoomControls());

        zoomOutOnUiThreadAndWait();
        Assert.assertTrue(canZoomInUsingZoomControls());
        Assert.assertFalse(canZoomOutUsingZoomControls());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setSupportZoom(true)")
    public void testZoomControlsOnNonZoomableContent() throws Throwable {
        AwSettings webSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getNonZoomableHtml(),
                "text/html",
                false);

        // ContentView must update itself according to the viewport setup.
        waitUntilCanNotZoom();

        Assert.assertTrue(webSettings.supportZoom());
        webSettings.setBuiltInZoomControls(true);
        webSettings.setDisplayZoomControls(true);
        Assert.assertNull(getZoomControlsViewOnUiThread());
        invokeZoomPickerOnUiThread();
        View zoomControls = getZoomControlsViewOnUiThread();
        Assert.assertEquals(View.GONE, getVisibilityOnUiThread(zoomControls));
    }

    @Test
    @DisableHardwareAcceleration
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setSupportZoom(true)")
    public void testZoomControlsOnOrientationChange() throws Throwable {
        AwSettings webSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(0.5f),
                "text/html",
                false);

        Assert.assertTrue(webSettings.supportZoom());
        webSettings.setBuiltInZoomControls(true);
        webSettings.setDisplayZoomControls(true);
        invokeZoomPickerOnUiThread();

        // Now force an orientation change, and try to display the zoom picker
        // again. Make sure that we don't crash when the ZoomPicker registers
        // its receiver.

        Activity activity = mActivityTestRule.getActivity();
        int orientation = activity.getRequestedOrientation();
        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        activity.setRequestedOrientation(orientation);
        invokeZoomPickerOnUiThread();

        // We may crash shortly (as the zoom picker has a short delay in it before
        // it tries to register its BroadcastReceiver), so sleep to verify we don't.
        // The delay is encoded in ZoomButtonsController#ZOOM_CONTROLS_TIMEOUT,
        // if that changes we may need to update this test.
        Thread.sleep(ViewConfiguration.getZoomControlsTimeout());
    }

    @Test
    @DisableHardwareAcceleration
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setSupportZoom(true)")
    public void testCtrlPlusMouseScrollZoomsIn() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSupportZoom(true);
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setUseWideViewPort(true);
        Assert.assertFalse(
                "Should not be able to zoom in",
                mActivityTestRule.canZoomInOnUiThread(mAwContents));

        float pageMinimumScale = 0.5f;
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getZoomableHtml(pageMinimumScale),
                "text/html",
                false);
        waitForScaleToBecome(pageMinimumScale);

        Assert.assertTrue(
                "Should be able to zoom in", mActivityTestRule.canZoomInOnUiThread(mAwContents));
        Assert.assertFalse(
                "Should not be able to zoom out",
                mActivityTestRule.canZoomOutOnUiThread(mAwContents));

        AwSettings webSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        Assert.assertTrue(webSettings.supportZoom());

        // Simulate scroll event by sending it to the center of the View.
        float mCurrentX = mContainerView.getWidth() / 2f;
        float mCurrentY = mContainerView.getHeight() / 2f;

        int ctrlMetaState = KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_RIGHT_ON;
        // Positive vertical scroll value for zoom in.
        float vScrollValue = 1.0f;

        float previousScale = mActivityTestRule.getPixelScaleOnUiThread(mAwContents);

        mouseScroll(mContainerView, mCurrentX, mCurrentY, ctrlMetaState, vScrollValue);

        // Page should zoom in.
        waitForScaleChange(previousScale);

        float currentScale = mActivityTestRule.getPixelScaleOnUiThread(mAwContents);
        Assert.assertTrue(currentScale > previousScale);
    }

    @Test
    @DisableHardwareAcceleration
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setSupportZoom(true)")
    public void testCtrlPlusMouseScrollZoomsOut() throws Throwable {
        // Page has to be zoomed in before zooming out.
        testCtrlPlusMouseScrollZoomsIn();

        Assert.assertTrue(
                "Should be able to zoom out", mActivityTestRule.canZoomOutOnUiThread(mAwContents));

        // Simulate scroll event by sending it to the center of the View.
        float mCurrentX = mContainerView.getWidth() / 2f;
        float mCurrentY = mContainerView.getHeight() / 2f;

        int ctrlMetaState = KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_RIGHT_ON;
        // Negative vertical scroll value for zoom out.
        float vScrollValue = -1.0f;

        final float previousScale = mActivityTestRule.getPixelScaleOnUiThread(mAwContents);

        mouseScroll(mContainerView, mCurrentX, mCurrentY, ctrlMetaState, vScrollValue);

        // Page should zoom out.
        waitForScaleChange(previousScale);

        float currentScale = mActivityTestRule.getPixelScaleOnUiThread(mAwContents);
        Assert.assertTrue(currentScale < previousScale);
    }

    /**
     * Sends a mouse scroll event to the View at the specified coordinates.
     *
     * @param view The view the coordinates are relative to.
     * @param x Relative x location to the view.
     * @param y Relative y location to the view.
     * @param metaState This can be used to pass any meta key states, like CTRL or SHIFT.
     * @param vScroll This value is normalized to a range from -1.0 (down) to 1.0 (up). In other
     *     words, positive value scrolls up moving the page content up and vice versa.
     */
    private static void mouseScroll(View view, float x, float y, int metaState, float vScroll) {

        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();

        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[1];
        coords[0] = new MotionEvent.PointerCoords();
        coords[0].x = x;
        coords[0].y = y;

        coords[0].setAxisValue(MotionEvent.AXIS_VSCROLL, vScroll);

        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[1];
        properties[0] = new MotionEvent.PointerProperties();
        properties[0].id = 0;
        properties[0].toolType = MotionEvent.TOOL_TYPE_MOUSE;

        MotionEvent event =
                MotionEvent.obtain(
                        downTime,
                        eventTime,
                        MotionEvent.ACTION_SCROLL,
                        1,
                        properties,
                        coords,
                        metaState,
                        0,
                        0.0f,
                        0.0f,
                        0,
                        0,
                        InputDevice.SOURCE_MOUSE,
                        0);

        ThreadUtils.runOnUiThreadBlocking(() -> view.dispatchGenericMotionEvent(event));
    }
}
