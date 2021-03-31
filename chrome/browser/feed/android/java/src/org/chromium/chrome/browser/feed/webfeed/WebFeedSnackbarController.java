// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.content.Context;

import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.url.GURL;

/**
 * Controller for showing Web Feed snackbars.
 */
class WebFeedSnackbarController {
    private final Context mContext;
    private final SnackbarManager mSnackbarManager;
    private final WebFeedBridge mWebFeedBridge;

    /**
     * Constructs an instance of {@link WebFeedSnackbarController}.
     *
     * @param context The {@link Context} to retrieve strings for the snackbars.
     * @param snackbarManager {@link SnackbarManager} to manage the snackbars.
     * @param webFeedBridge {@link WebFeedBridge} to connect with the backend to follow/unfollow.
     */
    WebFeedSnackbarController(
            Context context, SnackbarManager snackbarManager, WebFeedBridge webFeedBridge) {
        mContext = context;
        mSnackbarManager = snackbarManager;
        mWebFeedBridge = webFeedBridge;
    }

    /**
     * Show appropriate post-follow snackbar depending on success/failure.
     */
    void showSnackbarForFollow(WebFeedBridge.FollowResults results, GURL url, String title) {
        if (results.requestStatus == WebFeedSubscriptionRequestStatus.SUCCESS) {
            if (results.metadata != null) {
                showFollowSuccessSnackbar(results.metadata.title);
            } else {
                showFollowSuccessSnackbar(title);
            }
        } else {
            // TODO(crbug/1152592): Add snackbars for specific failures.
            // Show follow failure snackbar.
            showSnackbar(
                    mContext.getString(R.string.web_feed_follow_generic_failure_snackbar_message),
                    getFollowActionSnackbarController(url, title),
                    Snackbar.UMA_WEB_FEED_FOLLOW_FAILURE,
                    R.string.web_feed_generic_failure_snackbar_action);
        }
    }

    /**
     * Show appropriate post-unfollow snackbar depending on success/failure.
     */
    void showSnackbarForUnfollow(
            boolean successfulUnfollow, byte[] followId, GURL url, String title) {
        if (successfulUnfollow) {
            showUnfollowSuccessSnackbar(url, title);
        } else {
            showUnfollowFailureSnackbar(url, followId, title);
        }
    }

    private void showFollowSuccessSnackbar(String title) {
        SnackbarController snackbarController = new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                // TODO(crbug/1152592): Implement go to feed.
            }
        };
        showSnackbar(mContext.getString(R.string.web_feed_follow_success_snackbar_message, title),
                snackbarController, Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS,
                R.string.web_feed_follow_success_snackbar_action);
    }

    private void showUnfollowSuccessSnackbar(GURL url, String title) {
        showSnackbar(mContext.getString(R.string.web_feed_unfollow_success_snackbar_message, title),
                getFollowActionSnackbarController(url, title),
                Snackbar.UMA_WEB_FEED_UNFOLLOW_SUCCESS,
                R.string.web_feed_unfollow_success_snackbar_action);
    }

    private void showUnfollowFailureSnackbar(GURL url, byte[] followId, String title) {
        SnackbarController snackbarController = new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                mWebFeedBridge.unfollow(followId, result -> {
                    if (result.requestStatus == WebFeedSubscriptionRequestStatus.SUCCESS) {
                        showUnfollowSuccessSnackbar(url, title);
                    } else {
                        showUnfollowFailureSnackbar(url, followId, title);
                    }
                });
            }
        };
        showSnackbar(
                mContext.getString(R.string.web_feed_unfollow_generic_failure_snackbar_message),
                snackbarController, Snackbar.UMA_WEB_FEED_UNFOLLOW_FAILURE,
                R.string.web_feed_generic_failure_snackbar_action);
    }

    private void showSnackbar(String message, SnackbarController snackbarController, int umaId,
            int snackbarActionId) {
        Snackbar snackbar =
                Snackbar.make(message, snackbarController, Snackbar.TYPE_ACTION, umaId)
                        .setAction(mContext.getString(snackbarActionId), /*actionData=*/null)
                        .setSingleLine(false);
        mSnackbarManager.showSnackbar(snackbar);
    }

    /**
     * Returns {@link SnackbarController} for when the snackbar action is to follow.
     */
    private SnackbarController getFollowActionSnackbarController(GURL url, String title) {
        return new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                mWebFeedBridge.followFromUrl(
                        url, result -> { showSnackbarForFollow(result, url, title); });
            }
        };
    }
}
