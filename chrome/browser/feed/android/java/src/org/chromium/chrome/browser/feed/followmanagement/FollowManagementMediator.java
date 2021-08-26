// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.SUCCESS;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.base.Log;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.R;
import org.chromium.chrome.browser.feed.webfeed.WebFeedAvailabilityStatus;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.feed.webfeed.WebFeedFaviconFetcher;
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
    private Context mContext;
    private Adapter mAdapter;
    private boolean mSubscribed;
    private WebFeedFaviconFetcher mFaviconFetcher;

    /**
     * Nested class to curry arguments into the listener so we can use them later.
     */
    class ClickListener {
        private final byte[] mId;

        ClickListener(byte[] id) {
            mId = id;
        }

        /**
         * returns the click handler to use with android.
         */
        OnClickListener getClickListener() {
            return this::clickHandler;
        }

        /**
         * Click handler for clicks on the checkbox.  Follows or unfollows as needed.
         */
        void clickHandler(View view) {
            FollowManagementItemView itemView = (FollowManagementItemView) view.getParent();

            // If we were subscribed, unfollow, and vice versa.  The checkbox is already in its
            // intended new state, so make the reality match the checkbox state.
            if (itemView.isSubscribed()) {
                FeedServiceBridge.reportOtherUserAction(
                        FeedUserActionType.TAPPED_FOLLOW_ON_MANAGEMENT_SURFACE);
                // The lambda will set the item as subscribed if the follow operation succeeds.
                WebFeedBridge.followFromId(
                        mId, results -> itemView.setSubscribed(results.requestStatus == SUCCESS));
            } else {
                FeedServiceBridge.reportOtherUserAction(
                        FeedUserActionType.TAPPED_UNFOLLOW_ON_MANAGEMENT_SURFACE);
                // The lambda will set the item as unsubscribed if the unfollow operation succeeds.
                WebFeedBridge.unfollow(
                        mId, results -> itemView.setSubscribed(results.requestStatus != SUCCESS));
            }

            itemView.setTransitioning();
        }
    }

    /**
     * Build a FollowManagementMediator.
     */
    FollowManagementMediator(Context context, ModelList modelList, Adapter adapter,
            WebFeedFaviconFetcher faviconFetcher) {
        mModelList = modelList;
        mContext = context;
        mAdapter = adapter;
        mFaviconFetcher = faviconFetcher;

        // Inflate and show the loading state view inside the recycler view.
        PropertyModel pageModel = new PropertyModel();
        SimpleRecyclerViewAdapter.ListItem listItem = new SimpleRecyclerViewAdapter.ListItem(
                FollowManagementItemProperties.LOADING_ITEM_TYPE, pageModel);
        mModelList.add(listItem);

        // Control flow is to refresh the feeds, then get the feed list, then display it.
        // TODO(https://.crbug.com/1197286) Add a spinner while waiting for results.
        WebFeedBridge.refreshFollowedWebFeeds(this::getFollowedWebFeeds);
    }

    // Once the list of feeds has been refreshed, get the list.
    private void getFollowedWebFeeds(boolean success) {
        // TODO(https://.crbug.com/1197286) If this fails, show a snackbar with a failure message.
        WebFeedBridge.getAllFollowedWebFeeds(this::fillRecyclerView);
    }

    // When we get the list of followed pages, add them to the recycler view.
    void fillRecyclerView(List<WebFeedMetadata> followedWebFeeds) {
        String updatesUnavailable =
                mContext.getResources().getString(R.string.follow_manage_updates_unavailable);
        String waitingForContent =
                mContext.getResources().getString(R.string.follow_manage_waiting_for_content);

        // Remove the loading UI from the recycler view before showing the results.
        mModelList.clear();

        // Add the list items (if any) to the recycler view.
        for (WebFeedMetadata page : followedWebFeeds) {
            Log.d(TAG,
                    "page: " + page.visitUrl + ", availability status " + page.availabilityStatus);

            String status = "";
            if (page.availabilityStatus == WebFeedAvailabilityStatus.WAITING_FOR_CONTENT) {
                status = waitingForContent;
            } else if (page.availabilityStatus == WebFeedAvailabilityStatus.INACTIVE) {
                status = updatesUnavailable;
            }
            byte[] id = page.id;
            boolean subscribed = false;
            int subscriptionStatus = page.subscriptionStatus;
            if (subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBED
                    || subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBE_IN_PROGRESS) {
                subscribed = true;
            }
            OnClickListener clickListener = (new ClickListener(id)).getClickListener();
            PropertyModel pageModel = generateListItem(
                    page.title, page.visitUrl.getSpec(), status, subscribed, clickListener);
            SimpleRecyclerViewAdapter.ListItem listItem = new SimpleRecyclerViewAdapter.ListItem(
                    FollowManagementItemProperties.DEFAULT_ITEM_TYPE, pageModel);
            mModelList.add(listItem);

            // getFavicon is async.  We'll get the favicon, then add it to the model.
            mFaviconFetcher.beginFetch(mContext.getResources().getDimensionPixelSize(
                                               R.dimen.web_feed_management_icon_size),
                    mContext.getResources().getDimensionPixelSize(
                            R.dimen.web_feed_monogram_text_size),
                    page.visitUrl, page.faviconUrl, (favicon) -> {
                        listItem.model.set(FollowManagementItemProperties.FAVICON_KEY, favicon);
                        mAdapter.notifyDataSetChanged();
                    });
        }
        // If there are no subscribed feeds, show the empty state instead.
        if (followedWebFeeds.isEmpty()) {
            // Inflate and show the empty state view inside the recycler view.
            PropertyModel pageModel = new PropertyModel();
            SimpleRecyclerViewAdapter.ListItem listItem = new SimpleRecyclerViewAdapter.ListItem(
                    FollowManagementItemProperties.EMPTY_ITEM_TYPE, pageModel);
            mModelList.add(listItem);
        }
    }

    // Generate a list item for the recycler view for a followed page.
    private PropertyModel generateListItem(String title, String url, String status,
            boolean subscribed, OnClickListener clickListener) {
        return new PropertyModel.Builder(FollowManagementItemProperties.ALL_KEYS)
                .with(FollowManagementItemProperties.TITLE_KEY, title)
                .with(FollowManagementItemProperties.URL_KEY, url)
                .with(FollowManagementItemProperties.STATUS_KEY, status)
                .with(FollowManagementItemProperties.ON_CLICK_KEY, clickListener)
                .with(FollowManagementItemProperties.SUBSCRIBED_KEY, subscribed)
                .build();
    }

    ModelList getModelListForTest() {
        return mModelList;
    }
}
