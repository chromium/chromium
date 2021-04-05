// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Handler;
import android.view.View;

import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.textbubble.ClickableTextBubble;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Manages the view of the WebFeed follow intro.
 */
class WebFeedFollowIntroView {
    private static final int sAcceleratorTimeout = 10 * 1000; // 10 seconds

    private final Activity mActivity;
    private final Handler mHandler = new Handler();
    private final View mMenuButtonAnchorView;

    private ClickableTextBubble mFollowBubble;

    /**
     * Constructs an instance of {@link WebFeedFollowIntroView}.
     *
     * @param activity The current {@link Activity}.
     * @param menuButtonAnchorView The menu button {@link View} to serve as an anchor.
     */
    WebFeedFollowIntroView(Activity activity, View menuButtonAnchorView) {
        mActivity = activity;
        mMenuButtonAnchorView = menuButtonAnchorView;
    }

    void showAccelerator(View.OnTouchListener onTouchListener) {
        mFollowBubble = new ClickableTextBubble(mActivity, mMenuButtonAnchorView,
                R.string.menu_follow, R.string.menu_follow, createRectProvider(), R.drawable.ic_add,
                ChromeAccessibilityUtil.get().isAccessibilityEnabled(), onTouchListener);
        mFollowBubble.addOnDismissListener(
                ()
                        -> mHandler.postDelayed(this::turnOffHighlightForFollowMenuItem,
                                ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS));
        // TODO(crbug/1152592): Figure out a way to dismiss on outside taps as well.
        mFollowBubble.setAutoDismissTimeout(sAcceleratorTimeout);
        turnOnHighlightForFollowMenuItem();

        mFollowBubble.show();
    }

    void showLoadingUI() {
        if (mFollowBubble != null) {
            mFollowBubble.showLoadingUI(R.string.web_feed_follow_loading_description);
        }
    }

    void hideLoadingUI(LoadingView.Observer loadingViewObserver) {
        if (mFollowBubble != null) {
            mFollowBubble.hideLoadingUI(loadingViewObserver);
        }
    }

    void dismissBubble() {
        if (mFollowBubble != null) {
            mFollowBubble.dismiss();
            mFollowBubble.destroy();
            mFollowBubble = null;
        }
    }

    void showFollowingBubble() {
        TextBubble followingBubble = new TextBubble(mActivity, mMenuButtonAnchorView,
                R.string.menu_following, R.string.menu_following, /*showArrow=*/false,
                createRectProvider(), R.drawable.ic_done_blue, /*isRoundBubble=*/true,
                /*inverseColor=*/true, ChromeAccessibilityUtil.get().isAccessibilityEnabled());
        followingBubble.setDismissOnTouchInteraction(true);
        followingBubble.show();
    }

    private ViewRectProvider createRectProvider() {
        ViewRectProvider rectProvider = new ViewRectProvider(mMenuButtonAnchorView);
        int yInsetPx = mActivity.getResources().getDimensionPixelOffset(
                R.dimen.iph_text_bubble_menu_anchor_y_inset);
        Rect insetRect = new Rect(0, 0, 0, yInsetPx);
        rectProvider.setInsetPx(insetRect);

        return rectProvider;
    }

    private void turnOnHighlightForFollowMenuItem() {
        // TODO(crbug/1152592): Figure out how to highlight footer.
    }

    private void turnOffHighlightForFollowMenuItem() {}
}
