// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.base.Log;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
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

    FollowManagementMediator(Context context, ModelList modelList, Adapter adapter) {
        mModelList = modelList;
        mContext = context;
        mAdapter = adapter;

        // TODO(petewil) Make the weebFeedBridge method static.
        WebFeedBridge webFeedBridge = new WebFeedBridge();

        webFeedBridge.getAllFollowedWebFeeds(this::followedPagesCallback);
    }

    private void followedPagesCallback(List<WebFeedMetadata> followedPages) {
        // TODO(petewil) Remove when the feature is complete, for debugging.
        Log.d(TAG, "followedPagesCallback size " + followedPages.size());
        if (followedPages.size() < 1) return;

        for (WebFeedMetadata page : followedPages) {
            String title = page.title;
            String url = page.visitUrl.getSpec();
            PropertyModel pageModel = generateListItem(title, url, this::handleClick);
            mModelList.add(new SimpleRecyclerViewAdapter.ListItem(
                    FollowManagementItemProperties.DEFAULT_ITEM_TYPE, pageModel));
        }

        mAdapter.notifyDataSetChanged();
    }

    // Adapt for our data items.
    private PropertyModel generateListItem(String title, String url, OnClickListener listener) {
        return new PropertyModel.Builder(FollowManagementItemProperties.ALL_KEYS)
                .with(FollowManagementItemProperties.TITLE_KEY, title)
                .with(FollowManagementItemProperties.DESCRIPTION_KEY, url)
                .with(FollowManagementItemProperties.ON_CLICK_KEY, listener)
                .build();
    }

    private void handleClick(View view) {
        Log.d(TAG, "Follow management item click caught.");
        // TODO(petewil): Handle the click and enable or disable the following site for our feed.
    }
}
