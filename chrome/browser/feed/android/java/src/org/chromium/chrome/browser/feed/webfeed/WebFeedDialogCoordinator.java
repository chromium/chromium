// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedDialogProperties.DETAILS;
import static org.chromium.chrome.browser.feed.webfeed.WebFeedDialogProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.feed.webfeed.WebFeedDialogProperties.TITLE;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator for the WebFeed modal dialog.
 */
class WebFeedDialogCoordinator {
    private final WebFeedDialogMediator mMediator;

    private Context mContext;
    /**
     * This enum is reported in a UMA histogram. Changes must be synchronized with
     * WebFeedPostFollowDialogPresentation in enums.xml, and values may not be re-used.
     */
    @IntDef({WebFeedPostFollowDialogPresentation.AVAILABLE,
            WebFeedPostFollowDialogPresentation.UNAVAILABLE,
            WebFeedPostFollowDialogPresentation.VALUE_COUNT})
    @interface WebFeedPostFollowDialogPresentation {
        int AVAILABLE = 0;
        int UNAVAILABLE = 1;
        int VALUE_COUNT = 2;
    }

    /**
     * Constructs an instance of {@link WebFeedDialogCoordinator}.
     *
     * @param modalDialogManager {@link ModalDialogManager} for managing the dialog.
     */
    WebFeedDialogCoordinator(ModalDialogManager modalDialogManager) {
        mMediator = new WebFeedDialogMediator(modalDialogManager);
    }

    /**
     * Initializes the {@link WebFeedDialogCoordinator}.
     *
     * @param context The {@link Context}.
     * @param feedLauncher {@link FeedLauncher} for launching the NTP.
     * @param title The title of the site that was just followed.
     * @param isActive Whether the followed site is active (has content available).
     */
    void initialize(Context context, FeedLauncher feedLauncher, String title, boolean isActive) {
        mContext = context;
        View webFeedDialogView =
                LayoutInflater.from(context).inflate(R.layout.web_feed_dialog, null);
        WebFeedDialogContents dialogContents = buildDialogContents(feedLauncher, isActive, title);
        PropertyModel model = buildModel(dialogContents);
        mMediator.initialize(webFeedDialogView, dialogContents);
        PropertyModelChangeProcessor.create(
                model, webFeedDialogView, WebFeedDialogViewBinder::bind);
    }

    void showDialog() {
        mMediator.showDialog();
    }

    private WebFeedDialogContents buildDialogContents(
            FeedLauncher feedLauncher, boolean isActive, String title) {
        RecordHistogram.recordEnumeratedHistogram(
                "ContentSuggestions.Feed.WebFeed.PostFollowDialog.Show",
                isActive ? WebFeedPostFollowDialogPresentation.AVAILABLE
                         : WebFeedPostFollowDialogPresentation.UNAVAILABLE,
                WebFeedPostFollowDialogPresentation.VALUE_COUNT);

        String description;
        String primaryButtonText;
        String secondaryButtonText;
        Callback<Integer> buttonClickCallback;
        if (isActive) {
            description = mContext.getString(
                    R.string.web_feed_post_follow_dialog_stories_ready_description, title);
            primaryButtonText =
                    mContext.getString(R.string.web_feed_post_follow_dialog_open_a_new_tab);
            secondaryButtonText = mContext.getString(R.string.close);
            buttonClickCallback = dismissalCause -> {
                if (dismissalCause.equals(DialogDismissalCause.POSITIVE_BUTTON_CLICKED)) {
                    FeedServiceBridge.reportOtherUserAction(StreamKind.UNKNOWN,
                            FeedUserActionType.TAPPED_GO_TO_FEED_POST_FOLLOW_ACTIVE_HELP);
                    feedLauncher.openFollowingFeed();
                } else {
                    FeedServiceBridge.reportOtherUserAction(StreamKind.UNKNOWN,
                            FeedUserActionType.TAPPED_DISMISS_POST_FOLLOW_ACTIVE_HELP);
                }
            };
        } else {
            description = mContext.getString(
                    R.string.web_feed_post_follow_dialog_stories_not_ready_description, title);
            primaryButtonText = mContext.getString(R.string.ok);
            secondaryButtonText = null;
            buttonClickCallback = dismissalCause -> {};
        }
        return new WebFeedDialogContents(
                mContext.getString(R.string.web_feed_post_follow_dialog_title, title), description,
                R.drawable.web_feed_post_follow_illustration, primaryButtonText,
                secondaryButtonText, buttonClickCallback);
    }

    private PropertyModel buildModel(WebFeedDialogContents dialogContents) {
        return WebFeedDialogProperties.defaultModelBuilder()
                .with(TITLE, dialogContents.mTitle)
                .with(DETAILS, dialogContents.mDetails)
                .with(ILLUSTRATION, dialogContents.mIllustrationId)
                .build();
    }
}
