// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import android.graphics.Rect;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.gfx.RectUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.List;

/** Tests for FindAddress implementation. */
@RunWith(BaseRobolectricTestRunner.class)
public class RectUtilsTest {
    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testPixelCoverage() {
        Rect screenRect = new Rect(0, 0, 100, 100);
        int coveragePixels;

        List<Rect> singleCoverageRect = new ArrayList<Rect>();
        singleCoverageRect.add(new Rect(10, 10, 30, 30));

        coveragePixels = RectUtils.calculatePixelsOfCoverage(screenRect, singleCoverageRect);
        Assert.assertEquals(400, coveragePixels);

        List<Rect> multipleCoverageRect = new ArrayList<Rect>();
        multipleCoverageRect.add(new Rect(10, 10, 30, 30));
        multipleCoverageRect.add(new Rect(60, 10, 80, 30));

        coveragePixels = RectUtils.calculatePixelsOfCoverage(screenRect, multipleCoverageRect);
        Assert.assertEquals(800, coveragePixels);

        List<Rect> multipleCoverageRectSingleOverlap = new ArrayList<Rect>();
        multipleCoverageRectSingleOverlap.add(new Rect(10, 10, 60, 30));
        multipleCoverageRectSingleOverlap.add(new Rect(50, 10, 80, 30));

        coveragePixels =
                RectUtils.calculatePixelsOfCoverage(screenRect, multipleCoverageRectSingleOverlap);
        Assert.assertEquals(1400, coveragePixels);

        List<Rect> multipleCoverageRectDoubleOverlap = new ArrayList<Rect>();
        multipleCoverageRectDoubleOverlap.add(new Rect(10, 10, 60, 30));
        multipleCoverageRectDoubleOverlap.add(new Rect(50, 10, 80, 30));
        multipleCoverageRectDoubleOverlap.add(new Rect(55, 15, 65, 25));

        coveragePixels =
                RectUtils.calculatePixelsOfCoverage(screenRect, multipleCoverageRectDoubleOverlap);
        Assert.assertEquals(1400, coveragePixels);

        List<Rect> zeroDimensionsCoverageRect = new ArrayList<Rect>();
        zeroDimensionsCoverageRect.add(new Rect(10, 10, 10, 30));
        zeroDimensionsCoverageRect.add(new Rect(10, 10, 80, 10));
        zeroDimensionsCoverageRect.add(new Rect(10, 10, 10, 10));

        coveragePixels =
                RectUtils.calculatePixelsOfCoverage(screenRect, zeroDimensionsCoverageRect);
        Assert.assertEquals(0, coveragePixels);

        List<Rect> touchingCoverageRect = new ArrayList<Rect>();
        touchingCoverageRect.add(new Rect(10, 10, 30, 30));
        touchingCoverageRect.add(new Rect(30, 10, 50, 30));
        touchingCoverageRect.add(new Rect(10, 30, 30, 50));

        coveragePixels = RectUtils.calculatePixelsOfCoverage(screenRect, touchingCoverageRect);
        Assert.assertEquals(1200, coveragePixels);

        List<Rect> clippedCoverageRect = new ArrayList<Rect>();
        clippedCoverageRect.add(new Rect(90, 90, 130, 130));
        clippedCoverageRect.add(new Rect(120, 120, 130, 130));

        coveragePixels = RectUtils.calculatePixelsOfCoverage(screenRect, clippedCoverageRect);
        Assert.assertEquals(100, coveragePixels);
    }
}
