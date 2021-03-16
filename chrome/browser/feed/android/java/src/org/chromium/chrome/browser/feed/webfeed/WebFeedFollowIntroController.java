// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.app.Activity;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.LoadingView;
import org.chromium.url.GURL;

/**
 * Controls when and how the Web Feed follow intro is shown.
 */
public class WebFeedFollowIntroController {
    private final Activity mActivity;
    private final CurrentTabObserver mCurrentTabObserver;
    private final WebFeedFollowIntroView mWebFeedFollowIntroView;

    private boolean mAcceleratorPressed;

    /**
     * Constructs an instance of {@link WebFeedFollowIntroController}.
     *
     * @param activity The current {@link Activity}.
     * @param tabSupplier The supplier for the currently active {@link Tab}.
     * @param menuButtonAnchorView The menu button {@link View} to serve as an anchor.
     */
    public WebFeedFollowIntroController(
            Activity activity, ObservableSupplier<Tab> tabSupplier, View menuButtonAnchorView) {
        mActivity = activity;
        mWebFeedFollowIntroView = new WebFeedFollowIntroView(mActivity, menuButtonAnchorView);

        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                // TODO(sophey): Add proper heuristics for showing the accelerator.
                mAcceleratorPressed = false;
                maybeShowFollowIntro(url);
            }

            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                mWebFeedFollowIntroView.dismissBubble();
            }
        }, /*swapCallback=*/null);
    }

    public void destroy() {
        mCurrentTabObserver.destroy();
    }

    private void maybeShowFollowIntro(GURL url) {
        if (!shouldShowFollowIntro()) {
            return;
        }
        // TODO(crbug/1152592): Add IPH variation.
        showFollowAccelerator(url);
    }

    private void showFollowAccelerator(GURL url) {
        GestureDetector gestureDetector = new GestureDetector(
                mActivity.getApplicationContext(), new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public boolean onSingleTapUp(MotionEvent motionEvent) {
                        if (!mAcceleratorPressed) {
                            mAcceleratorPressed = true;
                            performFollowWithAccelerator(url);
                        }
                        return true;
                    }
                });
        View.OnTouchListener onTouchListener = (view, motionEvent) -> {
            view.performClick();
            gestureDetector.onTouchEvent(motionEvent);
            return true;
        };
        mWebFeedFollowIntroView.showAccelerator(onTouchListener);
    }

    private void performFollowWithAccelerator(GURL url) {
        mWebFeedFollowIntroView.showLoadingUI();
        WebFeedBridge bridge = new WebFeedBridge();
        bridge.followFromUrl(
                url, result -> mWebFeedFollowIntroView.hideLoadingUI(new LoadingView.Observer() {
                    @Override
                    public void onShowLoadingUIComplete() {}

                    @Override
                    public void onHideLoadingUIComplete() {
                        mWebFeedFollowIntroView.dismissBubble();
                        if (result.success) {
                            mWebFeedFollowIntroView.showFollowingBubble();
                        }
                        // TODO(crbug/1152592): Add snackbar for failure.
                    }
                }));
    }

    private boolean shouldShowFollowIntro() {
        return true;
    }
}