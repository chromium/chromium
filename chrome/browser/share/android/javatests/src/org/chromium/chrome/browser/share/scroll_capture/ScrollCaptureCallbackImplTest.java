// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.os.CancellationSignal;
import android.view.ScrollCaptureSession;
import android.view.Surface;

import androidx.annotation.RequiresApi;
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

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager.BitmapGeneratorObserver;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryListener;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.share.scroll_capture.ScrollCaptureCallbackImpl.EntryManagerWrapper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;

import java.util.function.Consumer;

/** Tests for the ScreenshotBoundsManager */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@RequiresApi(api = VERSION_CODES.S)
@MinAndroidSdkLevel(VERSION_CODES.S)
public class ScrollCaptureCallbackImplTest {
    @Mock
    private Tab mTab;
    @Mock
    private WebContentsImpl mWebContents;
    @Mock
    private RenderCoordinatesImpl mRenderCoordinates;
    @Mock
    private EntryManagerWrapper mEntryManagerWrapper;
    @Mock
    private EntryManager mEntryManager;
    @Mock
    private Consumer<Rect> mRectConsumer;

    // We should use the Object type here to avoid RuntimeError in classloader on the bots running
    // API versions before S.
    private Object mScrollCaptureCallbackObj;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        mScrollCaptureCallbackObj = new ScrollCaptureCallbackImpl(mEntryManagerWrapper);
        ((ScrollCaptureCallbackImpl) mScrollCaptureCallbackObj).setCurrentTab(mTab);
    }

    @Test
    @SmallTest
    public void testScrollCaptureSearch() {
        ScrollCaptureCallbackImpl scrollCaptureCallback =
                (ScrollCaptureCallbackImpl) mScrollCaptureCallbackObj;
        CancellationSignal signal = new CancellationSignal();
        InOrder inOrder = Mockito.inOrder(mRectConsumer);

        // WebContents is not set. Should return empty Rect.
        scrollCaptureCallback.onScrollCaptureSearch(signal, mRectConsumer);
        inOrder.verify(mRectConsumer).accept(eq(new Rect()));

        int viewportWidth = 500;
        int viewportHeight = 2000;
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mRenderCoordinates.getLastFrameViewportWidthPixInt()).thenReturn(viewportWidth);
        when(mRenderCoordinates.getLastFrameViewportHeightPixInt()).thenReturn(viewportHeight);
        scrollCaptureCallback.onScrollCaptureSearch(signal, mRectConsumer);
        inOrder.verify(mRectConsumer).accept(eq(new Rect(0, 0, viewportWidth, viewportHeight)));
    }

    @Test
    @SmallTest
    public void testScrollCaptureStart() {
        ScrollCaptureCallbackImpl scrollCaptureCallback =
                (ScrollCaptureCallbackImpl) mScrollCaptureCallbackObj;
        ScrollCaptureSession session = mock(ScrollCaptureSession.class);
        CancellationSignal signal = new CancellationSignal();
        Runnable onReady = mock(Runnable.class);
        InOrder inOrder = Mockito.inOrder(onReady, mEntryManagerWrapper, mEntryManager);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mEntryManagerWrapper.create(any())).thenReturn(mEntryManager);

        int viewportWidth = 200;
        int viewportHeight = 500;
        int contentWidth = 200;
        int contentHeight = 2000;
        int scrollY = 0;
        setupRenderCoordinates(viewportWidth, viewportHeight, contentWidth, contentHeight, scrollY);
        scrollCaptureCallback.onScrollCaptureSearch(signal, mRectConsumer);

        scrollCaptureCallback.onScrollCaptureStart(session, signal, onReady);
        Assert.assertEquals(new Rect(0, 0, contentWidth, contentHeight),
                scrollCaptureCallback.getContentAreaForTesting());
        Assert.assertEquals(new Rect(0, 0, viewportWidth, viewportHeight),
                scrollCaptureCallback.getInitialRectForTesting());
        Assert.assertFalse(signal.isCanceled());

        // Test non-zero Y offset
        scrollY = 300;
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(scrollY);
        scrollCaptureCallback.onScrollCaptureStart(session, signal, onReady);
        Assert.assertEquals(new Rect(0, scrollY, viewportWidth, scrollY + viewportHeight),
                scrollCaptureCallback.getInitialRectForTesting());
        Assert.assertFalse(signal.isCanceled());

        // Test zoomed on content
        int pageScaleFactor = 2;
        when(mRenderCoordinates.getPageScaleFactor()).thenReturn((float) pageScaleFactor);
        scrollCaptureCallback.onScrollCaptureStart(session, signal, onReady);
        Assert.assertEquals(
                new Rect(0, 0, contentWidth / pageScaleFactor, contentHeight / pageScaleFactor),
                scrollCaptureCallback.getContentAreaForTesting());
        Assert.assertEquals(new Rect(0, scrollY / pageScaleFactor, viewportWidth,
                                    scrollY / pageScaleFactor + viewportHeight),
                scrollCaptureCallback.getInitialRectForTesting());
        Assert.assertFalse(signal.isCanceled());

        // Test EntryManager initialization
        scrollCaptureCallback.onScrollCaptureStart(session, signal, onReady);
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
        inOrder.verify(onReady).run();
    }

    @Test
    @SmallTest
    public void testScrollCaptureRequest() {
        ScrollCaptureCallbackImpl scrollCaptureCallback =
                (ScrollCaptureCallbackImpl) mScrollCaptureCallbackObj;
        ScrollCaptureSession session = mock(ScrollCaptureSession.class);
        Surface surface = mock(Surface.class);
        Canvas canvas = mock(Canvas.class);
        CancellationSignal signal = new CancellationSignal();
        LongScreenshotsEntry entry = mock(LongScreenshotsEntry.class);
        Bitmap testBitmap = Bitmap.createBitmap(512, 1024, Bitmap.Config.ARGB_8888);
        Runnable onReady = mock(Runnable.class);
        InOrder inOrder = Mockito.inOrder(surface, canvas, mRectConsumer, mEntryManager, onReady);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mEntryManagerWrapper.create(any())).thenReturn(mEntryManager);
        when(session.getSurface()).thenReturn(surface);
        when(surface.lockCanvas(any())).thenReturn(canvas);

        int viewportWidth = 500;
        int viewportHeight = 1000;
        int contentWidth = 500;
        int contentHeight = 5000;
        int scrollY = 1000;
        setupRenderCoordinates(viewportWidth, viewportHeight, contentWidth, contentHeight, scrollY);
        scrollCaptureCallback.onScrollCaptureSearch(signal, mRectConsumer);
        scrollCaptureCallback.onScrollCaptureStart(session, signal, () -> {});

        // Test capture area outside the content area.
        Rect captureArea = new Rect(0, -2000, 500, -1000);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                session, signal, captureArea, mRectConsumer);
        inOrder.verify(mRectConsumer).accept(eq(new Rect()));

        // Test resulting capture area with width smaller than threshold.
        captureArea.set(0, -1010, 500, -1000);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                session, signal, captureArea, mRectConsumer);
        inOrder.verify(mRectConsumer).accept(eq(new Rect()));

        when(mEntryManager.generateEntry(any())).thenReturn(entry);
        doAnswer(invocation -> {
            EntryListener listener = invocation.getArgument(0);
            listener.onResult(EntryStatus.BITMAP_GENERATED);
            return null;
        })
                .when(entry)
                .setListener(any(EntryListener.class));
        // Test empty bitmap.
        captureArea.set(0, -1500, 500, -500);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                session, signal, captureArea, mRectConsumer);
        inOrder.verify(mRectConsumer).accept(eq(new Rect()));

        // Test successful capture
        when(entry.getBitmap()).thenReturn(testBitmap);
        captureArea.set(0, -1500, 500, -500);
        scrollCaptureCallback.onScrollCaptureImageRequest(
                session, signal, captureArea, mRectConsumer);
        inOrder.verify(surface).lockCanvas(any());
        inOrder.verify(canvas).drawBitmap(eq(testBitmap), eq(null), any(Rect.class), eq(null));
        inOrder.verify(surface).unlockCanvasAndPost(canvas);
        ArgumentCaptor<Rect> rectArgumentCaptor = ArgumentCaptor.forClass(Rect.class);
        inOrder.verify(mRectConsumer).accept(rectArgumentCaptor.capture());
        // The resulting capture Rect should be cropped to 500 height because the upper half of it
        // was out of the content area.
        Assert.assertEquals(new Rect(0, -1000, 500, -500), rectArgumentCaptor.getValue());

        // Test end capture
        scrollCaptureCallback.onScrollCaptureEnd(onReady);
        Assert.assertNull(scrollCaptureCallback.getContentAreaForTesting());
        Assert.assertNull(scrollCaptureCallback.getInitialRectForTesting());
        inOrder.verify(onReady).run();
    }

    private void setupRenderCoordinates(int viewportWidth, int viewportHeight, int contentWidth,
            int contentHeight, int scrollY) {
        when(mRenderCoordinates.getPageScaleFactor()).thenReturn(1f);
        when(mRenderCoordinates.getLastFrameViewportWidthPixInt()).thenReturn(viewportWidth);
        when(mRenderCoordinates.getLastFrameViewportHeightPixInt()).thenReturn(viewportHeight);
        when(mRenderCoordinates.getContentWidthPixInt()).thenReturn(contentWidth);
        when(mRenderCoordinates.getContentHeightPixInt()).thenReturn(contentHeight);
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(scrollY);
    }
}
