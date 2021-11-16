// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.os.CancellationSignal;
import android.util.Size;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * A RenderTest for {@link ScrollCaptureCallbackImp}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.SCROLL_CAPTURE})
@Batch(Batch.PER_CLASS)
public class ScrollCaptureCallbackRenderTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public RenderTestRule mRenderTestRule = RenderTestRule.Builder.withPublicCorpus().build();

    private ScrollCaptureCallbackDelegate mCallback;
    private Tab mTab;
    private TextureView mTextureView;
    private Bitmap mBitmap;
    private Rect mCapturedRect;

    @Before
    public void setUp() {
        // TODO(https://crbug.com/1254801): create a page that checkboards in a gradient or
        // something more complex to generate better test images.
        GURL url = new GURL(sActivityTestRule.getTestServer().getURL(
                "/chrome/test/data/android/very_long_google.html"));
        sActivityTestRule.loadUrl(url.getSpec());
        mCallback = new ScrollCaptureCallbackDelegate(
                new ScrollCaptureCallbackDelegate.EntryManagerWrapper());
        mTab = sActivityTestRule.getActivity().getActivityTab();
        mCallback.setCurrentTab(mTab);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testCaptureTop() throws Exception {
        View view = mTab.getView();
        Size size = new Size(view.getWidth(), view.getHeight());

        CallbackHelper surfaceChanged = new CallbackHelper();
        createTextureView(size, surfaceChanged);

        driveScrollCapture(size, surfaceChanged, "scroll_capture_top");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @FlakyTest(message = "https://crbug.com/1269583")
    public void testCaptureBottom() throws Exception {
        WebContents webContents = mTab.getWebContents();
        RenderCoordinates renderCoordinates = RenderCoordinates.fromWebContents(webContents);
        // Drive a scroll to the bottom of the page.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            webContents.getEventForwarder().scrollBy(0,
                    renderCoordinates.getContentHeightPixInt()
                            - renderCoordinates.getLastFrameViewportHeightPixInt());
        });
        View view = mTab.getView();
        Size size = new Size(view.getWidth(), view.getHeight());

        CallbackHelper surfaceChanged = new CallbackHelper();
        createTextureView(size, surfaceChanged);

        driveScrollCapture(size, surfaceChanged, "scroll_capture_bottom");
    }

    /**
     * Creates a texture view to draw bitmaps to.
     */
    private void createTextureView(Size size, CallbackHelper surfaceChanged)
            throws TimeoutException {
        CallbackHelper surfaceReady = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTextureView = new TextureView(ContextUtils.getApplicationContext());
            mTextureView.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
                @Override
                public boolean onSurfaceTextureDestroyed(SurfaceTexture texture) {
                    return true;
                }

                @Override
                public void onSurfaceTextureAvailable(
                        SurfaceTexture texture, int width, int height) {
                    surfaceReady.notifyCalled();
                }

                @Override
                public void onSurfaceTextureSizeChanged(
                        SurfaceTexture texture, int width, int height) {}

                @Override
                public void onSurfaceTextureUpdated(SurfaceTexture texture) {
                    mBitmap = mTextureView.getBitmap();
                    surfaceChanged.notifyCalled();
                }
            });
            ViewGroup group =
                    (ViewGroup) sActivityTestRule.getActivity().findViewById(android.R.id.content);
            group.addView(mTextureView, new LayoutParams(size.getWidth(), size.getHeight()));
        });
        surfaceReady.waitForFirst();
    }

    /**
     * Drives a scroll capture several viewports above and below the current viewport location.
     */
    private void driveScrollCapture(Size initialSize, CallbackHelper surfaceChanged, String tag)
            throws Exception {
        CancellationSignal signal = new CancellationSignal();

        // Start the session.
        CallbackHelper ready = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Rect r = mCallback.onScrollCaptureSearch(signal);
            mCallback.onScrollCaptureStart(signal, ready::notifyCalled);
            Assert.assertFalse(r.isEmpty());
        });
        ready.waitForFirst();

        Surface surface = new Surface(mTextureView.getSurfaceTexture());
        // Current viewport should always succeed.
        Assert.assertTrue(captureViewport(signal, surface, initialSize, surfaceChanged, tag, 0));
        // Below.
        int index = 1;
        while (captureViewport(signal, surface, initialSize, surfaceChanged, tag, index)) {
            ++index;
        }
        // Above.
        index = -1;
        while (captureViewport(signal, surface, initialSize, surfaceChanged, tag, index)) {
            --index;
        }
        surface.release();

        // End the session.
        CallbackHelper finished = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mCallback.onScrollCaptureEnd(finished::notifyCalled); });
        finished.waitForFirst();
    }

    /**
     * Captures the viewport at i * initialSize.getHeight() offset to the current viewport.
     */
    private boolean captureViewport(CancellationSignal signal, Surface surface, Size initialSize,
            CallbackHelper surfaceChanged, String tag, int index) throws Exception {
        final int callCount = surfaceChanged.getCallCount();
        CallbackHelper bitmapReady = new CallbackHelper();
        Callback<Rect> consume = r -> {
            mCapturedRect = r;
            bitmapReady.notifyCalled();
        };
        final int offset = index * initialSize.getHeight();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Rect captureArea =
                    new Rect(0, offset, initialSize.getWidth(), offset + initialSize.getHeight());
            mCallback.onScrollCaptureImageRequest(surface, signal, captureArea, consume);
        });
        bitmapReady.waitForFirst();
        if (mCapturedRect.isEmpty()) return false;

        // Only record when the rect is not empty otherwise the bitmap won't have changed.
        surfaceChanged.waitForCallback(callCount, 1);
        mRenderTestRule.compareForResult(mBitmap, tag + "_viewport_" + String.valueOf(index));
        return true;
    }
}
