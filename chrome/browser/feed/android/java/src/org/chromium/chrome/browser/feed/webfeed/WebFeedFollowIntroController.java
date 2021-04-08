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
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

/**
 * Controls when and how the Web Feed follow intro is shown.
 */
public class WebFeedFollowIntroController {
    private final Activity mActivity;
    private final CurrentTabObserver mCurrentTabObserver;
    private final WebFeedSnackbarController mWebFeedSnackbarController;
    private final WebFeedFollowIntroView mWebFeedFollowIntroView;

    private boolean mAcceleratorPressed;

    /**
     * Constructs an instance of {@link WebFeedFollowIntroController}.
     *
     * @param activity The current {@link Activity}.
     * @param tabSupplier The supplier for the currently active {@link Tab}.
     * @param menuButtonAnchorView The menu button {@link View} to serve as an anchor.
     * @param snackbarManager The {@link SnackbarManager} to show snackbars.
     * @param webFeedBridge The {@link WebFeedBridge} to connect to the Web Feed backend.
     */
    public WebFeedFollowIntroController(Activity activity, ObservableSupplier<Tab> tabSupplier,
            View menuButtonAnchorView, SnackbarManager snackbarManager,
            WebFeedBridge webFeedBridge) {
        mActivity = activity;
        mWebFeedSnackbarController =
                new WebFeedSnackbarController(activity, snackbarManager, webFeedBridge);
        mWebFeedFollowIntroView = new WebFeedFollowIntroView(mActivity, menuButtonAnchorView);

        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                // TODO(sophey): Add proper heuristics for showing the accelerator.
                mAcceleratorPressed = false;
                webFeedBridge.getWebFeedMetadataForPage(url, result -> {
                    // Shouldn't be recommended if there's no metadata.
                    if (result != null) {
                        maybeShowFollowIntro(url, result.title);
                    }
                });
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

    private void maybeShowFollowIntro(GURL url, String title) {
        if (!shouldShowFollowIntro()) {
            return;
        }
        // TODO(crbug/1152592): Add IPH variation.
        showFollowAccelerator(url, title);
    }

    private void showFollowAccelerator(GURL url, String title) {
        GestureDetector gestureDetector = new GestureDetector(
                mActivity.getApplicationContext(), new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public boolean onSingleTapUp(MotionEvent motionEvent) {
                        if (!mAcceleratorPressed) {
                            mAcceleratorPressed = true;
                            performFollowWithAccelerator(url, title);
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

    private void performFollowWithAccelerator(GURL url, String title) {
        mWebFeedFollowIntroView.showLoadingUI();
        WebFeedBridge bridge = new WebFeedBridge();
        bridge.followFromUrl(
                url, results -> mWebFeedFollowIntroView.hideLoadingUI(new LoadingView.Observer() {
                    @Override
                    public void onShowLoadingUIComplete() {}

                    @Override
                    public void onHideLoadingUIComplete() {
                        mWebFeedFollowIntroView.dismissBubble();
                        if (results.requestStatus == WebFeedSubscriptionRequestStatus.SUCCESS) {
                            mWebFeedFollowIntroView.showFollowingBubble();
                        }
                        mWebFeedSnackbarController.showSnackbarForFollow(results, url, title);
                    }
                }));
    }

    private boolean shouldShowFollowIntro() {
        return true;
    }
}
