// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.FAILED_OFFLINE;
import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.SUCCESS;

import android.content.Context;

import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Controller for showing Creator snackbars */
public class CreatorSnackbarController {
    static final int SNACKBAR_DURATION_MS = 8000;

    private Context mContext;
    private SnackbarManager mSnackbarManager;
    private SnackbarManager.SnackbarController mSnackbarController;

    /**
     * Constructs an instance of {@link CreatorSnackbarController}.
     *
     * @param context The Creator Activity this is a part of.
     * @param snackbarManager {@link SnackbarManager} to manage the snackbars.
     */
    public CreatorSnackbarController(Context context, SnackbarManager snackbarManager) {
        mContext = context;
        mSnackbarManager = snackbarManager;
        mSnackbarController =
                new SnackbarManager.SnackbarController() {
                    @Override
                    public void onAction(Object actionData) {
                        mSnackbarManager.dismissAllSnackbars();
                    }
                };
    }

    /** Show appropriate post-follow snackbar depending on success/failure. */
    void showSnackbarForFollow(int requestStatus, String title) {
        Snackbar snackbar;
        if (requestStatus == SUCCESS) {
            snackbar =
                    Snackbar.make(
                            mContext.getString(
                                    R.string.cormorant_creator_follow_success_snackbar, title),
                            mSnackbarController,
                            Snackbar.TYPE_ACTION,
                            Snackbar.UMA_CREATOR_FOLLOW_SUCCESS);
        } else if (requestStatus == FAILED_OFFLINE) {
            snackbar =
                    Snackbar.make(
                            mContext.getString(R.string.cormorant_creator_offline_failure_snackbar),
                            mSnackbarController,
                            Snackbar.TYPE_ACTION,
                            Snackbar.UMA_CREATOR_FOLLOW_FAILURE);
        } else {
            snackbar =
                    Snackbar.make(
                            mContext.getString(R.string.cormorant_creator_follow_failure_snackbar),
                            mSnackbarController,
                            Snackbar.TYPE_ACTION,
                            Snackbar.UMA_CREATOR_FOLLOW_FAILURE);
        }
        snackbar.setDuration(SNACKBAR_DURATION_MS);
        snackbar.setAction(mContext.getString(R.string.chrome_dismiss), null);
        snackbar.setSingleLine(false);
        mSnackbarManager.showSnackbar(snackbar);
    }

    /** Show appropriate post-unfollow snackbar depending on success/failure. */
    void showSnackbarForUnfollow(int requestStatus, String title) {
        Snackbar snackbar;
        if (requestStatus == SUCCESS) {
            snackbar =
                    Snackbar.make(
                            mContext.getString(
                                    R.string.cormorant_creator_unfollow_success_snackbar, title),
                            mSnackbarController,
                            Snackbar.TYPE_ACTION,
                            Snackbar.UMA_CREATOR_UNFOLLOW_SUCCESS);
        } else if (requestStatus == FAILED_OFFLINE) {
            snackbar =
                    Snackbar.make(
                            mContext.getString(R.string.cormorant_creator_offline_failure_snackbar),
                            mSnackbarController,
                            Snackbar.TYPE_ACTION,
                            Snackbar.UMA_CREATOR_UNFOLLOW_FAILURE);
        } else {
            snackbar =
                    Snackbar.make(
                            mContext.getString(
                                    R.string.cormorant_creator_unfollow_failure_snackbar),
                            mSnackbarController,
                            Snackbar.TYPE_ACTION,
                            Snackbar.UMA_CREATOR_UNFOLLOW_FAILURE);
        }
        snackbar.setDuration(SNACKBAR_DURATION_MS);
        snackbar.setAction(mContext.getString(R.string.chrome_dismiss), null);
        snackbar.setSingleLine(false);
        mSnackbarManager.showSnackbar(snackbar);
    }
}
