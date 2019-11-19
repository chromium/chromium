// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.omnibox.suggestions.base.SimpleHorizontalLayoutView.LayoutParams;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for {@link SimpleHorizontalLayoutView}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SimpleHorizontalLayoutViewTest {
    private static final int SMALL_VIEW_WIDTH = 50;
    private static final int LARGE_VIEW_WIDTH = 120;

    private SimpleHorizontalLayoutViewForTest mView;
    private Activity mActivity;

    private View mSmallView;
    private View mLargeView;
    private View mDynamicView;

    class SimpleHorizontalLayoutViewForTest extends SimpleHorizontalLayoutView {
        private int mCurrentDirection = View.LAYOUT_DIRECTION_LTR;

        SimpleHorizontalLayoutViewForTest(Context context) {
            super(context);
        }

        @Override
        public void setLayoutDirection(int newDirection) {
            mCurrentDirection = newDirection;
        }

        @Override
        public int getLayoutDirection() {
            return mCurrentDirection;
        }

        /**
         * Test method to force layout update based on specified view dimensions.
         */
        void performLayoutForTest(int width) {
            onMeasure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                    MeasureSpec.UNSPECIFIED);

            // Note: height is computed by onMeasure call.
            final int height = getMeasuredHeight();
            onLayout(true, 0, 0, width, height);
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mView = new SimpleHorizontalLayoutViewForTest(mActivity);

        mSmallView = new View(mActivity);
        mSmallView.setLayoutParams(new LayoutParams(SMALL_VIEW_WIDTH, LayoutParams.MATCH_PARENT));
        mLargeView = new View(mActivity);
        mLargeView.setLayoutParams(new LayoutParams(LARGE_VIEW_WIDTH, LayoutParams.MATCH_PARENT));
        mDynamicView = new View(mActivity);
        mDynamicView.setLayoutParams(LayoutParams.forDynamicView());
    }

    /**
     * Perform the measure and layout pass on the SimpleHorizontalLayoutView.
     * This method sets up the basic properties of the Suggestion container, specifies height of the
     * content view and executes the measure and layout pass.
     */
    private void executeLayoutTest(int containerWidth, int contentHeight, int layoutDirection) {
        mView.setLayoutDirection(layoutDirection);
        Assert.assertEquals(
                "layout direction not supported", layoutDirection, mView.getLayoutDirection());

        // Let ContentView drive the height of the Suggestion. The dummy view could shrink, so let's
        // prevent that from happening. We don't technically have any content, so we need to prevent
        // the view from shrinking, too.
        mDynamicView.setMinimumHeight(contentHeight);

        mView.performLayoutForTest(containerWidth);
    }

    /**
     * Confirm that specified view is positioned at specific coordinates.
     */
    private void verifyViewLayout(View v, int left, int top, int right, int bottom) {
        Assert.assertEquals("left view edge", left, v.getLeft());
        Assert.assertEquals("top view edge", top, v.getTop());
        Assert.assertEquals("right view edge", right, v.getRight());
        Assert.assertEquals("bottom view edge", bottom, v.getBottom());
        Assert.assertEquals("view width", right - left, v.getMeasuredWidth());
        Assert.assertEquals("view height", bottom - top, v.getMeasuredHeight());
    }

    /**
     * LTR layout with dynamic view in the middle.
     * [   LARGE   | DYNAMIC         |SMALL]
     */
    @Test
    public void layout_LtrWithDynamicInbetween() {
        final int useContentWidth = 123;
        final int giveSuggestionWidth = SMALL_VIEW_WIDTH + LARGE_VIEW_WIDTH + useContentWidth;
        final int giveContentHeight = 15;

        mView.addView(mLargeView);
        mView.addView(mDynamicView);
        mView.addView(mSmallView);

        final int expectedLargeCornerLeft = 0;
        final int expectedLargeCornerRight = LARGE_VIEW_WIDTH;
        final int expectedDynamicCornerLeft = expectedLargeCornerRight;
        final int expectedDynamicCornerRight = expectedDynamicCornerLeft + useContentWidth;
        final int expectedSmallCornerLeft = expectedDynamicCornerRight;
        final int expectedSmallCornerRight = expectedSmallCornerLeft + SMALL_VIEW_WIDTH;

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);

        verifyViewLayout(mLargeView, expectedLargeCornerLeft, 0, expectedLargeCornerRight,
                giveContentHeight);

        verifyViewLayout(mDynamicView, expectedDynamicCornerLeft, 0, expectedDynamicCornerRight,
                giveContentHeight);

        verifyViewLayout(mSmallView, expectedSmallCornerLeft, 0, expectedSmallCornerRight,
                giveContentHeight);
    }

    /**
     * RTL layout with dynamic view in the middle.
     * [SMALL| DYNAMIC         |   LARGE   ]
     */
    @Test
    public void layout_RtlWithDynamicInbetween() {
        final int useContentWidth = 234;
        final int giveSuggestionWidth = SMALL_VIEW_WIDTH + LARGE_VIEW_WIDTH + useContentWidth;
        final int giveContentHeight = 18;

        mView.addView(mLargeView);
        mView.addView(mDynamicView);
        mView.addView(mSmallView);

        final int expectedSmallCornerLeft = 0;
        final int expectedSmallCornerRight = SMALL_VIEW_WIDTH;
        final int expectedDynamicCornerLeft = expectedSmallCornerRight;
        final int expectedDynamicCornerRight = expectedDynamicCornerLeft + useContentWidth;
        final int expectedLargeCornerLeft = expectedDynamicCornerRight;
        final int expectedLargeCornerRight = expectedLargeCornerLeft + LARGE_VIEW_WIDTH;

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);

        verifyViewLayout(mLargeView, expectedLargeCornerLeft, 0, expectedLargeCornerRight,
                giveContentHeight);

        verifyViewLayout(mDynamicView, expectedDynamicCornerLeft, 0, expectedDynamicCornerRight,
                giveContentHeight);

        verifyViewLayout(mSmallView, expectedSmallCornerLeft, 0, expectedSmallCornerRight,
                giveContentHeight);
    }

    /**
     * LTR layout with dynamic view in the middle, and first fixed-size view element hidden.
     * [DYNAMIC          |SMALL]
     */
    @Test
    public void layout_LtrWithDynamicInbetween_FirstViewHidden() {
        final int useContentWidth = 123;
        final int giveSuggestionWidth = SMALL_VIEW_WIDTH + useContentWidth;
        final int giveContentHeight = 15;

        mLargeView.setVisibility(View.GONE);
        mView.addView(mLargeView);
        mView.addView(mDynamicView);
        mView.addView(mSmallView);

        final int expectedDynamicCornerLeft = 0;
        final int expectedDynamicCornerRight = expectedDynamicCornerLeft + useContentWidth;
        final int expectedSmallCornerLeft = expectedDynamicCornerRight;
        final int expectedSmallCornerRight = expectedSmallCornerLeft + SMALL_VIEW_WIDTH;

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);

        verifyViewLayout(mDynamicView, expectedDynamicCornerLeft, 0, expectedDynamicCornerRight,
                giveContentHeight);

        verifyViewLayout(mSmallView, expectedSmallCornerLeft, 0, expectedSmallCornerRight,
                giveContentHeight);
    }

    /**
     * RTL layout with dynamic view in the middle, and last fixed-size view element hidden.
     * [DYNAMIC          |   LARGE   ]
     */
    @Test
    public void layout_RtlWithDynamicInbetween_LastViewHidden() {
        final int useContentWidth = 234;
        final int giveSuggestionWidth = LARGE_VIEW_WIDTH + useContentWidth;
        final int giveContentHeight = 18;

        mSmallView.setVisibility(View.GONE);
        mView.addView(mLargeView);
        mView.addView(mDynamicView);
        mView.addView(mSmallView);

        final int expectedDynamicCornerLeft = 0;
        final int expectedDynamicCornerRight = expectedDynamicCornerLeft + useContentWidth;
        final int expectedLargeCornerLeft = expectedDynamicCornerRight;
        final int expectedLargeCornerRight = expectedLargeCornerLeft + LARGE_VIEW_WIDTH;

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);

        verifyViewLayout(mLargeView, expectedLargeCornerLeft, 0, expectedLargeCornerRight,
                giveContentHeight);

        verifyViewLayout(mDynamicView, expectedDynamicCornerLeft, 0, expectedDynamicCornerRight,
                giveContentHeight);
    }

    /**
     * LTR layout with dynamic view positioned first.
     * [ DYNAMIC         |   LARGE   |SMALL]
     */
    @Test
    public void layout_LtrWithDynamicFirst() {
        final int useContentWidth = 135;
        final int giveSuggestionWidth = SMALL_VIEW_WIDTH + LARGE_VIEW_WIDTH + useContentWidth;
        final int giveContentHeight = 25;

        mView.addView(mDynamicView);
        mView.addView(mLargeView);
        mView.addView(mSmallView);

        final int expectedDynamicCornerLeft = 0;
        final int expectedDynamicCornerRight = useContentWidth;
        final int expectedLargeCornerLeft = expectedDynamicCornerRight;
        final int expectedLargeCornerRight = expectedLargeCornerLeft + LARGE_VIEW_WIDTH;
        final int expectedSmallCornerLeft = expectedLargeCornerRight;
        final int expectedSmallCornerRight = expectedSmallCornerLeft + SMALL_VIEW_WIDTH;

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);

        verifyViewLayout(mLargeView, expectedLargeCornerLeft, 0, expectedLargeCornerRight,
                giveContentHeight);

        verifyViewLayout(mDynamicView, expectedDynamicCornerLeft, 0, expectedDynamicCornerRight,
                giveContentHeight);

        verifyViewLayout(mSmallView, expectedSmallCornerLeft, 0, expectedSmallCornerRight,
                giveContentHeight);
    }

    /**
     * RTL layout with dynamic view positioned first
     * [   LARGE   |SMALL| DYNAMIC         ]
     */
    @Test
    public void layout_RtlWithDynamicFirst() {
        final int useContentWidth = 246;
        final int giveSuggestionWidth = SMALL_VIEW_WIDTH + LARGE_VIEW_WIDTH + useContentWidth;
        final int giveContentHeight = 28;

        mView.addView(mDynamicView);
        mView.addView(mSmallView);
        mView.addView(mLargeView);

        final int expectedLargeCornerLeft = 0;
        final int expectedLargeCornerRight = LARGE_VIEW_WIDTH;
        final int expectedSmallCornerLeft = expectedLargeCornerRight;
        final int expectedSmallCornerRight = expectedSmallCornerLeft + SMALL_VIEW_WIDTH;
        final int expectedDynamicCornerLeft = expectedSmallCornerRight;
        final int expectedDynamicCornerRight = expectedDynamicCornerLeft + useContentWidth;

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);

        verifyViewLayout(mLargeView, expectedLargeCornerLeft, 0, expectedLargeCornerRight,
                giveContentHeight);

        verifyViewLayout(mDynamicView, expectedDynamicCornerLeft, 0, expectedDynamicCornerRight,
                giveContentHeight);

        verifyViewLayout(mSmallView, expectedSmallCornerLeft, 0, expectedSmallCornerRight,
                giveContentHeight);
    }

    /**
     * LTR layout with dynamic view positioned last.
     * [   LARGE   |SMALL| DYNAMIC         ]
     */
    @Test
    public void layout_LtrWithDynamicLast() {
        final int useContentWidth = 147;
        final int giveSuggestionWidth = SMALL_VIEW_WIDTH + LARGE_VIEW_WIDTH + useContentWidth;
        final int giveContentHeight = 17;

        mView.addView(mLargeView);
        mView.addView(mSmallView);
        mView.addView(mDynamicView);

        final int expectedLargeCornerLeft = 0;
        final int expectedLargeCornerRight = LARGE_VIEW_WIDTH;
        final int expectedSmallCornerLeft = expectedLargeCornerRight;
        final int expectedSmallCornerRight = expectedSmallCornerLeft + SMALL_VIEW_WIDTH;
        final int expectedDynamicCornerLeft = expectedSmallCornerRight;
        final int expectedDynamicCornerRight = expectedDynamicCornerLeft + useContentWidth;

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);

        verifyViewLayout(mLargeView, expectedLargeCornerLeft, 0, expectedLargeCornerRight,
                giveContentHeight);

        verifyViewLayout(mDynamicView, expectedDynamicCornerLeft, 0, expectedDynamicCornerRight,
                giveContentHeight);

        verifyViewLayout(mSmallView, expectedSmallCornerLeft, 0, expectedSmallCornerRight,
                giveContentHeight);
    }

    /**
     * RTL layout with dynamic view positioned last.
     * [ DYNAMIC         |   LARGE   |SMALL]
     */
    @Test
    public void layout_RtlWithDynamicLast() {
        final int useContentWidth = 258;
        final int giveSuggestionWidth = SMALL_VIEW_WIDTH + LARGE_VIEW_WIDTH + useContentWidth;
        final int giveContentHeight = 27;

        mView.addView(mSmallView);
        mView.addView(mLargeView);
        mView.addView(mDynamicView);

        final int expectedDynamicCornerLeft = 0;
        final int expectedDynamicCornerRight = useContentWidth;
        final int expectedLargeCornerLeft = expectedDynamicCornerRight;
        final int expectedLargeCornerRight = expectedLargeCornerLeft + LARGE_VIEW_WIDTH;
        final int expectedSmallCornerLeft = expectedLargeCornerRight;
        final int expectedSmallCornerRight = expectedSmallCornerLeft + SMALL_VIEW_WIDTH;

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);

        verifyViewLayout(mLargeView, expectedLargeCornerLeft, 0, expectedLargeCornerRight,
                giveContentHeight);

        verifyViewLayout(mDynamicView, expectedDynamicCornerLeft, 0, expectedDynamicCornerRight,
                giveContentHeight);

        verifyViewLayout(mSmallView, expectedSmallCornerLeft, 0, expectedSmallCornerRight,
                giveContentHeight);
    }

    /**
     * Two dynamic views. Expect the layout mechanism to fail.
     */
    @Test(expected = AssertionError.class)
    public void layout_MultipleDynamicViews() {
        View dynamicView2 = new View(mActivity);
        dynamicView2.setLayoutParams(LayoutParams.forDynamicView());

        mView.addView(mDynamicView);
        mView.addView(dynamicView2);

        executeLayoutTest(100, 100, View.LAYOUT_DIRECTION_LTR);
    }

    /**
     * No dynamic views. Expect the layout mechanism to fail.
     */
    @Test(expected = AssertionError.class)
    public void layout_NoDynamicViews() {
        View dynamicView2 = new View(mActivity);
        mView.addView(mLargeView);
        mView.addView(mSmallView);

        executeLayoutTest(100, 100, View.LAYOUT_DIRECTION_LTR);
    }
}
