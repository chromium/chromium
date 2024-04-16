// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.SUCCESS;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedAvailabilityStatus;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.feed.webfeed.WebFeedFaviconFetcher;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionStatus;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/**
 * The MVC pattern Mediator for the Follow Management activity.
 * Design doc here: https://docs.google.com/document/d/1D-ZfhGv9GFLXHYKzAqsaw-LiVhsENRTJC5ZMaZ9z0sQ
 */
class FollowManagementMediator {
    private static final String TAG = "FollowManagementMdtr";
    private ModelList mModelList;
    private Observer mObserver;
    private Context mContext;
    private WebFeedFaviconFetcher mFaviconFetcher;

    public interface Observer {
        /** An operation failed because there is no network connection. */
        void networkConnectionError();

        /** An operation failed for an unknown reason. */
        void otherOperationError();
    }

    /** Build a FollowManagementMediator. */
    FollowManagementMediator(
            Context context,
            ModelList modelList,
            Observer observer,
            WebFeedFaviconFetcher faviconFetcher) {
        mModelList = modelList;
        mObserver = observer;
        mContext = context;
        mFaviconFetcher = faviconFetcher;

        // Inflate and show the loading state view inside the recycler view.
        PropertyModel pageModel = new PropertyModel();
        SimpleRecyclerViewAdapter.ListItem listItem =
                new SimpleRecyclerViewAdapter.ListItem(
                        FollowManagementItemProperties.LOADING_ITEM_TYPE, pageModel);
        mModelList.add(listItem);

        // Control flow is to refresh the feeds, then get the feed list, then display it.
        WebFeedBridge.refreshFollowedWebFeeds(this::getFollowedWebFeeds);
    }

    // Once the list of feeds has been refreshed, get the list.
    private void getFollowedWebFeeds(boolean success) {
        // TODO(crbug.com/40176853) If this fails, show a snackbar with a failure message.
        WebFeedBridge.getAllFollowedWebFeeds(this::fillRecyclerView);
    }

    // When we get the list of followed pages, add them to the recycler view.
    @VisibleForTesting
    void fillRecyclerView(List<WebFeedMetadata> followedWebFeeds) {
        String updatesUnavailable =
                mContext.getResources().getString(R.string.follow_manage_updates_unavailable);
        String waitingForContent =
                mContext.getResources().getString(R.string.follow_manage_waiting_for_content);

        // Remove the loading UI from the recycler view before showing the results.
        mModelList.clear();

        // Add the list items (if any) to the recycler view.
        for (WebFeedMetadata page : followedWebFeeds) {
            Log.d(
                    TAG,
                    "page: " + page.visitUrl + ", availability status " + page.availabilityStatus);

            String status = "";
            if (page.availabilityStatus == WebFeedAvailabilityStatus.WAITING_FOR_CONTENT) {
                status = waitingForContent;
            } else if (page.availabilityStatus == WebFeedAvailabilityStatus.INACTIVE) {
                status = updatesUnavailable;
            }
            boolean subscribed = false;
            int subscriptionStatus = page.subscriptionStatus;
            if (subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBED
                    || subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBE_IN_PROGRESS) {
                subscribed = true;
            }
            PropertyModel pageModel =
                    generateListItem(
                            page.id, page.title, page.visitUrl.getSpec(), status, subscribed);
            SimpleRecyclerViewAdapter.ListItem listItem =
                    new SimpleRecyclerViewAdapter.ListItem(
                            FollowManagementItemProperties.DEFAULT_ITEM_TYPE, pageModel);
            mModelList.add(listItem);

            // getFavicon is async.  We'll get the favicon, then add it to the model.
            mFaviconFetcher.beginFetch(
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.web_feed_management_icon_size),
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.web_feed_monogram_text_size),
                    page.visitUrl,
                    page.faviconUrl,
                    (favicon) -> {
                        listItem.model.set(FollowManagementItemProperties.FAVICON_KEY, favicon);
                    });
        }
        // If there are no subscribed feeds, show the empty state instead.
        if (followedWebFeeds.isEmpty()) {
            // Inflate and show the empty state view inside the recycler view.
            PropertyModel pageModel = new PropertyModel();
            SimpleRecyclerViewAdapter.ListItem listItem =
                    new SimpleRecyclerViewAdapter.ListItem(
                            FollowManagementItemProperties.EMPTY_ITEM_TYPE, pageModel);
            mModelList.add(listItem);
        }
    }

    // Generate a list item for the recycler view for a followed page.
    private PropertyModel generateListItem(
            byte[] id, String title, String url, String status, boolean subscribed) {
        PropertyModel model =
                new PropertyModel.Builder(FollowManagementItemProperties.ALL_KEYS)
                        .with(FollowManagementItemProperties.ID_KEY, id)
                        .with(FollowManagementItemProperties.TITLE_KEY, title)
                        .with(FollowManagementItemProperties.URL_KEY, url)
                        .with(FollowManagementItemProperties.STATUS_KEY, status)
                        .with(FollowManagementItemProperties.SUBSCRIBED_KEY, subscribed)
                        .with(FollowManagementItemProperties.CHECKBOX_ENABLED_KEY, true)
                        .build();

        model.set(FollowManagementItemProperties.ON_CLICK_KEY, () -> clickHandler(model));
        return model;
    }

    /** Click handler for clicks on the checkbox. Follows or unfollows as needed. */
    @VisibleForTesting
    void clickHandler(PropertyModel itemModel) {
        byte[] id = itemModel.get(FollowManagementItemProperties.ID_KEY);
        boolean subscribed = itemModel.get(FollowManagementItemProperties.SUBSCRIBED_KEY);
        // If we were subscribed, unfollow, and vice versa.  The checkbox is already in its
        // intended new state, so make the reality match the checkbox state.
        if (!subscribed) {
            FeedServiceBridge.reportOtherUserAction(
                    StreamKind.UNKNOWN, FeedUserActionType.TAPPED_FOLLOW_ON_MANAGEMENT_SURFACE);
            // The lambda will set the item as subscribed if the follow operation succeeds.
            WebFeedBridge.followFromId(
                    id,
                    /* isDurable= */ false,
                    WebFeedBridge.CHANGE_REASON_MANAGEMENT,
                    results -> {
                        reportRequestStatus(results.requestStatus);
                        itemModel.set(
                                FollowManagementItemProperties.SUBSCRIBED_KEY,
                                results.requestStatus == SUCCESS);
                        itemModel.set(FollowManagementItemProperties.CHECKBOX_ENABLED_KEY, true);
                    });
        } else {
            FeedServiceBridge.reportOtherUserAction(
                    StreamKind.UNKNOWN, FeedUserActionType.TAPPED_UNFOLLOW_ON_MANAGEMENT_SURFACE);
            // The lambda will set the item as unsubscribed if the unfollow operation succeeds.
            WebFeedBridge.unfollow(
                    id,
                    /* isDurable= */ false,
                    WebFeedBridge.CHANGE_REASON_MANAGEMENT,
                    results -> {
                        reportRequestStatus(results.requestStatus);
                        itemModel.set(
                                FollowManagementItemProperties.SUBSCRIBED_KEY,
                                results.requestStatus != SUCCESS);
                        itemModel.set(FollowManagementItemProperties.CHECKBOX_ENABLED_KEY, true);
                    });
        }

        itemModel.set(FollowManagementItemProperties.CHECKBOX_ENABLED_KEY, false);
        itemModel.set(FollowManagementItemProperties.SUBSCRIBED_KEY, !subscribed);
    }

    void reportRequestStatus(@WebFeedSubscriptionRequestStatus int status) {
        if (status == WebFeedSubscriptionRequestStatus.FAILED_OFFLINE) {
            mObserver.networkConnectionError();
        } else if (status != WebFeedSubscriptionRequestStatus.SUCCESS) {
            mObserver.otherOperationError();
        }
    }
}
