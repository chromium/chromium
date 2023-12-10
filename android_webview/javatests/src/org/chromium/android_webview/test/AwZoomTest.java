// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Activity;
import android.content.pm.ActivityInfo;
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
    private static final float MAXIMUM_SCALE = 2.0f;
    private static final float EPSILON = 0.00001f;

    public AwZoomTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    private String getZoomableHtml(float scale) {
        final int divWidthPercent = (int) (100.0f / scale);
        return String.format(
                Locale.US,
                "<html><head><meta name=\"viewport\" content=\"width=device-width,"
                        + " minimum-scale=%f, maximum-scale=%f, initial-scale=%f\"/></head><body"
                        + " style='margin:0'><div style='width:%d%%;height:100px;border:1px solid"
                        + " black'>Zoomable</div></body></html>",
                scale,
                MAXIMUM_SCALE,
                scale,
                divWidthPercent);
    }

    private String getNonZoomableHtml() {
        // This page can't be zoomed because its viewport fully occupies
        // view area and is explicitly made non user-scalable.
        return "<html><head>"
                + "<meta name=\"viewport\" "
                + "content=\"width=device-width,height=device-height,"
                + "initial-scale=1,maximum-scale=1,user-scalable=no\">"
                + "</head><body>Non-zoomable</body></html>";
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
}
