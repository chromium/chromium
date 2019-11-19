// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.View.MeasureSpec;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AwLayoutSizer;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwLayoutSizerTest {
    static class LayoutSizerDelegate implements AwLayoutSizer.Delegate {
        public int requestLayoutCallCount;
        public boolean setMeasuredDimensionCalled;
        public int measuredWidth;
        public int measuredHeight;
        public boolean forceZeroHeight;
        public boolean heightWrapContent;

        @Override
        public void requestLayout() {
            requestLayoutCallCount++;
        }

        @Override
        public void setMeasuredDimension(int measuredWidth, int measuredHeight) {
            setMeasuredDimensionCalled = true;
            this.measuredWidth = measuredWidth;
            this.measuredHeight = measuredHeight;
        }

        @Override
        public void setForceZeroLayoutHeight(boolean forceZeroHeight) {
            this.forceZeroHeight = forceZeroHeight;
        }

        @Override
        public boolean isLayoutParamsHeightWrapContent() {
            return heightWrapContent;
        }
    }

    private static final int FIRST_CONTENT_WIDTH = 101;
    private static final int FIRST_CONTENT_HEIGHT = 389;
    private static final int SECOND_CONTENT_WIDTH = 103;
    private static final int SECOND_CONTENT_HEIGHT = 397;

    private static final int SMALLER_CONTENT_SIZE = 25;
    private static final int AT_MOST_MEASURE_SIZE = 50;
    private static final int TOO_LARGE_CONTENT_SIZE = 100;

    private static final float INITIAL_PAGE_SCALE = 1.0f;
    private static final double DIP_SCALE = 1.0;

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCanQueryContentSize() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        final int contentWidth = 101;
        final int contentHeight = 389;

        layoutSizer.onContentSizeChanged(contentWidth, contentHeight);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

        Assert.assertTrue(delegate.setMeasuredDimensionCalled);
        Assert.assertEquals(contentWidth, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        Assert.assertEquals(contentHeight, delegate.measuredHeight & View.MEASURED_SIZE_MASK);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentSizeChangeRequestsLayout() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SECOND_CONTENT_WIDTH, SECOND_CONTENT_WIDTH);

        Assert.assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentSizeChangeDoesNotRequestLayoutIfMeasuredExcatly() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SECOND_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);

        Assert.assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDuplicateContentSizeChangeDoesNotRequestLayout() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);

        Assert.assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentHeightGrowsTillAtMostSize() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, SMALLER_CONTENT_SIZE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        Assert.assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth);
        Assert.assertEquals(SMALLER_CONTENT_SIZE, delegate.measuredHeight);

        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        Assert.assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        Assert.assertEquals(
                AT_MOST_MEASURE_SIZE, delegate.measuredHeight & View.MEASURED_SIZE_MASK);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentHeightGrowthRequestsLayoutInAtMostSizeMode() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, SMALLER_CONTENT_SIZE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        Assert.assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth);
        Assert.assertEquals(SMALLER_CONTENT_SIZE, delegate.measuredHeight);

        int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, AT_MOST_MEASURE_SIZE - 1);
        Assert.assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);

        // Test that crossing the AT_MOST_MEASURE_SIZE threshold results in a requestLayout.
        requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, AT_MOST_MEASURE_SIZE + 1);
        Assert.assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentHeightShrinksAfterAtMostSize() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, SMALLER_CONTENT_SIZE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        Assert.assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth);
        Assert.assertEquals(SMALLER_CONTENT_SIZE, delegate.measuredHeight);

        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        Assert.assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        Assert.assertEquals(
                AT_MOST_MEASURE_SIZE, delegate.measuredHeight & View.MEASURED_SIZE_MASK);

        int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE + 1);
        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE);
        Assert.assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);

        requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, SMALLER_CONTENT_SIZE);
        Assert.assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        Assert.assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth);
        Assert.assertEquals(SMALLER_CONTENT_SIZE, delegate.measuredHeight);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testScaleChangeRequestsLayout() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE + 0.5f);

        Assert.assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDuplicateScaleChangeDoesNotRequestLayout() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(50, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onPageScaleChanged(INITIAL_PAGE_SCALE);

        Assert.assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testScaleChangeGrowsTillAtMostSize() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        final float tooLargePageScale = 3.00f;

        layoutSizer.onContentSizeChanged(SMALLER_CONTENT_SIZE, SMALLER_CONTENT_SIZE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        Assert.assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth);
        Assert.assertEquals(SMALLER_CONTENT_SIZE, delegate.measuredHeight);

        layoutSizer.onPageScaleChanged(tooLargePageScale);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        Assert.assertEquals(AT_MOST_MEASURE_SIZE, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        Assert.assertEquals(
                AT_MOST_MEASURE_SIZE, delegate.measuredHeight & View.MEASURED_SIZE_MASK);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFreezeAndUnfreezeDoesntCauseLayout() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.freezeLayoutRequests();
        layoutSizer.unfreezeLayoutRequests();
        Assert.assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFreezeInhibitsLayoutRequest() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.freezeLayoutRequests();
        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SECOND_CONTENT_WIDTH, SECOND_CONTENT_WIDTH);
        Assert.assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUnfreezeIssuesLayoutRequest() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.freezeLayoutRequests();
        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        final int requestLayoutCallCount = delegate.requestLayoutCallCount;
        layoutSizer.onContentSizeChanged(SECOND_CONTENT_WIDTH, SECOND_CONTENT_WIDTH);
        Assert.assertEquals(requestLayoutCallCount, delegate.requestLayoutCallCount);
        layoutSizer.unfreezeLayoutRequests();
        Assert.assertEquals(requestLayoutCallCount + 1, delegate.requestLayoutCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testViewportWithExactMeasureSpec() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);

        final float dipScale = 2.0f;
        final int measuredWidth = 800;
        final int measuredHeight = 400;

        layoutSizer.setDIPScale(dipScale);

        layoutSizer.onContentSizeChanged(FIRST_CONTENT_WIDTH, FIRST_CONTENT_HEIGHT);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(measuredWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(measuredHeight, MeasureSpec.EXACTLY));
        Assert.assertEquals(measuredWidth, delegate.measuredWidth & View.MEASURED_SIZE_MASK);
        Assert.assertEquals(measuredHeight, delegate.measuredHeight & View.MEASURED_SIZE_MASK);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testViewportDipSizeOverrideRounding() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        layoutSizer.setDelegate(delegate);

        final float dipScale = 0.666f;

        int contentWidth = 9;
        int contentHeight = 6;

        layoutSizer.setDIPScale(dipScale);
        layoutSizer.onContentSizeChanged(contentWidth, contentHeight);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));

        Assert.assertTrue(delegate.setMeasuredDimensionCalled);
        int measuredWidth = delegate.measuredWidth & View.MEASURED_SIZE_MASK;
        int measuredHeight = delegate.measuredHeight & View.MEASURED_SIZE_MASK;
        Assert.assertNotEquals((int) Math.ceil(measuredWidth / dipScale), contentWidth);
        Assert.assertNotEquals((int) Math.ceil(measuredHeight / dipScale), contentHeight);

        layoutSizer.onSizeChanged(measuredWidth, measuredHeight, 0, 0);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testViewportWithAtMostMeasureSpec() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        delegate.heightWrapContent = true;
        layoutSizer.setDelegate(delegate);

        final float dipScale = 1.5f;
        final int pageScale = 2;
        final int dipAndPageScale = (int) (dipScale * pageScale);

        int contentWidth = 800;
        int contentHeight = 400;
        int contentWidthPix = contentWidth * dipAndPageScale;
        int contentHeightPix = contentHeight * dipAndPageScale;

        Assert.assertFalse(delegate.forceZeroHeight);

        layoutSizer.setDIPScale(dipScale);
        layoutSizer.onContentSizeChanged(contentWidth, contentHeight);
        layoutSizer.onPageScaleChanged(pageScale);
        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(contentWidthPix, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(contentHeightPix * 2, MeasureSpec.AT_MOST));

        Assert.assertTrue(delegate.setMeasuredDimensionCalled);
        Assert.assertFalse(delegate.forceZeroHeight);

        int measuredWidth = delegate.measuredWidth & View.MEASURED_SIZE_MASK;
        int measuredHeight = delegate.measuredHeight & View.MEASURED_SIZE_MASK;
        layoutSizer.onSizeChanged(measuredWidth, measuredHeight, 0, 0);

        Assert.assertTrue(delegate.forceZeroHeight);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFixedLayoutSizeDependsOnHeightWrapContent() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        delegate.heightWrapContent = false;
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        layoutSizer.onSizeChanged(AT_MOST_MEASURE_SIZE, AT_MOST_MEASURE_SIZE, 0, 0);

        Assert.assertFalse(delegate.forceZeroHeight);

        delegate.heightWrapContent = true;
        layoutSizer.onSizeChanged(AT_MOST_MEASURE_SIZE, AT_MOST_MEASURE_SIZE, 0, 0);

        Assert.assertTrue(delegate.forceZeroHeight);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFixedLayoutSizeDoesNotDependOnMeasureSpec() {
        AwLayoutSizer layoutSizer = new AwLayoutSizer();
        LayoutSizerDelegate delegate = new LayoutSizerDelegate();
        delegate.heightWrapContent = false;
        layoutSizer.setDelegate(delegate);
        layoutSizer.setDIPScale(DIP_SCALE);

        layoutSizer.onContentSizeChanged(TOO_LARGE_CONTENT_SIZE, TOO_LARGE_CONTENT_SIZE);
        layoutSizer.onMeasure(
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(AT_MOST_MEASURE_SIZE, MeasureSpec.AT_MOST));
        layoutSizer.onSizeChanged(AT_MOST_MEASURE_SIZE, AT_MOST_MEASURE_SIZE, 0, 0);

        Assert.assertFalse(delegate.forceZeroHeight);

        layoutSizer.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        layoutSizer.onSizeChanged(AT_MOST_MEASURE_SIZE, AT_MOST_MEASURE_SIZE, 0, 0);
        Assert.assertFalse(delegate.forceZeroHeight);
    }
}