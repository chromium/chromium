// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.CheckBox;

import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.FollowResults;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.UnfollowResults;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
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
    private Context mContext;
    private Adapter mAdapter;
    private boolean mSubscribed;
    private WebFeedBridge mWebFeedBridge;

    /**
     * Nested class to curry arguments into the listener so we can use them later.
     */
    public class ClickListener {
        private final byte[] mId;
        private boolean mSubscribed;
        private CheckBox mCheckbox;

        /** Callback from the WebFeedBridge's followFromId method */
        public class FollowCallback implements Callback<FollowResults> {
            /** Report reesults of follow operation. */
            @Override
            public void onResult(FollowResults results) {
                if (results.requestStatus != WebFeedSubscriptionRequestStatus.SUCCESS) {
                    // If the operation failed, re-uncheck the checkbox.
                    mCheckbox.setChecked(false);
                }
                mCheckbox.setClickable(true);
            }
        }

        /** Callback from WebFeedBridge's unfollow operation. */
        public class UnfollowCallback implements Callback<UnfollowResults> {
            /** Report results of unfollow operation. */
            @Override
            public void onResult(UnfollowResults results) {
                if (results.requestStatus != WebFeedSubscriptionRequestStatus.SUCCESS) {
                    // If the operation failed, re-check the checkbox.
                    mCheckbox.setChecked(true);
                }
                mCheckbox.setClickable(true);
            }
        }

        ClickListener(byte[] id) {
            mId = id;
        }

        /**
         * returns the click handler to use with android.
         */
        public OnClickListener getClickListener() {
            return this::clickHandler;
        }

        /**
         * Click handler for clicks on the checkbox.  Follows or unfollows as needed.
         */
        public void clickHandler(View view) {
            mCheckbox = (CheckBox) view;
            // Disable the button until we get a callback to prevent duplicate events.
            mCheckbox.setClickable(false);
            // If we were subscribed, unfollow, and vice versa.  The checkbox is already in its new
            // state, so make the reality match the checkbox state.
            if (mCheckbox.isChecked()) {
                mWebFeedBridge.followFromId(mId, new FollowCallback());
            } else {
                mWebFeedBridge.unfollow(mId, new UnfollowCallback());
            }
        }
    }

    FollowManagementMediator(Context context, ModelList modelList, Adapter adapter) {
        mModelList = modelList;
        mContext = context;
        mAdapter = adapter;
        mWebFeedBridge = new WebFeedBridge();

        mWebFeedBridge.getAllFollowedWebFeeds(this::followedWebFeedsCallback);
    }

    // When we get the list of followed pages, add them to the recycler view.
    private void followedWebFeedsCallback(List<WebFeedMetadata> followedWebFeeds) {
        for (WebFeedMetadata page : followedWebFeeds) {
            String title = page.title;
            String url = page.visitUrl.getSpec();
            byte[] id = page.id;
            boolean subscribed = false;
            int subscriptionStatus = page.subscriptionStatus;
            if (subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBED
                    || subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBE_IN_PROGRESS) {
                subscribed = true;
            }
            OnClickListener listener = (new ClickListener(id)).getClickListener();
            PropertyModel pageModel = generateListItem(title, url, subscribed, listener);
            mModelList.add(new SimpleRecyclerViewAdapter.ListItem(
                    FollowManagementItemProperties.DEFAULT_ITEM_TYPE, pageModel));
            // TODO(1197286): Get favicons from the item cache async, then attach to the item.
        }

        mAdapter.notifyDataSetChanged();
    }

    // Generate a list item for the recycler vivew for a followed page.
    private PropertyModel generateListItem(
            String title, String url, boolean subscribed, OnClickListener listener) {
        return new PropertyModel.Builder(FollowManagementItemProperties.ALL_KEYS)
                .with(FollowManagementItemProperties.TITLE_KEY, title)
                .with(FollowManagementItemProperties.URL_KEY, url)
                .with(FollowManagementItemProperties.ON_CLICK_KEY, listener)
                .with(FollowManagementItemProperties.SUBSCRIBED_KEY, subscribed)
                .build();
    }
}
