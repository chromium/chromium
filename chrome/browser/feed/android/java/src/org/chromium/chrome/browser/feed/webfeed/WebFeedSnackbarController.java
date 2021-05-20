// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.content.Context;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Controller for showing Web Feed snackbars.
 */
public class WebFeedSnackbarController {
    /**
     * A helper interface for exposing a method to launch the feed.
     */
    @FunctionalInterface
    public interface FeedLauncher {
        void openFollowingFeed();
    }

    static final int SNACKBAR_DURATION_MS = (int) TimeUnit.SECONDS.toMillis(8);

    private final Context mContext;
    private final FeedLauncher mFeedLauncher;
    private final SnackbarManager mSnackbarManager;
    private final WebFeedDialogCoordinator mWebFeedDialogCoordinator;
    private final WebFeedBridge mWebFeedBridge;

    /**
     * Constructs an instance of {@link WebFeedSnackbarController}.
     *
     * @param context The {@link Context} to retrieve strings for the snackbars.
     * @param feedLauncher The {@link FeedLauncher} to launch the feed.
     * @param dialogManager {@link ModalDialogManager} for managing the dialog.
     * @param snackbarManager {@link SnackbarManager} to manage the snackbars.
     * @param webFeedBridge {@link WebFeedBridge} to connect with the backend to follow/unfollow.
     */
    WebFeedSnackbarController(Context context, FeedLauncher feedLauncher,
            ModalDialogManager dialogManager, SnackbarManager snackbarManager,
            WebFeedBridge webFeedBridge) {
        mContext = context;
        mFeedLauncher = feedLauncher;
        mSnackbarManager = snackbarManager;
        mWebFeedDialogCoordinator = new WebFeedDialogCoordinator(dialogManager);
        mWebFeedBridge = webFeedBridge;
    }

    /**
     * Show appropriate post-follow snackbar/dialog depending on success/failure.
     */
    void showPostFollowHelp(
            WebFeedBridge.FollowResults results, byte[] followId, GURL url, String fallbackTitle) {
        if (results.requestStatus == WebFeedSubscriptionRequestStatus.SUCCESS) {
            if (results.metadata != null) {
                showPostSuccessfulFollowHelp(results.metadata.title, results.metadata.isActive);
            } else {
                showPostSuccessfulFollowHelp(fallbackTitle, /*isActive=*/false);
            }
        } else {
            // TODO(crbug/1152592): Add snackbars for specific failures.
            // Show follow failure snackbar.
            showSnackbar(
                    mContext.getString(R.string.web_feed_follow_generic_failure_snackbar_message),
                    getFollowActionSnackbarController(followId, url, fallbackTitle),
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
            showUnfollowSuccessSnackbar(followId, url, title);
        } else {
            showUnfollowFailureSnackbar(followId, url, title);
        }
    }

    private void showPostSuccessfulFollowHelp(String title, boolean isActive) {
        if (TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile())
                        .shouldTriggerHelpUI(
                                FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE)) {
            mWebFeedDialogCoordinator.initialize(mContext, mFeedLauncher, title, isActive);
            mWebFeedDialogCoordinator.showDialog();
        } else {
            SnackbarController snackbarController = new SnackbarController() {
                @Override
                public void onAction(Object actionData) {
                    mFeedLauncher.openFollowingFeed();
                }
            };
            showSnackbar(
                    mContext.getString(R.string.web_feed_follow_success_snackbar_message, title),
                    snackbarController, Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS,
                    R.string.web_feed_follow_success_snackbar_action);
        }
    }

    private void showUnfollowSuccessSnackbar(byte[] followId, GURL url, String title) {
        showSnackbar(mContext.getString(R.string.web_feed_unfollow_success_snackbar_message, title),
                getFollowActionSnackbarController(followId, url, title),
                Snackbar.UMA_WEB_FEED_UNFOLLOW_SUCCESS,
                R.string.web_feed_unfollow_success_snackbar_action);
    }

    private void showUnfollowFailureSnackbar(byte[] followId, GURL url, String title) {
        SnackbarController snackbarController = new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                mWebFeedBridge.unfollow(followId, result -> {
                    if (result.requestStatus == WebFeedSubscriptionRequestStatus.SUCCESS) {
                        showUnfollowSuccessSnackbar(followId, url, title);
                    } else {
                        showUnfollowFailureSnackbar(followId, url, title);
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
                        .setSingleLine(false)
                        .setDuration(SNACKBAR_DURATION_MS);
        mSnackbarManager.showSnackbar(snackbar);
    }

    /**
     * Returns {@link SnackbarController} for when the snackbar action is to follow. Prefers
     * {@link WebFeedBridge#followFromId} if an ID is available.
     */
    private SnackbarController getFollowActionSnackbarController(
            byte[] followId, GURL url, String title) {
        return new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                if (followId == null || followId.length == 0) {
                    // TODO(harringtond): How will we get the RSS information for this call?
                    mWebFeedBridge.followFromUrl(/*tab=*/null, url, result -> {
                        byte[] followId = result.metadata != null ? result.metadata.id : null;
                        showPostFollowHelp(result, followId, url, title);
                    });
                } else {
                    mWebFeedBridge.followFromId(
                            followId, result -> showPostFollowHelp(result, followId, url, title));
                }
            }
        };
    }
}
