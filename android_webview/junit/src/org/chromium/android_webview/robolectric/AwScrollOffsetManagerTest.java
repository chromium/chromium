// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import android.graphics.Rect;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AwScrollOffsetManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Integration tests for ScrollOffsetManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwScrollOffsetManagerTest {
    private static class TestScrollOffsetManagerDelegate implements AwScrollOffsetManager.Delegate {
        private int mOverScrollDeltaX;
        private int mOverScrollDeltaY;
        private int mOverScrollCallCount;
        private int mScrollX;
        private int mScrollY;
        private int mNativeScrollX;
        private int mNativeScrollY;
        private int mInvalidateCount;

        public int getOverScrollDeltaX() {
            return mOverScrollDeltaX;
        }

        public int getOverScrollDeltaY() {
            return mOverScrollDeltaY;
        }

        public int getOverScrollCallCount() {
            return mOverScrollCallCount;
        }

        public int getScrollX() {
            return mScrollX;
        }

        public int getScrollY() {
            return mScrollY;
        }

        public int getNativeScrollX() {
            return mNativeScrollX;
        }

        public int getNativeScrollY() {
            return mNativeScrollY;
        }

        public int getInvalidateCount() {
            return mInvalidateCount;
        }

        @Override
        public void overScrollContainerViewBy(
                int deltaX,
                int deltaY,
                int scrollX,
                int scrollY,
                int scrollRangeX,
                int scrollRangeY,
                boolean isTouchEvent) {
            mOverScrollDeltaX = deltaX;
            mOverScrollDeltaY = deltaY;
            mOverScrollCallCount += 1;
        }

        @Override
        public void scrollContainerViewTo(int x, int y) {
            mScrollX = x;
            mScrollY = y;
        }

        @Override
        public void scrollNativeTo(int x, int y) {
            mNativeScrollX = x;
            mNativeScrollY = y;
        }

        @Override
        public int getContainerViewScrollX() {
            return mScrollX;
        }

        @Override
        public int getContainerViewScrollY() {
            return mScrollY;
        }

        @Override
        public void invalidate() {
            mInvalidateCount += 1;
        }

        @Override
        public void cancelFling() {}

        @Override
        public void smoothScroll(int targetX, int targetY, long durationMs) {}
    }

    private void simulateScrolling(
            AwScrollOffsetManager offsetManager,
            TestScrollOffsetManagerDelegate delegate,
            int scrollX,
            int scrollY) {
        // Scrolling is a two-phase action. First we ask the manager to scroll
        int callCount = delegate.getOverScrollCallCount();
        offsetManager.scrollContainerViewTo(scrollX, scrollY);
        // The manager then asks the delegate to overscroll the view.
        Assert.assertEquals(callCount + 1, delegate.getOverScrollCallCount());
        Assert.assertEquals(scrollX, delegate.getOverScrollDeltaX() + delegate.getScrollX());
        Assert.assertEquals(scrollY, delegate.getOverScrollDeltaY() + delegate.getScrollY());
        // As a response to that the menager expects the view to call back with the new scroll.
        offsetManager.onContainerViewOverScrolled(scrollX, scrollY, false, false);
    }

    private void simlateOverScrollPropagation(
            AwScrollOffsetManager offsetManager, TestScrollOffsetManagerDelegate delegate) {
        Assert.assertTrue(delegate.getOverScrollCallCount() > 0);

        offsetManager.onContainerViewOverScrolled(
                delegate.getOverScrollDeltaX() + delegate.getScrollX(),
                delegate.getOverScrollDeltaY() + delegate.getScrollY(),
                false,
                false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWhenContentSizeMatchesView() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        final int width = 132;
        final int height = 212;
        final int scrollX = 11;
        final int scrollY = 13;

        offsetManager.setMaxScrollOffset(0, 0);
        offsetManager.setContainerViewSize(width, height);

        Assert.assertEquals(width, offsetManager.computeHorizontalScrollRange());
        Assert.assertEquals(height, offsetManager.computeVerticalScrollRange());

        // Since the view size and contents size are equal no scrolling should be possible.
        Assert.assertEquals(0, offsetManager.computeMaximumHorizontalScrollOffset());
        Assert.assertEquals(0, offsetManager.computeMaximumVerticalScrollOffset());

        // Scrolling should generate overscroll but not update the scroll offset.
        simulateScrolling(offsetManager, delegate, scrollX, scrollY);
        Assert.assertEquals(scrollX, delegate.getOverScrollDeltaX());
        Assert.assertEquals(scrollY, delegate.getOverScrollDeltaY());
        Assert.assertEquals(0, delegate.getScrollX());
        Assert.assertEquals(0, delegate.getScrollY());
        Assert.assertEquals(0, delegate.getNativeScrollX());
        Assert.assertEquals(0, delegate.getNativeScrollY());

        // Scrolling to 0,0 should result in no deltas.
        simulateScrolling(offsetManager, delegate, 0, 0);
        Assert.assertEquals(0, delegate.getOverScrollDeltaX());
        Assert.assertEquals(0, delegate.getOverScrollDeltaY());

        // Negative scrolling should result in negative deltas but no scroll offset update.
        simulateScrolling(offsetManager, delegate, -scrollX, -scrollY);
        Assert.assertEquals(-scrollX, delegate.getOverScrollDeltaX());
        Assert.assertEquals(-scrollY, delegate.getOverScrollDeltaY());
        Assert.assertEquals(0, delegate.getScrollX());
        Assert.assertEquals(0, delegate.getScrollY());
        Assert.assertEquals(0, delegate.getNativeScrollX());
        Assert.assertEquals(0, delegate.getNativeScrollY());
    }

    private static final int VIEW_WIDTH = 211;
    private static final int VIEW_HEIGHT = 312;
    private static final int MAX_HORIZONTAL_OFFSET = 757;
    private static final int MAX_VERTICAL_OFFSET = 127;
    private static final int CONTENT_WIDTH = VIEW_WIDTH + MAX_HORIZONTAL_OFFSET;
    private static final int CONTENT_HEIGHT = VIEW_HEIGHT + MAX_VERTICAL_OFFSET;

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testScrollRangeAndMaxOffset() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        offsetManager.setMaxScrollOffset(MAX_HORIZONTAL_OFFSET, MAX_VERTICAL_OFFSET);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        Assert.assertEquals(CONTENT_WIDTH, offsetManager.computeHorizontalScrollRange());
        Assert.assertEquals(CONTENT_HEIGHT, offsetManager.computeVerticalScrollRange());

        Assert.assertEquals(
                MAX_HORIZONTAL_OFFSET, offsetManager.computeMaximumHorizontalScrollOffset());
        Assert.assertEquals(
                MAX_VERTICAL_OFFSET, offsetManager.computeMaximumVerticalScrollOffset());

        // Scrolling beyond the maximum should be clamped.
        final int scrollX = MAX_HORIZONTAL_OFFSET + 10;
        final int scrollY = MAX_VERTICAL_OFFSET + 11;

        simulateScrolling(offsetManager, delegate, scrollX, scrollY);
        Assert.assertEquals(scrollX, delegate.getOverScrollDeltaX());
        Assert.assertEquals(scrollY, delegate.getOverScrollDeltaY());
        Assert.assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getScrollX());
        Assert.assertEquals(MAX_VERTICAL_OFFSET, delegate.getScrollY());
        Assert.assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getNativeScrollX());
        Assert.assertEquals(MAX_VERTICAL_OFFSET, delegate.getNativeScrollY());

        // Scrolling to negative coordinates should be clamped back to 0,0.
        simulateScrolling(offsetManager, delegate, -scrollX, -scrollY);
        Assert.assertEquals(0, delegate.getScrollX());
        Assert.assertEquals(0, delegate.getScrollY());
        Assert.assertEquals(0, delegate.getNativeScrollX());
        Assert.assertEquals(0, delegate.getNativeScrollY());

        // The onScrollChanged method is callable by third party code and should also be clamped
        offsetManager.onContainerViewScrollChanged(scrollX, scrollY);
        Assert.assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getNativeScrollX());
        Assert.assertEquals(MAX_VERTICAL_OFFSET, delegate.getNativeScrollY());

        offsetManager.onContainerViewScrollChanged(-scrollX, -scrollY);
        Assert.assertEquals(0, delegate.getNativeScrollX());
        Assert.assertEquals(0, delegate.getNativeScrollY());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDelegateCanOverrideScroll() {
        final int overrideScrollX = 10;
        final int overrideScrollY = 10;

        TestScrollOffsetManagerDelegate delegate =
                new TestScrollOffsetManagerDelegate() {
                    @Override
                    public int getContainerViewScrollX() {
                        return overrideScrollX;
                    }

                    @Override
                    public int getContainerViewScrollY() {
                        return overrideScrollY;
                    }
                };
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        offsetManager.setMaxScrollOffset(MAX_HORIZONTAL_OFFSET, MAX_VERTICAL_OFFSET);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        offsetManager.onContainerViewOverScrolled(0, 0, false, false);
        Assert.assertEquals(overrideScrollX, delegate.getNativeScrollX());
        Assert.assertEquals(overrideScrollY, delegate.getNativeScrollY());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDelegateOverridenScrollsDontExceedBounds() {
        final int overrideScrollX = MAX_HORIZONTAL_OFFSET + 10;
        final int overrideScrollY = MAX_VERTICAL_OFFSET + 20;
        TestScrollOffsetManagerDelegate delegate =
                new TestScrollOffsetManagerDelegate() {
                    @Override
                    public int getContainerViewScrollX() {
                        return overrideScrollX;
                    }

                    @Override
                    public int getContainerViewScrollY() {
                        return overrideScrollY;
                    }
                };
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        offsetManager.setMaxScrollOffset(MAX_HORIZONTAL_OFFSET, MAX_VERTICAL_OFFSET);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        offsetManager.onContainerViewOverScrolled(0, 0, false, false);
        Assert.assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getNativeScrollX());
        Assert.assertEquals(MAX_VERTICAL_OFFSET, delegate.getNativeScrollY());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testScrollContainerViewTo() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        final int scrollX = 31;
        final int scrollY = 41;

        offsetManager.setMaxScrollOffset(MAX_HORIZONTAL_OFFSET, MAX_VERTICAL_OFFSET);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        Assert.assertEquals(0, delegate.getOverScrollDeltaX());
        Assert.assertEquals(0, delegate.getOverScrollDeltaY());
        int callCount = delegate.getOverScrollCallCount();

        offsetManager.scrollContainerViewTo(scrollX, scrollY);
        Assert.assertEquals(callCount + 1, delegate.getOverScrollCallCount());
        Assert.assertEquals(scrollX, delegate.getOverScrollDeltaX());
        Assert.assertEquals(scrollY, delegate.getOverScrollDeltaY());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnContainerViewOverScrolled() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        final int scrollX = 31;
        final int scrollY = 41;

        offsetManager.setMaxScrollOffset(MAX_HORIZONTAL_OFFSET, MAX_VERTICAL_OFFSET);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        Assert.assertEquals(0, delegate.getScrollX());
        Assert.assertEquals(0, delegate.getScrollY());
        Assert.assertEquals(0, delegate.getNativeScrollX());
        Assert.assertEquals(0, delegate.getNativeScrollY());

        offsetManager.onContainerViewOverScrolled(scrollX, scrollY, false, false);
        Assert.assertEquals(scrollX, delegate.getScrollX());
        Assert.assertEquals(scrollY, delegate.getScrollY());
        Assert.assertEquals(scrollX, delegate.getNativeScrollX());
        Assert.assertEquals(scrollY, delegate.getNativeScrollY());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDefersScrollUntilTouchEnd() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        final int scrollX = 31;
        final int scrollY = 41;

        offsetManager.setMaxScrollOffset(MAX_HORIZONTAL_OFFSET, MAX_VERTICAL_OFFSET);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        offsetManager.setProcessingTouchEvent(true);
        offsetManager.onContainerViewOverScrolled(scrollX, scrollY, false, false);
        Assert.assertEquals(scrollX, delegate.getScrollX());
        Assert.assertEquals(scrollY, delegate.getScrollY());
        Assert.assertEquals(0, delegate.getNativeScrollX());
        Assert.assertEquals(0, delegate.getNativeScrollY());

        offsetManager.setProcessingTouchEvent(false);
        Assert.assertEquals(scrollX, delegate.getScrollX());
        Assert.assertEquals(scrollY, delegate.getScrollY());
        Assert.assertEquals(scrollX, delegate.getNativeScrollX());
        Assert.assertEquals(scrollY, delegate.getNativeScrollY());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRequestChildRectangleOnScreenDontScrollIfAlreadyThere() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        offsetManager.setMaxScrollOffset(MAX_HORIZONTAL_OFFSET, MAX_VERTICAL_OFFSET);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        offsetManager.requestChildRectangleOnScreen(
                0, 0, new Rect(0, 0, VIEW_WIDTH / 4, VIEW_HEIGHT / 4), true);
        Assert.assertEquals(0, delegate.getOverScrollDeltaX());
        Assert.assertEquals(0, delegate.getOverScrollDeltaY());
        Assert.assertEquals(0, delegate.getScrollX());
        Assert.assertEquals(0, delegate.getScrollY());

        offsetManager.requestChildRectangleOnScreen(
                3 * VIEW_WIDTH / 4,
                3 * VIEW_HEIGHT / 4,
                new Rect(0, 0, VIEW_WIDTH / 4, VIEW_HEIGHT / 4),
                true);
        Assert.assertEquals(0, delegate.getOverScrollDeltaX());
        Assert.assertEquals(0, delegate.getOverScrollDeltaY());
        Assert.assertEquals(0, delegate.getScrollX());
        Assert.assertEquals(0, delegate.getScrollY());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRequestChildRectangleOnScreenScrollToBottom() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        final int rectWidth = 2;
        final int rectHeight = 3;

        offsetManager.setMaxScrollOffset(MAX_HORIZONTAL_OFFSET, MAX_VERTICAL_OFFSET);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        offsetManager.requestChildRectangleOnScreen(
                CONTENT_WIDTH - rectWidth,
                CONTENT_HEIGHT - rectHeight,
                new Rect(0, 0, rectWidth, rectHeight),
                true);
        simlateOverScrollPropagation(offsetManager, delegate);
        Assert.assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getOverScrollDeltaX());
        Assert.assertEquals(
                CONTENT_HEIGHT - rectHeight - VIEW_HEIGHT / 3, delegate.getOverScrollDeltaY());
        Assert.assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getScrollX());
        Assert.assertEquals(MAX_VERTICAL_OFFSET, delegate.getScrollY());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRequestChildRectangleOnScreenScrollToBottomLargeRect() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        final int rectWidth = VIEW_WIDTH;
        final int rectHeight = VIEW_HEIGHT;

        offsetManager.setMaxScrollOffset(MAX_HORIZONTAL_OFFSET, MAX_VERTICAL_OFFSET);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);

        offsetManager.requestChildRectangleOnScreen(
                CONTENT_WIDTH - rectWidth,
                CONTENT_HEIGHT - rectHeight,
                new Rect(0, 0, rectWidth, rectHeight),
                true);
        simlateOverScrollPropagation(offsetManager, delegate);
        Assert.assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getOverScrollDeltaX());
        Assert.assertEquals(MAX_VERTICAL_OFFSET, delegate.getOverScrollDeltaY());
        Assert.assertEquals(MAX_HORIZONTAL_OFFSET, delegate.getScrollX());
        Assert.assertEquals(MAX_VERTICAL_OFFSET, delegate.getScrollY());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRequestChildRectangleOnScreenScrollToTop() {
        TestScrollOffsetManagerDelegate delegate = new TestScrollOffsetManagerDelegate();
        AwScrollOffsetManager offsetManager = new AwScrollOffsetManager(delegate);

        final int rectWidth = 2;
        final int rectHeight = 3;

        offsetManager.setMaxScrollOffset(MAX_HORIZONTAL_OFFSET, MAX_VERTICAL_OFFSET);
        offsetManager.setContainerViewSize(VIEW_WIDTH, VIEW_HEIGHT);
        simulateScrolling(
                offsetManager, delegate, CONTENT_WIDTH - VIEW_WIDTH, CONTENT_HEIGHT - VIEW_HEIGHT);

        offsetManager.requestChildRectangleOnScreen(
                0, 0, new Rect(0, 0, rectWidth, rectHeight), true);
        simlateOverScrollPropagation(offsetManager, delegate);
        Assert.assertEquals(-CONTENT_WIDTH + VIEW_WIDTH, delegate.getOverScrollDeltaX());
        Assert.assertEquals(-CONTENT_HEIGHT + VIEW_HEIGHT, delegate.getOverScrollDeltaY());
        Assert.assertEquals(0, delegate.getScrollX());
        Assert.assertEquals(0, delegate.getScrollX());
    }
}
