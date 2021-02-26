// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;

/** Tests for the ScreenshotBoundsManager */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT)
public class ScreenshotBoundsManagerTest {
    @Mock
    private Context mContext;

    @Mock
    private Tab mTab;

    @Mock
    private RenderCoordinatesImpl mRenderCoordinates;

    @Mock
    private WebContentsImpl mWebContents;

    @Mock
    private LongScreenshotsCompositor mCompositor;

    @Mock
    private LongScreenshotsTabService mTabService;

    private Bitmap mTestBitmap = Bitmap.createBitmap(512, 1024, Bitmap.Config.ARGB_8888);

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
        compareRects(200, 1200, boundsManager.getCaptureBounds());
    }

    @Test
    public void testCaptureBoundsHighStartPoint() {
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(100);

        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(0, 700, boundsManager.getCaptureBounds());
    }

    @Test
    public void testCaptureBoundsLowStartPoint() {
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(9900);

        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(9500, 10000, boundsManager.getCaptureBounds());
    }

    @Test
    public void testCalculateClipBoundsBelowPastCapture() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(200, 1200, boundsManager.getCaptureBounds());

        Rect compositeBounds = boundsManager.calculateClipBoundsBelow(1200);
        assertNull(compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsBelow() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(200, 1200, boundsManager.getCaptureBounds());

        Rect compositeBounds = boundsManager.calculateClipBoundsBelow(1000);
        compareRects(1000, 1100, compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsBelowCutOff() {
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(600);

        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);

        Rect captureBounds = boundsManager.getCaptureBounds();
        compareRects(200, 1200, boundsManager.getCaptureBounds());

        Rect compositeBounds = boundsManager.calculateClipBoundsBelow(1150);
        compareRects(1150, 1200, compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsAboveHigherThanCapture() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(200, 1200, boundsManager.getCaptureBounds());

        Rect compositeBounds = boundsManager.calculateClipBoundsAbove(100);
        assertNull(compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsAbove() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);
        compareRects(200, 1200, boundsManager.getCaptureBounds());

        Rect compositeBounds = boundsManager.calculateClipBoundsAbove(1000);
        compareRects(900, 1000, compositeBounds);
    }

    @Test
    public void testCalculateClipBoundsAboveCutoff() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);

        compareRects(200, 1200, boundsManager.getCaptureBounds());

        Rect compositeBounds = boundsManager.calculateClipBoundsAbove(250);
        compareRects(200, 250, compositeBounds);
    }

    @Test
    public void testCalculateBoundsRelativeToCapture() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);

        compareRects(200, 1200, boundsManager.getCaptureBounds());

        Rect compositeBounds = boundsManager.calculateClipBoundsAbove(250);
        compareRects(200, 250, compositeBounds);

        Rect relativeRect = boundsManager.calculateBoundsRelativeToCapture(compositeBounds);
        compareRects(0, 50, relativeRect);
    }

    @Test
    public void testCalculateBoundsRelativeToCaptureTooHigh() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);

        compareRects(200, 1200, boundsManager.getCaptureBounds());

        Rect relativeRect =
                boundsManager.calculateBoundsRelativeToCapture(new Rect(0, 150, 0, 350));
        compareRects(0, 150, relativeRect);
    }

    @Test
    public void testCalculateBoundsRelativeToCaptureTooLong() {
        ScreenshotBoundsManager boundsManager =
                ScreenshotBoundsManager.createForTests(mContext, mTab, 100);

        compareRects(200, 1200, boundsManager.getCaptureBounds());

        Rect relativeRect =
                boundsManager.calculateBoundsRelativeToCapture(new Rect(0, 250, 0, 3500));
        compareRects(50, 1000, relativeRect);
    }
}
