// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.SUCCESS;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.feed.webfeed.R;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionStatus;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

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
    private LargeIconBridge mLargeIconBridge;

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
                // The lambda will set the item as subscribed if the follow operation succeeds.
                mWebFeedBridge.followFromId(
                        mId, results -> itemView.setSubscribed(results.requestStatus == SUCCESS));
            } else {
                // The lambda will set the item as unsubscribed if the unfollow operation succeeds.
                mWebFeedBridge.unfollow(
                        mId, results -> itemView.setSubscribed(results.requestStatus != SUCCESS));
            }

            itemView.setTransitioning();
        }
    }

    /**
     * Generates the favicon to use, and if no favicon is available, creates a monogram.  A monogram
     * is a rounded icon with the first letter of the domain.  A rounded icon generator is used
     * to create the monogram.
     */
    class FaviconProvider {
        GURL mUrl;
        Callback<Bitmap> mInsertFaviconCallback;

        // Constructor - save off arguments we will need to continue.
        FaviconProvider(GURL url, Callback<Bitmap> insertFaviconCallback) {
            mUrl = url;
            mInsertFaviconCallback = insertFaviconCallback;
        }
        /**
         * Retrieve a favicon. Will callback into onFaviconAvailable.
         */
        void startFaviconFetch() {
            // Start the async call for the favicon, passing onFaviconAvailable as the callback.
            mLargeIconBridge.getLargeIconForUrl(mUrl,
                    mContext.getResources().getDimensionPixelSize(
                            R.dimen.web_feed_management_icon_size),
                    this::onFaviconAvailable);
        }

        /**
         * Passed as the callback to {@link LargeIconBridge#getLargeIconForUrl}.
         */
        private void onFaviconAvailable(@Nullable Bitmap favicon, @ColorInt int fallbackColor,
                boolean isColorDefault, @IconType int iconType) {
            // If we have a favicon, set it into the bitmap.  If not, make a monogram and put that
            // into the bitmap.
            int faviconSize = mContext.getResources().getDimensionPixelSize(
                    R.dimen.web_feed_management_icon_size);

            if (favicon == null) {
                // TODO(crbug/1152592): Update monogram according to specs.
                RoundedIconGenerator iconGenerator =
                        createRoundedIconGenerator(fallbackColor, faviconSize);
                favicon = iconGenerator.generateIconForUrl(mUrl.getSpec());
            } else {
                // Scale the bitmap to the size of the area on the screen we have for it.
                favicon = Bitmap.createScaledBitmap(favicon, faviconSize, faviconSize, false);
            }

            // Update the favicon in the model.
            mInsertFaviconCallback.onResult(favicon);
        }

        private RoundedIconGenerator createRoundedIconGenerator(
                @ColorInt int iconColor, int faviconSize) {
            int cornerRadius = faviconSize / 2;
            int textSize = mContext.getResources().getDimensionPixelSize(
                    R.dimen.web_feed_monogram_text_size);

            return new RoundedIconGenerator(
                    faviconSize, faviconSize, cornerRadius, iconColor, textSize);
        }
    }

    /**
     * Build a FollowManagementMediator.
     */
    FollowManagementMediator(Context context, ModelList modelList, Adapter adapter,
            LargeIconBridge largeIconBridge) {
        mModelList = modelList;
        mContext = context;
        mAdapter = adapter;
        mWebFeedBridge = new WebFeedBridge();
        mLargeIconBridge = largeIconBridge;

        // Control flow is to refresh the feeds, then get the feed list, then display it.
        // TODO(https://.crbug.com/1197286) Add a spinner while waiting for results.
        mWebFeedBridge.refreshFollowedWebFeeds(this::getFollowedWebFeeds);
    }

    // Once the list of feeds has been refreshed, get the list.
    private void getFollowedWebFeeds(boolean success) {
        // TODO(https://.crbug.com/1197286) If this fails, show a snackbar with a failure message.
        mWebFeedBridge.getAllFollowedWebFeeds(this::fillRecyclerView);
    }

    // When we get the list of followed pages, add them to the recycler view.
    private void fillRecyclerView(List<WebFeedMetadata> followedWebFeeds) {
        String updatesUnavailable =
                mContext.getResources().getString(R.string.follow_manage_updates_unavailable);
        for (WebFeedMetadata page : followedWebFeeds) {
            String title = page.title;
            GURL url = page.visitUrl;
            String status = page.isActive ? "" : updatesUnavailable;
            byte[] id = page.id;
            boolean subscribed = false;
            int subscriptionStatus = page.subscriptionStatus;
            if (subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBED
                    || subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBE_IN_PROGRESS) {
                subscribed = true;
            }
            OnClickListener clickListener = (new ClickListener(id)).getClickListener();
            // TODO(187319361): Once the service is returning proper paths, instead of displaying
            // just the host of the URL, display everything in the URL except the scheme.
            PropertyModel pageModel =
                    generateListItem(title, url.getHost(), status, subscribed, clickListener);
            SimpleRecyclerViewAdapter.ListItem listItem = new SimpleRecyclerViewAdapter.ListItem(
                    FollowManagementItemProperties.DEFAULT_ITEM_TYPE, pageModel);
            mModelList.add(listItem);

            // Obtain the favicon asynchronously, and insert it into the model once it arrives.
            FaviconProvider faviconProvider = new FaviconProvider(url, (favicon) -> {
                listItem.model.set(FollowManagementItemProperties.FAVICON_KEY, favicon);
                mAdapter.notifyDataSetChanged();
            });

            // getFavicon is async.  We'll get the favicon, then add it to the model.
            faviconProvider.startFaviconFetch();
        }
    }

    // Generate a list item for the recycler vivew for a followed page.
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
}
