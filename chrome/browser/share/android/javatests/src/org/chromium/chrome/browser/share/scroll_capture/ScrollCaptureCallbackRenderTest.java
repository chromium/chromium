// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import static org.chromium.base.test.transit.Condition.whether;
import static org.chromium.base.test.transit.Condition.whetherEquals;
import static org.chromium.base.test.transit.SimpleConditions.uiThreadCondition;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** A RenderTest for {@link ScrollCaptureCallbackImpl}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Unbatched to deflake")
@DisabledTest(message = "crbug.com/359632845")
public class ScrollCaptureCallbackRenderTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setDescription("new test html")
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_SHARING)
                    .build();

    private ScrollCaptureCallbackDelegate mCallback;
    private WebPageStation mInitialPage;
    private Tab mTab;
    private TextureView mTextureView;
    private Bitmap mBitmap;
    private Rect mCapturedRect;

    @Before
    public void setUp() {
        // TODO(crbug.com/40199744): create a page that checkboards in a gradient or
        // something more complex to generate better test images.
        GURL url =
                new GURL(
                        mActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/share/checkerboard.html"));
        mInitialPage = mActivityTestRule.startOnWebPage(url.getSpec());
        mCallback =
                new ScrollCaptureCallbackDelegate(
                        new ScrollCaptureCallbackDelegate.EntryManagerWrapper());
        mTab = mInitialPage.loadedTabElement.get();
        mCallback.setCurrentTab(mTab);
        // Wait for the script to execute and for the renderer to actually paint everything.
        Condition.waitFor(
                uiThreadCondition(
                        "Title is \"rendered\"",
                        () -> whetherEquals("rendered", mTab.getWebContents().getTitle())),
                uiThreadCondition(
                        "Drawing succeeded",
                        () -> {
                            RenderCoordinates renderCoordinates =
                                    RenderCoordinates.fromWebContents(mTab.getWebContents());
                            int contentHeightPix = renderCoordinates.getContentHeightPixInt();
                            return whether(
                                    contentHeightPix > 10000,
                                    "contentHeightPix %d",
                                    contentHeightPix);
                        }));
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
    public void testCaptureBottom() throws Exception {
        RenderCoordinates renderCoordinates =
                RenderCoordinates.fromWebContents(mTab.getWebContents());
        // Drive a scroll to the bottom of the page.
        final int offset =
                renderCoordinates.getContentHeightPixInt()
                        - renderCoordinates.getLastFrameViewportHeightPixInt();
        Condition.runAndWaitFor(
                Transition.runTriggerOnUiThreadOption(),
                () -> mTab.getWebContents().getEventForwarder().scrollBy(0, offset),
                uiThreadCondition(
                        String.format("Scroll of offset %d occurred", offset),
                        () -> {
                            RenderCoordinates scrolledCoordinates =
                                    RenderCoordinates.fromWebContents(mTab.getWebContents());
                            int scrollYPixInt = scrolledCoordinates.getScrollYPixInt();
                            return whether(
                                    offset - 5 <= scrollYPixInt && scrollYPixInt <= offset + 5,
                                    "getScrollYPixInt() %d within 5px of %d",
                                    scrollYPixInt,
                                    offset);
                        }));

        View view = mTab.getView();
        Size size = new Size(view.getWidth(), view.getHeight());

        CallbackHelper surfaceChanged = new CallbackHelper();
        createTextureView(size, surfaceChanged);

        driveScrollCapture(size, surfaceChanged, "scroll_capture_bottom");
    }

    /** Creates a texture view to draw bitmaps to. */
    private void createTextureView(Size size, CallbackHelper surfaceChanged)
            throws TimeoutException {
        CallbackHelper surfaceReady = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTextureView = new TextureView(ContextUtils.getApplicationContext());
                    mTextureView.setSurfaceTextureListener(
                            new TextureView.SurfaceTextureListener() {
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
                            (ViewGroup)
                                    mActivityTestRule
                                            .getActivity()
                                            .findViewById(android.R.id.content);
                    group.addView(
                            mTextureView, new LayoutParams(size.getWidth(), size.getHeight()));
                });
        surfaceReady.waitForOnly();
    }

    /** Drives a scroll capture several viewports above and below the current viewport location. */
    private void driveScrollCapture(Size initialSize, CallbackHelper surfaceChanged, String tag)
            throws Exception {
        CancellationSignal signal = new CancellationSignal();

        // Start the session.
        CallbackHelper ready = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Rect r = mCallback.onScrollCaptureSearch(signal);
                    mCallback.onScrollCaptureStart(signal, ready::notifyCalled);
                    Assert.assertFalse(r.isEmpty());
                });
        ready.waitForOnly();

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCallback.onScrollCaptureEnd(finished::notifyCalled);
                });
        finished.waitForOnly();
    }

    /** Captures the viewport at i * initialSize.getHeight() offset to the current viewport. */
    private boolean captureViewport(
            CancellationSignal signal,
            Surface surface,
            Size initialSize,
            CallbackHelper surfaceChanged,
            String tag,
            int index)
            throws Exception {
        int callCount = surfaceChanged.getCallCount();
        CallbackHelper bitmapReady = new CallbackHelper();
        Callback<Rect> consume =
                r -> {
                    mCapturedRect = r;
                    bitmapReady.notifyCalled();
                };
        // Clear any old content from the surface.
        Canvas canvas =
                surface.lockCanvas(new Rect(0, 0, initialSize.getWidth(), initialSize.getHeight()));
        canvas.drawColor(0, PorterDuff.Mode.CLEAR);
        surface.unlockCanvasAndPost(canvas);
        surfaceChanged.waitForCallback(callCount, 1);

        // Capture the content in the right location.
        callCount = surfaceChanged.getCallCount();
        final int offset = index * initialSize.getHeight();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Rect captureArea =
                            new Rect(
                                    0,
                                    offset,
                                    initialSize.getWidth(),
                                    offset + initialSize.getHeight());
                    mCallback.onScrollCaptureImageRequest(surface, signal, captureArea, consume);
                });
        bitmapReady.waitForOnly();
        if (mCapturedRect.isEmpty()) return false;

        // Only record when the rect is not empty otherwise the bitmap won't have changed.
        surfaceChanged.waitForCallback(callCount, 1);
        mRenderTestRule.compareForResult(mBitmap, tag + "_viewport_" + String.valueOf(index));
        return true;
    }
}
