// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.CancellationSignal;
import android.util.Size;
import android.view.Surface;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.paint_preview.PaintPreviewCompositorUtils;
import org.chromium.chrome.browser.paint_preview.PaintPreviewCompositorUtilsJni;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager.BitmapGeneratorObserver;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryListener;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.share.scroll_capture.ScrollCaptureCallbackDelegate.EntryManagerWrapper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;

/** Tests for the {@link ScrollCaptureCallbackDelegate} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ScrollCaptureCallbackDelegateTest {
    @Mock private Tab mTab;
    @Mock private WebContentsImpl mWebContents;
    @Mock private RenderCoordinatesImpl mRenderCoordinates;
    @Mock private EntryManagerWrapper mEntryManagerWrapper;
    @Mock private EntryManager mEntryManager;
    @Mock private LongScreenshotsEntry mEntry;
    @Mock private Callback<Rect> mRectConsumer;
    @Mock private PaintPreviewCompositorUtils.Natives mCompositorUtils;

    // We should use the Object type here to avoid RuntimeError in classloader on the bots running
    // API versions before S.
    private Object mScrollCaptureCallbackObj;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Before
    public void setUp() {
        mJniMocker.mock(PaintPreviewCompositorUtilsJni.TEST_HOOKS, mCompositorUtils);
        doReturn(false).when(mCompositorUtils).stopWarmCompositor();
        doNothing().when(mCompositorUtils).warmupCompositor();
        mScrollCaptureCallbackObj = new ScrollCaptureCallbackDelegate(mEntryManagerWrapper);
        ((ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj).setCurrentTab(mTab);
    }

    @Test
    @SmallTest
    public void testScrollCaptureSearch() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        CancellationSignal signal = new CancellationSignal();
        InOrder inOrder = Mockito.inOrder(mRectConsumer);

        // WebContents is not set. Should return empty Rect.
        Assert.assertTrue(scrollCaptureCallback.onScrollCaptureSearch(signal).isEmpty());

        final int viewportWidth = 500;
        final int viewportHeight = 2000;
        View view = mock(View.class);
        when(mTab.getView()).thenReturn(view);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mRenderCoordinates.getLastFrameViewportWidthPixInt()).thenReturn(viewportWidth);
        when(mRenderCoordinates.getLastFrameViewportHeightPixInt()).thenReturn(viewportHeight);
        when(mRenderCoordinates.getMinPageScaleFactor()).thenReturn(1f);
        Assert.assertEquals(
                new Rect(0, 0, viewportWidth, viewportHeight),
                scrollCaptureCallback.onScrollCaptureSearch(signal));
    }

    @Test
    @SmallTest
    public void testScrollCaptureStart() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        CancellationSignal signal = new CancellationSignal();
        Runnable onReady = mock(Runnable.class);
        InOrder inOrder = Mockito.inOrder(onReady, mEntryManagerWrapper, mEntryManager);

        View view = mock(View.class);
        when(mTab.getView()).thenReturn(view);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mEntryManagerWrapper.create(any())).thenReturn(mEntryManager);

        int viewportWidth = 200;
        int viewportHeight = 500;
        when(mRenderCoordinates.getLastFrameViewportWidthPixInt()).thenReturn(viewportWidth);
        when(mRenderCoordinates.getLastFrameViewportHeightPixInt()).thenReturn(viewportHeight);
        when(mRenderCoordinates.getMinPageScaleFactor()).thenReturn(1f);
        Assert.assertEquals(
                new Rect(0, 0, viewportWidth, viewportHeight),
                scrollCaptureCallback.onScrollCaptureSearch(signal));

        // Test EntryManager initialization
        scrollCaptureCallback.onScrollCaptureStart(signal, onReady);
        inOrder.verify(mEntryManagerWrapper).create(any());
        ArgumentCaptor<BitmapGeneratorObserver> observerArgumentCaptor =
                ArgumentCaptor.forClass(BitmapGeneratorObserver.class);
        inOrder.verify(mEntryManager).addBitmapGeneratorObserver(observerArgumentCaptor.capture());
        BitmapGeneratorObserver observer = observerArgumentCaptor.getValue();
        observer.onStatusChange(EntryStatus.CAPTURE_IN_PROGRESS);
        inOrder.verify(onReady, times(0)).run();
        observer.onStatusChange(EntryStatus.GENERATION_ERROR);
        Assert.assertTrue(signal.isCanceled());
        inOrder.verify(onReady, times(0)).run();
        observer.onStatusChange(EntryStatus.CAPTURE_COMPLETE);
        inOrder.verify(onReady, times(0)).run();

        observer.onCompositorReady(new Size(0, 0), new Point(0, 0));
        inOrder.verify(onReady, times(0)).run();
        observer.onCompositorReady(new Size(100, 100), new Point(0, 0));
        inOrder.verify(onReady).run();

        // Test contentArea and initialRect assignment
        int contentWidth = 200;
        int contentHeight = 2000;
        observer.onCompositorReady(new Size(contentWidth, contentHeight), new Point(0, 0));
        Assert.assertEquals(
                new Rect(0, 0, contentWidth, contentHeight),
                scrollCaptureCallback.getContentAreaForTesting());
        Assert.assertEquals(0, scrollCaptureCallback.getInitialYOffsetForTesting());

        // Test non-zero Y offset
        int scrollY = 300;
        observer.onCompositorReady(new Size(contentWidth, contentHeight), new Point(0, scrollY));
        scrollCaptureCallback.onScrollCaptureStart(signal, onReady);
        Assert.assertEquals(scrollY, scrollCaptureCallback.getInitialYOffsetForTesting());
    }

    @Test
    @SmallTest
    public void testScrollCaptureRequest() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        Surface surface = mock(Surface.class);
        Canvas canvas = mock(Canvas.class);
        CancellationSignal signal = new CancellationSignal();
        Bitmap testBitmap = Bitmap.createBitmap(512, 1024, Bitmap.Config.ARGB_8888);
        Runnable onReady = mock(Runnable.class);
        InOrder inOrder =
                Mockito.inOrder(surface, canvas, mRectConsumer, mEntryManager, onReady, mEntry);

        View view = mock(View.class);
        when(mTab.getView()).thenReturn(view);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mEntryManagerWrapper.create(any())).thenReturn(mEntryManager);
        when(surface.lockCanvas(any())).thenReturn(canvas);

        int viewportWidth = 500;
        int viewportHeight = 1000;
        int contentWidth = 500;
        int contentHeight = 5000;
        int scrollY = 1000;
        when(mRenderCoordinates.getLastFrameViewportWidthPixInt()).thenReturn(viewportWidth);
        when(mRenderCoordinates.getLastFrameViewportHeightPixInt()).thenReturn(viewportHeight);
        when(mRenderCoordinates.getMinPageScaleFactor()).thenReturn(1f);
        // Set up viewportRect
        Assert.assertEquals(
                new Rect(0, 0, viewportWidth, viewportHeight),
                scrollCaptureCallback.onScrollCaptureSearch(signal));
        scrollCaptureCallback.onScrollCaptureStart(signal, () -> {});
        // Set up contentArea and initialRect
        ArgumentCaptor<BitmapGeneratorObserver> observerArgumentCaptor =
                ArgumentCaptor.forClass(BitmapGeneratorObserver.class);
        inOrder.verify(mEntryManager).addBitmapGeneratorObserver(observerArgumentCaptor.capture());
        BitmapGeneratorObserver observer = observerArgumentCaptor.getValue();
        observer.onCompositorReady(new Size(contentWidth, contentHeight), new Point(0, scrollY));

        // Test capture area outside the content area.
        Rect captureArea = new Rect(0, -2000, 500, -1000);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                surface, signal, captureArea, mRectConsumer);
        inOrder.verify(mRectConsumer).onResult(eq(new Rect()));

        // Test resulting capture area with width smaller than threshold.
        captureArea.set(0, -1010, 500, -1000);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                surface, signal, captureArea, mRectConsumer);
        inOrder.verify(mRectConsumer).onResult(eq(new Rect()));

        doAnswer(
                        invocation -> {
                            EntryListener listener = invocation.getArgument(0);
                            listener.onResult(EntryStatus.BITMAP_GENERATED);
                            return null;
                        })
                .when(mEntry)
                .setListener(any(EntryListener.class));
        when(mEntryManager.generateEntry(any())).thenReturn(mEntry);
        // Test empty bitmap.
        captureArea.set(0, -1500, 500, -500);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                surface, signal, captureArea, mRectConsumer);
        inOrder.verify(mEntryManager).generateEntry(any());
        inOrder.verify(mEntry).setListener(any());
        inOrder.verify(mEntry).getBitmap();
        inOrder.verify(mRectConsumer).onResult(eq(new Rect()));

        // Test successful capture
        when(mEntry.getBitmap()).thenReturn(testBitmap);
        captureArea.set(0, -1500, 500, -500);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                surface, signal, captureArea, mRectConsumer);
        inOrder.verify(mEntryManager).generateEntry(any());
        inOrder.verify(mEntry).setListener(any());
        inOrder.verify(mEntry).getBitmap();
        inOrder.verify(surface).lockCanvas(any());
        inOrder.verify(canvas).drawBitmap(eq(testBitmap), eq(null), any(Rect.class), eq(null));
        inOrder.verify(surface).unlockCanvasAndPost(canvas);
        ArgumentCaptor<Rect> rectArgumentCaptor = ArgumentCaptor.forClass(Rect.class);
        inOrder.verify(mRectConsumer).onResult(rectArgumentCaptor.capture());
        // The resulting capture Rect should be cropped to 500 height because the upper half of it
        // was out of the content area.
        Assert.assertEquals(new Rect(0, -1000, 500, -500), rectArgumentCaptor.getValue());

        // Test end capture
        scrollCaptureCallback.onScrollCaptureEnd(onReady);
        inOrder.verify(mEntryManager).destroy();
        Assert.assertNull(scrollCaptureCallback.getContentAreaForTesting());
        inOrder.verify(onReady).run();
    }
}
