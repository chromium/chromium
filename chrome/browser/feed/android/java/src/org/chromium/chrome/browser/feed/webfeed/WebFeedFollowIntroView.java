// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Handler;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Manages the view of the WebFeed follow intro.
 *
 * This is the chip that shows up under the 3-dot menu informing users that this is a page
 * they can follow.
 */
class WebFeedFollowIntroView {
    private static final int DEFAULT_SHOW_TIMEOUT_MILLIS = 8 * 1000;

    private final Activity mActivity;
    private final AppMenuHandler mAppMenuHandler;
    private final Handler mHandler = new Handler();
    private final View mMenuButtonAnchorView;
    @Nullable private final Tracker mFeatureEngagementTracker;
    private final Runnable mIntroDismissedCallback;

    private ClickableTextBubble mFollowBubble;
    private final int mShowTimeoutMillis;

    /**
     * Constructs an instance of {@link WebFeedFollowIntroView}.
     *
     * @param activity The current {@link Activity}.
     * @param appMenuHandler The {@link AppMenuHandler} to highlight the Web Feed menu item.
     * @param menuButtonAnchorView The menu button {@link View} to serve as an anchor.
     */
    WebFeedFollowIntroView(
            Activity activity,
            AppMenuHandler appMenuHandler,
            View menuButtonAnchorView,
            @Nullable Tracker featureEngagementTracker,
            Runnable introDismissedCallback) {
        mActivity = activity;
        mAppMenuHandler = appMenuHandler;
        mMenuButtonAnchorView = menuButtonAnchorView;
        mFeatureEngagementTracker = featureEngagementTracker;
        mIntroDismissedCallback = introDismissedCallback;

        mShowTimeoutMillis = DEFAULT_SHOW_TIMEOUT_MILLIS;
    }

    void showAccelerator(
            View.OnTouchListener onTouchListener,
            Runnable introShownCallback,
            Runnable introNotShownCallback) {
        if (mFeatureEngagementTracker != null
                && !mFeatureEngagementTracker.shouldTriggerHelpUI(
                        FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE)) {
            introNotShownCallback.run();
            return;
        }

        mFollowBubble =
                new ClickableTextBubble(
                        mActivity,
                        mMenuButtonAnchorView,
                        R.string.menu_follow,
                        R.string.menu_follow,
                        createRectProvider(),
                        R.drawable.ic_add,
                        ChromeAccessibilityUtil.get().isAccessibilityEnabled(),
                        onTouchListener,
                        /* inverseColor= */ false);
        mFollowBubble.addOnDismissListener(this::introDismissed);
        // TODO(crbug.com/40158714): Figure out a way to dismiss on outside taps as well.
        mFollowBubble.setAutoDismissTimeout(mShowTimeoutMillis);
        turnOnHighlightForFollowMenuItem();

        mFollowBubble.show();
        introShownCallback.run();
    }

    void showIPH(
            UserEducationHelper helper,
            Runnable introShownCallback,
            Runnable introNotShownCallback) {
        int iphStringResource = R.string.follow_accelerator;
        int iphAccessibilityStringResource = R.string.accessibility_follow_accelerator_iph;

        // Make the request to show the IPH.
        helper.requestShowIPH(
                new IPHCommandBuilder(
                                mMenuButtonAnchorView.getContext().getResources(),
                                FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE,
                                iphStringResource,
                                iphAccessibilityStringResource)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setDismissOnTouch(false)
                        .setAutoDismissTimeout(mShowTimeoutMillis)
                        .setOnShowCallback(
                                () -> {
                                    turnOnHighlightForFollowMenuItem();
                                    introShownCallback.run();
                                })
                        .setOnNotShownCallback(introNotShownCallback)
                        .setOnDismissCallback(this::introDismissed)
                        .build());
    }

    private void introDismissed() {
        mHandler.postDelayed(
                this::turnOffHighlightForFollowMenuItem,
                ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS);
        mIntroDismissedCallback.run();
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
        TextBubble followingBubble =
                new ClickableTextBubble(
                        mActivity,
                        mMenuButtonAnchorView,
                        R.string.menu_following,
                        R.string.menu_following,
                        createRectProvider(),
                        R.drawable.ic_done_blue,
                        ChromeAccessibilityUtil.get().isAccessibilityEnabled(),
                        /* onTouchListener= */ null,
                        /* inverseColor= */ false);
        followingBubble.setDismissOnTouchInteraction(true);
        followingBubble.show();
    }

    private ViewRectProvider createRectProvider() {
        ViewRectProvider rectProvider = new ViewRectProvider(mMenuButtonAnchorView);
        int yInsetPx =
                mActivity.getResources().getDimensionPixelOffset(R.dimen.web_feed_intro_y_inset);
        Rect insetRect = new Rect(0, 0, 0, yInsetPx);
        rectProvider.setInsetPx(insetRect);

        return rectProvider;
    }

    private void turnOnHighlightForFollowMenuItem() {
        mAppMenuHandler.setMenuHighlight(R.id.follow_chip_view);
    }

    private void turnOffHighlightForFollowMenuItem() {
        mAppMenuHandler.clearMenuHighlight();
    }

    boolean wasFollowBubbleShownForTesting() {
        return mFollowBubble != null;
    }
}
