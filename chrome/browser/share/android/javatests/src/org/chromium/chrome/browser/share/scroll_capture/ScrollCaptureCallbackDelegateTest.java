// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
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
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.MemoryPressureLevel;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
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
    @Mock private View mView;
    @Mock private Runnable mOnReady;
    @Mock private Surface mSurface;
    @Mock private Canvas mCanvas;

    private Activity mActivity;

    @Captor private ArgumentCaptor<BitmapGeneratorObserver> mObserverCaptor;
    @Captor private ArgumentCaptor<Rect> mRectCaptor;

    // We should use the Object type here to avoid RuntimeError in classloader on the bots running
    // API versions before S.
    private Object mScrollCaptureCallbackObj;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);

    @Before
    public void setUp() {
        MemoryPressureMonitor.INSTANCE.setLastReportedPressureForTesting(MemoryPressureLevel.NONE);
        PaintPreviewCompositorUtilsJni.setInstanceForTesting(mCompositorUtils);
        doReturn(false).when(mCompositorUtils).stopWarmCompositor();
        doNothing().when(mCompositorUtils).warmupCompositor();
        mActivity = Robolectric.setupActivity(Activity.class);
        when(mTab.getContext()).thenReturn(mActivity);

        when(mTab.getView()).thenReturn(mView);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mEntryManagerWrapper.create(any())).thenReturn(mEntryManager);
        when(mRenderCoordinates.getMinPageScaleFactor()).thenReturn(1f);
        when(mRenderCoordinates.getLastFrameViewportWidthPixInt()).thenReturn(200);
        when(mRenderCoordinates.getLastFrameViewportHeightPixInt()).thenReturn(500);

        mScrollCaptureCallbackObj = new ScrollCaptureCallbackDelegate(mEntryManagerWrapper);
        ((ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj).setCurrentTab(mTab);
    }

    @Test
    @SmallTest
    public void testScrollCaptureSearch() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        CancellationSignal signal = new CancellationSignal();

        // View is null. Should return empty Rect.
        when(mTab.getView()).thenReturn(null);
        Assert.assertTrue(scrollCaptureCallback.onScrollCaptureSearch(signal).isEmpty());

        // WebContents is null. Should return empty Rect.
        when(mTab.getView()).thenReturn(mView);
        when(mTab.getWebContents()).thenReturn(null);
        Assert.assertTrue(scrollCaptureCallback.onScrollCaptureSearch(signal).isEmpty());

        final int viewportWidth = 500;
        final int viewportHeight = 2000;
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mRenderCoordinates.getLastFrameViewportWidthPixInt()).thenReturn(viewportWidth);
        when(mRenderCoordinates.getLastFrameViewportHeightPixInt()).thenReturn(viewportHeight);
        Assert.assertEquals(
                new Rect(0, 0, viewportWidth, viewportHeight),
                scrollCaptureCallback.onScrollCaptureSearch(signal));
    }

    @Test
    @SmallTest
    public void testScrollCaptureStart_CaptureInProgress() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        scrollCaptureCallback.onScrollCaptureSearch(new CancellationSignal());

        scrollCaptureCallback.onScrollCaptureStart(new CancellationSignal(), mOnReady);
        verify(mEntryManagerWrapper).create(any());
        verify(mEntryManager).addBitmapGeneratorObserver(mObserverCaptor.capture());
        BitmapGeneratorObserver observer = mObserverCaptor.getValue();

        observer.onStatusChange(EntryStatus.CAPTURE_IN_PROGRESS);
        verify(mOnReady, times(0)).run();
    }

    @Test
    @SmallTest
    public void testScrollCaptureStart_GenerationErrorOrder() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        scrollCaptureCallback.onScrollCaptureSearch(new CancellationSignal());

        InOrder inOrder = Mockito.inOrder(mOnReady, mEntryManager);
        scrollCaptureCallback.onScrollCaptureStart(new CancellationSignal(), mOnReady);
        verify(mEntryManager).addBitmapGeneratorObserver(mObserverCaptor.capture());
        BitmapGeneratorObserver observer = mObserverCaptor.getValue();

        observer.onStatusChange(EntryStatus.GENERATION_ERROR);
        inOrder.verify(mEntryManager).destroy();
        inOrder.verify(mOnReady).run();
    }

    @Test
    @SmallTest
    public void testScrollCaptureStart_CompositorReady_EmptySize() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        scrollCaptureCallback.onScrollCaptureSearch(new CancellationSignal());

        InOrder inOrder = Mockito.inOrder(mOnReady, mEntryManager);
        scrollCaptureCallback.onScrollCaptureStart(new CancellationSignal(), mOnReady);
        verify(mEntryManager).addBitmapGeneratorObserver(mObserverCaptor.capture());
        BitmapGeneratorObserver observer = mObserverCaptor.getValue();

        observer.onCompositorReady(new Size(0, 0), new Point(0, 0));
        inOrder.verify(mEntryManager).destroy();
        inOrder.verify(mOnReady).run();
    }

    @Test
    @SmallTest
    public void testScrollCaptureStart_Success() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        scrollCaptureCallback.onScrollCaptureSearch(new CancellationSignal());

        scrollCaptureCallback.onScrollCaptureStart(new CancellationSignal(), mOnReady);
        verify(mEntryManager).addBitmapGeneratorObserver(mObserverCaptor.capture());
        BitmapGeneratorObserver observer = mObserverCaptor.getValue();

        int contentWidth = 200;
        int contentHeight = 2000;
        int scrollY = 0;
        observer.onCompositorReady(new Size(contentWidth, contentHeight), new Point(0, scrollY));
        verify(mOnReady).run();
        Assert.assertEquals(
                new Rect(0, 0, contentWidth, contentHeight),
                scrollCaptureCallback.getContentAreaForTesting());
        Assert.assertEquals(scrollY, scrollCaptureCallback.getInitialYOffsetForTesting());
    }

    @Test
    @SmallTest
    public void testScrollCaptureStart_NonZeroYOffset() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        scrollCaptureCallback.onScrollCaptureSearch(new CancellationSignal());

        scrollCaptureCallback.onScrollCaptureStart(new CancellationSignal(), mOnReady);
        verify(mEntryManager).addBitmapGeneratorObserver(mObserverCaptor.capture());
        BitmapGeneratorObserver observer = mObserverCaptor.getValue();

        int contentWidth = 200;
        int contentHeight = 2000;
        int scrollY = 300;
        observer.onCompositorReady(new Size(contentWidth, contentHeight), new Point(0, scrollY));
        Assert.assertEquals(scrollY, scrollCaptureCallback.getInitialYOffsetForTesting());
    }

    @Test
    @SmallTest
    public void testScrollCaptureStart_GenerationError() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        CancellationSignal signal = new CancellationSignal();

        scrollCaptureCallback.onScrollCaptureSearch(signal);
        scrollCaptureCallback.onScrollCaptureStart(signal, mOnReady);

        verify(mEntryManager).addBitmapGeneratorObserver(mObserverCaptor.capture());
        BitmapGeneratorObserver observer = mObserverCaptor.getValue();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sharing.ScrollCapture.BitmapGeneratorStatus", 2);
        observer.onStatusChange(EntryStatus.GENERATION_ERROR);

        Assert.assertFalse(signal.isCanceled());
        verify(mOnReady).run();
        verify(mCompositorUtils).stopWarmCompositor();
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testScrollCaptureStart_InsufficientMemory() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        CancellationSignal signal = new CancellationSignal();

        scrollCaptureCallback.onScrollCaptureSearch(signal);
        scrollCaptureCallback.onScrollCaptureStart(signal, mOnReady);

        verify(mEntryManager).addBitmapGeneratorObserver(mObserverCaptor.capture());
        BitmapGeneratorObserver observer = mObserverCaptor.getValue();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sharing.ScrollCapture.BitmapGeneratorStatus", 1);
        observer.onStatusChange(EntryStatus.INSUFFICIENT_MEMORY);

        Assert.assertFalse(signal.isCanceled());
        verify(mOnReady).run();
        verify(mCompositorUtils).stopWarmCompositor();
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testScrollCaptureSearch_MemoryPressure() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        CancellationSignal signal = new CancellationSignal();

        MemoryPressureMonitor.INSTANCE.setLastReportedPressureForTesting(
                MemoryPressureLevel.MODERATE);
        Assert.assertTrue(scrollCaptureCallback.onScrollCaptureSearch(signal).isEmpty());

        MemoryPressureMonitor.INSTANCE.setLastReportedPressureForTesting(MemoryPressureLevel.NONE);
        when(mRenderCoordinates.getLastFrameViewportWidthPixInt()).thenReturn(500);
        when(mRenderCoordinates.getLastFrameViewportHeightPixInt()).thenReturn(2000);

        Assert.assertEquals(
                new Rect(0, 0, 500, 2000), scrollCaptureCallback.onScrollCaptureSearch(signal));
    }

    @Test
    @SmallTest
    public void testScrollCaptureRequest_NullContentArea() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        CancellationSignal signal = new CancellationSignal();

        // mContentArea is null because onScrollCaptureStart hasn't finished or failed.
        Rect captureArea = new Rect(0, 0, 500, 1000);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                mSurface, signal, captureArea, mRectConsumer);
        verify(mRectConsumer).onResult(eq(new Rect()));
    }

    @Test
    @SmallTest
    public void testScrollCaptureRequest() {
        ScrollCaptureCallbackDelegate scrollCaptureCallback =
                (ScrollCaptureCallbackDelegate) mScrollCaptureCallbackObj;
        CancellationSignal signal = new CancellationSignal();
        Bitmap testBitmap = Bitmap.createBitmap(512, 1024, Bitmap.Config.ARGB_8888);
        InOrder inOrder =
                Mockito.inOrder(mSurface, mCanvas, mRectConsumer, mEntryManager, mOnReady, mEntry);

        when(mSurface.lockCanvas(any())).thenReturn(mCanvas);

        int viewportWidth = 500;
        int viewportHeight = 1000;
        int contentWidth = 500;
        int contentHeight = 5000;
        int scrollY = 1000;
        when(mRenderCoordinates.getLastFrameViewportWidthPixInt()).thenReturn(viewportWidth);
        when(mRenderCoordinates.getLastFrameViewportHeightPixInt()).thenReturn(viewportHeight);

        // Set up viewportRect
        Assert.assertEquals(
                new Rect(0, 0, viewportWidth, viewportHeight),
                scrollCaptureCallback.onScrollCaptureSearch(signal));
        scrollCaptureCallback.onScrollCaptureStart(signal, mOnReady);
        // Set up contentArea and initialRect
        inOrder.verify(mEntryManager).addBitmapGeneratorObserver(mObserverCaptor.capture());
        BitmapGeneratorObserver observer = mObserverCaptor.getValue();
        observer.onCompositorReady(new Size(contentWidth, contentHeight), new Point(0, scrollY));

        // Test capture area outside the content area.
        Rect captureArea = new Rect(0, -2000, 500, -1000);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                mSurface, signal, captureArea, mRectConsumer);
        inOrder.verify(mRectConsumer).onResult(eq(new Rect()));

        // Test resulting capture area with width smaller than threshold.
        captureArea.set(0, -1010, 500, -1000);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                mSurface, signal, captureArea, mRectConsumer);
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
                mSurface, signal, captureArea, mRectConsumer);
        inOrder.verify(mEntryManager).generateEntry(any());
        inOrder.verify(mEntry).setListener(any());
        inOrder.verify(mEntry).getBitmap();
        inOrder.verify(mRectConsumer).onResult(eq(new Rect()));

        // Test successful capture
        when(mEntry.getBitmap()).thenReturn(testBitmap);
        captureArea.set(0, -1500, 500, -500);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                mSurface, signal, captureArea, mRectConsumer);
        inOrder.verify(mEntryManager).generateEntry(any());
        inOrder.verify(mEntry).setListener(any());
        inOrder.verify(mEntry).getBitmap();
        inOrder.verify(mSurface).lockCanvas(any());
        inOrder.verify(mCanvas).drawBitmap(eq(testBitmap), eq(null), any(Rect.class), eq(null));
        inOrder.verify(mSurface).unlockCanvasAndPost(mCanvas);
        inOrder.verify(mRectConsumer).onResult(mRectCaptor.capture());
        // The resulting capture Rect should be cropped to 500 height because the upper half of it
        // was out of the content area.
        Assert.assertEquals(new Rect(0, -1000, 500, -500), mRectCaptor.getValue());

        // Test end capture
        scrollCaptureCallback.onScrollCaptureEnd(mOnReady);
        inOrder.verify(mEntryManager).destroy();
        Assert.assertNull(scrollCaptureCallback.getContentAreaForTesting());
        inOrder.verify(mOnReady).run();
    }
}
