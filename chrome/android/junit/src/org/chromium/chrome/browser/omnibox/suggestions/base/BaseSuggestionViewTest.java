// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.hamcrest.Matchers.lessThanOrEqualTo;

import android.app.Activity;
import android.view.View;
import android.view.View.MeasureSpec;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.R;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for {@link BaseSuggestionView}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseSuggestionViewTest {
    // Used as a (fixed) margin between screen edge and refine icon.
    private int mSuggestionPaddingEndPx;
    // Used as a (fixed) width of a refine icon.
    private int mActionIconWidthPx;

    private BaseSuggestionViewForTest mView;
    private Activity mActivity;
    private View mRefineView;
    private View mDecoratedView;
    private View mContentView;

    // IMPORTANT: We need to extend the tested class here to support functionality currently
    // omitted by Robolectric, that is relevant to the tests below (layout direction change).
    //
    // TODO(https://github.com/robolectric/robolectric/issues/3910) Remove the class below once
    // the above issue is resolved and our robolectric version is rolled forward to the version
    // that supports layout direction changes.
    static class BaseSuggestionViewForTest extends BaseSuggestionView {
        private int mCurrentDirection = View.LAYOUT_DIRECTION_LTR;

        BaseSuggestionViewForTest(View childView) {
            super(childView);
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

        View getDecoratedView() {
            return mContentView;
        }

        View getRefineView() {
            return mActionView;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mContentView = new View(mActivity);
        mView = new BaseSuggestionViewForTest(mContentView);

        mSuggestionPaddingEndPx = mActivity.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_view_modern_end_padding);
        mActionIconWidthPx = mActivity.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_width);

        mRefineView = mView.getRefineView();
        mDecoratedView = mView.getDecoratedView();
    }

    /**
     * Perform the measure and layout pass on the BaseSuggestionView.
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
        mContentView.setMinimumHeight(contentHeight);

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
        Assert.assertThat("view height", v.getMeasuredHeight(), lessThanOrEqualTo(bottom - top));
    }

    @Test
    public void layout_LtrRefineVisible() {
        final int useContentWidth = 120;

        // Expectations (edge to edge):
        //
        // +----------------+----+-+  ^
        // | CONTENT        |ACT1|#|  giveContentHeight
        // +----------------+----+-+  v
        //
        // <- giveSuggestionWidth ->
        //
        // where ACT is action button and # is final padding.

        final int giveSuggestionWidth =
                useContentWidth + mActionIconWidthPx + mSuggestionPaddingEndPx;
        final int giveContentHeight = 15;

        final int expectedContentLeft = 0;
        final int expectedContentRight = useContentWidth;
        final int expectedRefineLeft = expectedContentRight;
        final int expectedRefineRight = useContentWidth + mActionIconWidthPx;

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);

        verifyViewLayout(
                mRefineView, expectedRefineLeft, 0, expectedRefineRight, giveContentHeight);
        verifyViewLayout(
                mDecoratedView, expectedContentLeft, 0, expectedContentRight, giveContentHeight);
    }

    @Test
    public void layout_RtlRefineVisible() {
        final int useContentWidth = 120;

        // Expectations (edge to edge):
        //
        // +----+----------------+-+  ^
        // |ACT1| CONTENT        |#|  giveContentHeight
        // +----+----------------+-+  v
        //
        // <- giveSuggestionWidth ->
        //
        // where ACT is action button and # is final padding.

        final int giveSuggestionWidth =
                useContentWidth + mActionIconWidthPx + mSuggestionPaddingEndPx;
        final int giveContentHeight = 25;

        final int expectedRefineLeft = 0;
        final int expectedRefineRight = mActionIconWidthPx;
        final int expectedContentLeft = expectedRefineRight;
        final int expectedContentRight = giveSuggestionWidth - mSuggestionPaddingEndPx;

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);

        verifyViewLayout(
                mRefineView, expectedRefineLeft, 0, expectedRefineRight, giveContentHeight);
        verifyViewLayout(
                mDecoratedView, expectedContentLeft, 0, expectedContentRight, giveContentHeight);
    }

    @Test
    public void layout_LtrRefineInvisible() {
        // Expectations (edge to edge):
        //
        // +---------------------+-+  ^
        // |CONTENT              |#|  giveContentHeight
        // +---------------------+-+  v
        //
        // <- giveSuggestionWidth ->
        //
        // The reason for this is that we want content to align correctly with the end of the
        // omnibox field. Otherwise, content would end at the right screen edge.

        final int giveSuggestionWidth = 250;
        final int giveContentHeight = 15;

        final int expectedContentLeft = 0;
        final int expectedContentRight = giveSuggestionWidth - mSuggestionPaddingEndPx;

        mRefineView.setVisibility(View.GONE);

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_LTR);
        verifyViewLayout(
                mDecoratedView, expectedContentLeft, 0, expectedContentRight, giveContentHeight);
    }

    @Test
    public void layout_RtlRefineInvisible() {
        // Expectations (edge to edge):
        //
        // +---------------------+-+  ^
        // |CONTENT              |#|  giveContentHeight
        // +---------------------+-+  v
        //
        // <- giveSuggestionWidth ->
        //
        // The reason for this is that we want content to align correctly with the end of the
        // omnibox field. Otherwise, content would end (RTL) at the left screen edge.

        final int giveSuggestionWidth = 250;
        final int giveContentHeight = 15;

        final int expectedContentLeft = 0;
        final int expectedContentRight = giveSuggestionWidth - mSuggestionPaddingEndPx;

        mRefineView.setVisibility(View.GONE);

        executeLayoutTest(giveSuggestionWidth, giveContentHeight, View.LAYOUT_DIRECTION_RTL);
        verifyViewLayout(
                mDecoratedView, expectedContentLeft, 0, expectedContentRight, giveContentHeight);
    }
}
