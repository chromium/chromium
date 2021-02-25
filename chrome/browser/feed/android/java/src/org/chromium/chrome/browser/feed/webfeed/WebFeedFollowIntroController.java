// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Handler;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.textbubble.ClickableTextBubble;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.url.GURL;

/**
 * Controls when and how the Web Feed follow intro is shown.
 */
public class WebFeedFollowIntroController {
    private final Activity mActivity;
    private final AppMenuHandler mAppMenuHandler;
    private final Handler mHandler = new Handler();
    private final View mMenuButtonAnchorView;

    private CurrentTabObserver mCurrentTabObserver;

    /**
     * Constructs an instance of {@link WebFeedFollowIntroController}.
     *
     * @param activity The current {@link Activity}.
     * @param appMenuHandler The {@link AppMenuHandler} to control menu item highlights.
     * @param tabSupplier The supplier for the currently active {@link Tab}.
     * @param menuButtonAnchorView The menu button {@link View} to serve as an anchor.
     */
    public WebFeedFollowIntroController(Activity activity, AppMenuHandler appMenuHandler,
            ObservableSupplier<Tab> tabSupplier, View menuButtonAnchorView) {
        mActivity = activity;
        mAppMenuHandler = appMenuHandler;
        mMenuButtonAnchorView = menuButtonAnchorView;

        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                // TODO(sophey): Add proper heuristics for showing the accelerator. Also add IPH
                // variation.
                maybeShowFollowAccelerator();
            }
        }, /*swapCallback=*/null);
    }

    public void destroy() {
        mCurrentTabObserver.destroy();
    }

    private void maybeShowFollowAccelerator() {
        if (!shouldShowFollowAccelerator()) {
            return;
        }

        ViewRectProvider rectProvider = new ViewRectProvider(mMenuButtonAnchorView);
        int yInsetPx = mActivity.getResources().getDimensionPixelOffset(
                R.dimen.iph_text_bubble_menu_anchor_y_inset);
        Rect insetRect = new Rect(0, 0, 0, yInsetPx);
        rectProvider.setInsetPx(insetRect);

        View.OnTouchListener onTouchListener = new View.OnTouchListener() {
            @Override
            public boolean onTouch(View view, MotionEvent motionEvent) {
                // TODO(sophey): Hook up follow functionality and implement post-follow animation.
                view.performClick();
                return true;
            }
        };

        ClickableTextBubble textBubble = new ClickableTextBubble(mActivity, mMenuButtonAnchorView,
                R.string.menu_follow, R.string.menu_follow, rectProvider, R.drawable.ic_add,
                ChromeAccessibilityUtil.get().isAccessibilityEnabled(), onTouchListener);
        // TODO(sophey): Remove once onClick functionality is implemented.
        textBubble.setDismissOnTouchInteraction(true);
        textBubble.addOnDismissListener(() -> mHandler.postDelayed(() -> {
            turnOffHighlightForFollowMenuItem();
        }, ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS));
        turnOnHighlightForFollowMenuItem();

        textBubble.show();
    }

    private boolean shouldShowFollowAccelerator() {
        return false;
    }

    private void turnOnHighlightForFollowMenuItem() {
        mAppMenuHandler.setMenuHighlight(R.id.feed_follow_id);
    }

    private void turnOffHighlightForFollowMenuItem() {
        mAppMenuHandler.clearMenuHighlight();
    }
}