// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link BackToTopBubbleScrollListener}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class BackToTopBubbleScrollListenerTest
        implements FeedBubbleDelegate, BackToTopBubbleScrollListener.ResultHandler {
    private boolean mIsFeedExpanded;
    private boolean mIsShowingBackToTopBubble;
    private int mHeaderCount;
    private int mItemCount;
    private int mFirstVisiblePosition;
    private int mLastVisiblePosition;
    private boolean mShowRunnableCalled;
    private boolean mDismissRunnableCalled;

    @Override
    public Tracker getFeatureEngagementTracker() {
        return null;
    }

    @Override
    public boolean isFeedExpanded() {
        return mIsFeedExpanded;
    }

    @Override
    public boolean isSignedIn() {
        return false;
    }

    @Override
    public boolean isFeedHeaderPositionInContainerSuitableForIPH(float headerMaxPosFraction) {
        return false;
    }

    @Override
    public long getCurrentTimeMs() {
        return 0;
    }

    @Override
    public long getLastFetchTimeMs() {
        return 0;
    }

    @Override
    public boolean canScrollUp() {
        return false;
    }

    @Override
    public boolean isShowingBackToTopBubble() {
        return mIsShowingBackToTopBubble;
    }

    @Override
    public int getHeaderCount() {
        return mHeaderCount;
    }

    @Override
    public int getItemCount() {
        return mItemCount;
    }

    @Override
    public int getFirstVisiblePosition() {
        return mFirstVisiblePosition;
    }

    @Override
    public int getLastVisiblePosition() {
        return mLastVisiblePosition;
    }

    @Override
    public void showBubble() {
        mShowRunnableCalled = true;
    }

    @Override
    public void dismissBubble() {
        mDismissRunnableCalled = true;
    }

    @Before
    public void setUp() {}

    @Test
    @Feature({"Feed"})
    public void testFeedNotExpanded() {
        mIsFeedExpanded = false;
        mIsShowingBackToTopBubble = false;
        mHeaderCount = 2;
        mItemCount = 2;
        mFirstVisiblePosition = 0;
        mLastVisiblePosition = 1;

        mShowRunnableCalled = false;
        mDismissRunnableCalled = false;
        BackToTopBubbleScrollListener listener = new BackToTopBubbleScrollListener(this, this);
        listener.onScrolled(0, 5);

        Assert.assertFalse(mShowRunnableCalled);
        Assert.assertFalse(mDismissRunnableCalled);
    }

    @Test
    @Feature({"Feed"})
    public void testNotReachingEndOfFeed() {
        mIsFeedExpanded = true;
        mIsShowingBackToTopBubble = false;
        mHeaderCount = 2;
        mItemCount = 10;

        mShowRunnableCalled = false;
        mDismissRunnableCalled = false;
        BackToTopBubbleScrollListener listener = new BackToTopBubbleScrollListener(this, this);

        // Scroll down but not reaching the end of the feed.
        mFirstVisiblePosition = 6;
        mLastVisiblePosition = 8;
        listener.onScrolled(0, 5);

        // The bubble is not triggered.
        Assert.assertFalse(mShowRunnableCalled);
        Assert.assertFalse(mDismissRunnableCalled);

        // Scroll up.
        mFirstVisiblePosition = 4;
        mLastVisiblePosition = 6;
        listener.onScrolled(0, 5);

        // The bubble is not triggered.
        Assert.assertFalse(mShowRunnableCalled);
        Assert.assertFalse(mDismissRunnableCalled);
    }

    @Test
    @Feature({"Feed"})
    public void testReachingEndOfFeed() {
        mIsFeedExpanded = true;
        mIsShowingBackToTopBubble = false;
        mHeaderCount = 2;
        mItemCount = 10;

        mShowRunnableCalled = false;
        mDismissRunnableCalled = false;
        BackToTopBubbleScrollListener listener = new BackToTopBubbleScrollListener(this, this);

        // Scroll down but not reaching the end of the feed.
        mFirstVisiblePosition = 7;
        mLastVisiblePosition = 9;
        listener.onScrolled(0, 5);

        // The bubble is not triggered.
        Assert.assertFalse(mShowRunnableCalled);
        Assert.assertFalse(mDismissRunnableCalled);

        // Scroll up.
        mFirstVisiblePosition = 4;
        mLastVisiblePosition = 6;
        listener.onScrolled(0, 5);

        // The bubble is triggered.
        Assert.assertTrue(mShowRunnableCalled);
        Assert.assertFalse(mDismissRunnableCalled);

        // Scroll up to the top.
        mIsShowingBackToTopBubble = true;
        mShowRunnableCalled = false;
        mFirstVisiblePosition = 0;
        mLastVisiblePosition = 2;
        listener.onScrolled(0, 5);

        // The bubble should be dismissed.
        Assert.assertFalse(mShowRunnableCalled);
        Assert.assertTrue(mDismissRunnableCalled);
    }

    @Test
    @Feature({"Feed"})
    public void testNotPassingRequiredNumberOfFeedCards() {
        mIsFeedExpanded = true;
        mIsShowingBackToTopBubble = false;
        mHeaderCount = 2;
        mItemCount = 52;

        mShowRunnableCalled = false;
        mDismissRunnableCalled = false;
        BackToTopBubbleScrollListener listener = new BackToTopBubbleScrollListener(this, this);

        // Scroll down but not passing the required number of feed cards.
        mLastVisiblePosition =
                mHeaderCount + BackToTopBubbleScrollListener.FEED_CARD_POSITION_SCROLLED_DOWN - 2;
        mFirstVisiblePosition = mLastVisiblePosition - 2;
        listener.onScrolled(0, 5);

        // The bubble is not triggered.
        Assert.assertFalse(mShowRunnableCalled);
        Assert.assertFalse(mDismissRunnableCalled);

        // Scroll up.
        mFirstVisiblePosition -= 2;
        mLastVisiblePosition -= 2;
        listener.onScrolled(0, 5);

        // The bubble is not triggered.
        Assert.assertFalse(mShowRunnableCalled);
        Assert.assertFalse(mDismissRunnableCalled);
    }

    @Test
    @Feature({"Feed"})
    public void testPassingRequiredNumberOfFeedCards() {
        mIsFeedExpanded = true;
        mIsShowingBackToTopBubble = false;
        mHeaderCount = 2;
        mItemCount = 52;

        mShowRunnableCalled = false;
        mDismissRunnableCalled = false;
        BackToTopBubbleScrollListener listener = new BackToTopBubbleScrollListener(this, this);

        // Scroll down but not passing the required number of feed cards.
        mLastVisiblePosition =
                mHeaderCount + BackToTopBubbleScrollListener.FEED_CARD_POSITION_SCROLLED_DOWN;
        mFirstVisiblePosition = mLastVisiblePosition - 2;
        listener.onScrolled(0, 5);

        // The bubble is not triggered.
        Assert.assertFalse(mShowRunnableCalled);
        Assert.assertFalse(mDismissRunnableCalled);

        // Scroll up.
        mFirstVisiblePosition -= 2;
        mLastVisiblePosition -= 2;
        listener.onScrolled(0, 5);

        // The bubble is triggered.
        Assert.assertTrue(mShowRunnableCalled);
        Assert.assertFalse(mDismissRunnableCalled);

        // Scroll up to the top.
        mIsShowingBackToTopBubble = true;
        mShowRunnableCalled = false;
        mFirstVisiblePosition = 0;
        mLastVisiblePosition = 2;
        listener.onScrolled(0, 5);

        // The bubble should be dismissed.
        Assert.assertFalse(mShowRunnableCalled);
        Assert.assertTrue(mDismissRunnableCalled);
    }
}
