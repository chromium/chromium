// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.content.Context;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedSurfaceTracker;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
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

    /**
     * Constructs an instance of {@link WebFeedSnackbarController}.
     *
     * @param context The {@link Context} to retrieve strings for the snackbars.
     * @param feedLauncher The {@link FeedLauncher} to launch the feed.
     * @param dialogManager {@link ModalDialogManager} for managing the dialog.
     * @param snackbarManager {@link SnackbarManager} to manage the snackbars.
     */
    WebFeedSnackbarController(Context context, FeedLauncher feedLauncher,
            ModalDialogManager dialogManager, SnackbarManager snackbarManager) {
        mContext = context;
        mFeedLauncher = feedLauncher;
        mSnackbarManager = snackbarManager;
        mWebFeedDialogCoordinator = new WebFeedDialogCoordinator(dialogManager);
    }

    /**
     * Show appropriate post-follow snackbar/dialog depending on success/failure.
     */
    void showPostFollowHelp(Tab tab, WebFeedBridge.FollowResults results, byte[] followId, GURL url,
            String fallbackTitle, int webFeedChangeReason) {
        if (results.requestStatus == WebFeedSubscriptionRequestStatus.SUCCESS) {
            if (results.metadata != null) {
                showPostSuccessfulFollowHelp(results.metadata.title,
                        results.metadata.availabilityStatus == WebFeedAvailabilityStatus.ACTIVE);
            } else {
                showPostSuccessfulFollowHelp(fallbackTitle, /*isActive=*/false);
            }
        } else {
            int failureMessage = R.string.web_feed_follow_generic_failure_snackbar_message;
            if (results.requestStatus == WebFeedSubscriptionRequestStatus.FAILED_OFFLINE) {
                failureMessage = R.string.web_feed_offline_failure_snackbar_message;
            }

            // Show follow failure snackbar.
            FollowActionSnackbarController snackbarController = new FollowActionSnackbarController(
                    followId, url, fallbackTitle,
                    FeedUserActionType.TAPPED_FOLLOW_TRY_AGAIN_ON_SNACKBAR, webFeedChangeReason);
            int actionStringId = 0;
            if (canRetryFollow(tab, followId, url)) {
                actionStringId = R.string.web_feed_generic_failure_snackbar_action;
                if (!isFollowIdValid(followId)) {
                    snackbarController.pinToUrl(tab, url);
                }
            }
            showSnackbar(mContext.getString(failureMessage), snackbarController,
                    Snackbar.UMA_WEB_FEED_FOLLOW_FAILURE, actionStringId);
        }
    }

    /**
     * Show appropriate post-unfollow snackbar depending on success/failure.
     */
    void showSnackbarForUnfollow(
            int requestStatus, byte[] followId, GURL url, String title, int webFeedChangeReason) {
        if (requestStatus == WebFeedSubscriptionRequestStatus.SUCCESS) {
            showUnfollowSuccessSnackbar(followId, url, title, webFeedChangeReason);
        } else {
            showUnfollowFailureSnackbar(requestStatus, followId, url, title, webFeedChangeReason);
        }
    }

    private void showPostSuccessfulFollowHelp(String title, boolean isActive) {
        if (TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile())
                        .shouldTriggerHelpUI(
                                FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE)) {
            mWebFeedDialogCoordinator.initialize(mContext, mFeedLauncher, title, isActive);
            mWebFeedDialogCoordinator.showDialog();
        } else {
            SnackbarController snackbarController = new PinnedSnackbarController() {
                @Override
                public void onAction(Object actionData) {
                    super.onAction(actionData);
                    mFeedLauncher.openFollowingFeed();
                }
            };
            showSnackbar(
                    mContext.getString(R.string.web_feed_follow_success_snackbar_message, title),
                    snackbarController, Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS,
                    R.string.web_feed_follow_success_snackbar_action);
        }
    }

    private void showUnfollowSuccessSnackbar(
            byte[] followId, GURL url, String title, int webFeedChangeReason) {
        showSnackbar(mContext.getString(R.string.web_feed_unfollow_success_snackbar_message, title),
                new FollowActionSnackbarController(followId, url, title,
                        FeedUserActionType.TAPPED_REFOLLOW_AFTER_UNFOLLOW_ON_SNACKBAR,
                        webFeedChangeReason),
                Snackbar.UMA_WEB_FEED_UNFOLLOW_SUCCESS,
                R.string.web_feed_unfollow_success_snackbar_action);
    }

    private void showUnfollowFailureSnackbar(
            int requestStatus, byte[] followId, GURL url, String title, int webFeedChangeReason) {
        int failureMessage = R.string.web_feed_unfollow_generic_failure_snackbar_message;
        if (requestStatus == WebFeedSubscriptionRequestStatus.FAILED_OFFLINE) {
            failureMessage = R.string.web_feed_offline_failure_snackbar_message;
        }

        SnackbarController snackbarController = new PinnedSnackbarController() {
            @Override
            public void onAction(Object actionData) {
                super.onAction(actionData);
                FeedServiceBridge.reportOtherUserAction(StreamKind.UNKNOWN,
                        FeedUserActionType.TAPPED_UNFOLLOW_TRY_AGAIN_ON_SNACKBAR);
                WebFeedBridge.unfollow(
                        followId, /*isDurable=*/false, webFeedChangeReason, result -> {
                            showSnackbarForUnfollow(result.requestStatus, followId, url, title,
                                    webFeedChangeReason);
                        });
            }
        };
        showSnackbar(mContext.getString(failureMessage), snackbarController,
                Snackbar.UMA_WEB_FEED_UNFOLLOW_FAILURE,
                R.string.web_feed_generic_failure_snackbar_action);
    }

    private void showSnackbar(String message, SnackbarController snackbarController, int umaId,
            int snackbarActionId) {
        Snackbar snackbar = Snackbar.make(message, snackbarController, Snackbar.TYPE_ACTION, umaId)
                                    .setSingleLine(false)
                                    .setDuration(SNACKBAR_DURATION_MS);
        if (snackbarActionId != 0) {
            snackbar =
                    snackbar.setAction(mContext.getString(snackbarActionId), /*actionData=*/null);
        }
        mSnackbarManager.showSnackbar(snackbar);
    }

    /**
     * A snackbar controller which dismisses the snackbar if the feed surface is shown, and
     *  optionally, upon navigation to a different URL.
     */
    private class PinnedSnackbarController
            implements SnackbarController, FeedSurfaceTracker.Observer {
        Tab mPinnedTab;
        private GURL mPinnedUrl;
        private TabObserver mTabObserver;

        PinnedSnackbarController() {
            FeedSurfaceTracker.getInstance().addObserver(this);
        }

        @Override
        public void onAction(Object actionData) {
            unregisterObservers();
        }

        @Override
        public void onDismissNoAction(Object actionData) {
            unregisterObservers();
        }

        @Override
        public void surfaceOpened() {
            // Hide the snackbar if the feed surface is opened.
            mSnackbarManager.dismissSnackbars(this);
        }

        /**
         * Watch the current tab. Hide the snackbar if the current tab's URL changes.
         */
        void pinToUrl(Tab tab, GURL url) {
            assert mTabObserver == null;
            mPinnedUrl = url;
            mPinnedTab = tab;
            mTabObserver = new EmptyTabObserver() {
                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    if (!mPinnedUrl.equals(url)) {
                        urlChanged();
                    }
                }
                @Override
                public void onHidden(Tab tab, @TabHidingType int type) {
                    urlChanged();
                }
                @Override
                public void onDestroyed(Tab tab) {
                    urlChanged();
                }
            };
            mPinnedTab.addObserver(mTabObserver);
        }

        private void urlChanged() {
            mSnackbarManager.dismissSnackbars(this);
        }

        private void unregisterObservers() {
            if (mTabObserver != null) {
                mPinnedTab.removeObserver(mTabObserver);
                mTabObserver = null;
            }
            FeedSurfaceTracker.getInstance().removeObserver(this);
        }
    }

    /**
     * A {@link SnackbarController} for when the snackbar action is to follow. Prefers
     * {@link WebFeedBridge#followFromId} if an ID is available.
     */
    private class FollowActionSnackbarController extends PinnedSnackbarController {
        private final byte[] mFollowId;
        private final GURL mUrl;
        private final String mTitle;
        private final @FeedUserActionType int mUserActionType;
        private final int mWebFeedChangeReason;

        FollowActionSnackbarController(byte[] followId, GURL url, String title,
                @FeedUserActionType int userActionType, int webFeedChangeReason) {
            mFollowId = followId;
            mUrl = url;
            mTitle = title;
            mUserActionType = userActionType;
            mWebFeedChangeReason = webFeedChangeReason;
        }

        @Override
        public void onAction(Object actionData) {
            super.onAction(actionData);

            // The snackbar should not be showing if canRetryFollow() returns false.
            assert canRetryFollow(mPinnedTab, mFollowId, mUrl);
            FeedServiceBridge.reportOtherUserAction(StreamKind.UNKNOWN, mUserActionType);

            if (!isFollowIdValid(mFollowId)) {
                WebFeedBridge.followFromUrl(mPinnedTab, mUrl, mWebFeedChangeReason, result -> {
                    byte[] mFollowId = result.metadata != null ? result.metadata.id : null;
                    showPostFollowHelp(
                            mPinnedTab, result, mFollowId, mUrl, mTitle, mWebFeedChangeReason);
                });
            } else {
                WebFeedBridge.followFromId(mFollowId, /*isDurable=*/false, mWebFeedChangeReason,
                        result
                        -> showPostFollowHelp(
                                mPinnedTab, result, mFollowId, mUrl, mTitle, mWebFeedChangeReason));
            }
        }
    }

    private static boolean isFollowIdValid(byte[] followId) {
        return followId != null && followId.length != 0;
    }

    private static boolean canRetryFollow(Tab tab, byte[] followId, GURL url) {
        if (isFollowIdValid(followId)) {
            return true;
        }
        return tab != null && url.equals(tab.getOriginalUrl());
    }
}
