// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.chrome.browser.feed.ScrollListener.ScrollState;

/**
 * Creates a ScrollListener that triggers the "Back to top" callout bubble.
 *
 * The "Back to top" bubble will only appear when the user reaches end of the feed or pass a
 * desired number of cards and scroll upward a few card length.
 *
 */
public class BackToTopBubbleScrollListener implements ScrollListener {
    /**
     * Handles how the bubble should be shown or dismissed.
     * */
    public interface ResultHandler {
        /**
         * Shows the bubble.
         * */
        void showBubble();

        /**
         * Dismisses the bubble.
         * */
        void dismissBubble();
    }

    // The feed position that the user needs to scroll down to before the next step
    static final int FEED_CARD_POSITION_SCROLLED_DOWN = 30;
    // The number of feed cards that the user needs to scroll up before the "Back to top" prompt can
    // be triggered.
    static final int NUMBER_OF_FEED_CARDS_SCROLLED_UPWARD = 1;

    private final FeedBubbleDelegate mDelegate;
    private final ResultHandler mResultHandler;

    private int mTriggeredPosition;

    /**
     * Constructor for "Back to top" callout triggering.
     */
    public BackToTopBubbleScrollListener(FeedBubbleDelegate delegate, ResultHandler resultHandler) {
        mDelegate = delegate;
        mResultHandler = resultHandler;
    }

    @Override
    public void onScrollStateChanged(@ScrollState int state) {}

    @Override
    public void onScrolled(int dx, int dy) {
        performCheck();
    }

    @Override
    public void onHeaderOffsetChanged(int verticalOffset) {
        performCheck();
    }

    private void performCheck() {
        if (!mDelegate.isFeedExpanded()) return;
        if (mDelegate.isShowingBackToTopBubble()) {
            maybeDismissBubble();
        } else {
            maybeShowBubble();
        }
    }

    private void maybeShowBubble() {
        if (mTriggeredPosition > 0) {
            if (mDelegate.getLastVisiblePosition()
                    > mTriggeredPosition - NUMBER_OF_FEED_CARDS_SCROLLED_UPWARD + 1) {
                return;
            }
            mTriggeredPosition = 0;
            mResultHandler.showBubble();
        } else {
            if (mDelegate.getLastVisiblePosition()
                            < FEED_CARD_POSITION_SCROLLED_DOWN + mDelegate.getHeaderCount() - 1
                    && mDelegate.getLastVisiblePosition() != mDelegate.getItemCount() - 1) {
                return;
            }
            mTriggeredPosition = mDelegate.getLastVisiblePosition();
        }
    }

    private void maybeDismissBubble() {
        if (mDelegate.getFirstVisiblePosition() < mDelegate.getHeaderCount()) {
            mResultHandler.dismissBubble();
        }
    }
}
