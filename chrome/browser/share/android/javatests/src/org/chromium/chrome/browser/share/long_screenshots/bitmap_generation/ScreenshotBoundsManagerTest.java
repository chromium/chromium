// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;

/** Tests for the ScreenshotBoundsManager */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ScreenshotBoundsManagerTest {
    @Mock private Context mContext;

    @Mock private Tab mTab;

    @Mock private RenderCoordinatesImpl mRenderCoordinates;

    @Mock private WebContentsImpl mWebContents;

    @Mock private LongScreenshotsCompositor mCompositor;

    @Mock private LongScreenshotsTabService mTabService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mRenderCoordinates.getPageScaleFactorInt()).thenReturn(1);
        when(mRenderCoordinates.getContentWidthPixInt()).thenReturn(200);
        when(mRenderCoordinates.getContentHeightPixInt()).thenReturn(10000);
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(600);
    }

    private void compareRects(int expectedTop, int expectedBottom, Rect actual) {
        assertEquals(expectedTop, actual.top);
        assertEquals(expectedBottom, actual.bottom);
        assertEquals(0, actual.left);
        assertEquals(0, actual.right);
    }

    @Test
    public void testCaptureBounds() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());
    }

    @Test
    public void testCalculateClipBoundsBelowPastCapture() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());

        boundsManager.setCompositedSize(new Size(500, 1200));
        boundsManager.setCompositedScrollOffset(new Point(0, 0));

        Rect compositeBounds = boundsManager.calculateClipBoundsBelow(1200);
        assertNull(compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsBelow() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());

        boundsManager.setCompositedSize(new Size(500, 1200));
        boundsManager.setCompositedScrollOffset(new Point(0, 0));

        Rect compositeBounds = boundsManager.calculateClipBoundsBelow(1000);
        compareRects(1000, 1100, compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsWithCutOff() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());

        boundsManager.setCompositedSize(new Size(500, 1200));
        boundsManager.setCompositedScrollOffset(new Point(0, 0));

        Rect compositeBounds = boundsManager.calculateClipBoundsBelow(1150);
        compareRects(1150, 1200, compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsOutsideRange() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());

        boundsManager.setCompositedSize(new Size(500, 1200));
        boundsManager.setCompositedScrollOffset(new Point(0, 0));

        Rect compositeBounds = boundsManager.calculateClipBoundsBelow(1300);
        assertNull(compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsAboveHigherThanCapture() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());

        boundsManager.setCompositedSize(new Size(500, 1200));
        boundsManager.setCompositedScrollOffset(new Point(0, 0));

        Rect compositeBounds = boundsManager.calculateClipBoundsAbove(-100);
        assertNull(compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsAbove() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());

        boundsManager.setCompositedSize(new Size(500, 1200));
        boundsManager.setCompositedScrollOffset(new Point(0, 0));

        Rect compositeBounds = boundsManager.calculateClipBoundsAbove(250);
        compareRects(150, 250, compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsAboveCutoff() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());

        boundsManager.setCompositedSize(new Size(500, 1200));
        boundsManager.setCompositedScrollOffset(new Point(0, 0));

        Rect compositeBounds = boundsManager.calculateClipBoundsAbove(50);
        compareRects(0, 50, compositeBounds);
    }

    @Test
    public void testCalculateFullClipBoundsAtTop() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());

        boundsManager.setCompositedSize(new Size(500, 1200));
        boundsManager.setCompositedScrollOffset(new Point(0, 0));

        Rect bounds = boundsManager.getFullEntryBounds();
        compareRects(0, 700, bounds);
    }

    @Test
    public void testCalculateFullClipBoundsScrolled() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());

        boundsManager.setCompositedSize(new Size(500, 1200));
        boundsManager.setCompositedScrollOffset(new Point(0, 500));

        Rect bounds = boundsManager.getFullEntryBounds();
        compareRects(300, 1000, bounds);
    }

    @Test
    public void testCalculateFullClipBoundsScrolledToBottom() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(-1, 999, boundsManager.getCaptureBounds());

        boundsManager.setCompositedSize(new Size(500, 1200));
        boundsManager.setCompositedScrollOffset(new Point(0, 1100));

        Rect bounds = boundsManager.getFullEntryBounds();
        compareRects(500, 1200, bounds);
    }

    @Test
    public void testGetBitmapScaleFactor() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        boundsManager.setCompositedSize(new Size(0, 0));
        boundsManager.setCompositedScrollOffset(new Point(0, 1100));
        assertEquals(1f, boundsManager.getBitmapScaleFactor(), 0.0001);

        when(mRenderCoordinates.getLastFrameViewportWidthPixInt()).thenReturn(1000);
        boundsManager.setCompositedSize(new Size(500, 0));
        assertEquals(2f, boundsManager.getBitmapScaleFactor(), 0.0001);

        boundsManager.setCompositedSize(new Size(2000, 0));
        assertEquals(0.5f, boundsManager.getBitmapScaleFactor(), 0.0001);
    }
}
