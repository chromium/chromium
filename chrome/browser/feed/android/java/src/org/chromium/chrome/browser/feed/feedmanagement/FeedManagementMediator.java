// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.base.Log;
import org.chromium.chrome.browser.feed.webfeed.R;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The MVC pattern Mediator for the Feed Management activity. This activity provides a common place
 * to present management options such as managing the user's activity, their interests, and their
 * list of followed sites.
 * Design doc here: https://docs.google.com/document/d/1D-ZfhGv9GFLXHYKzAqsaw-LiVhsENRTJC5ZMaZ9z0sQ/
 * edit#heading=h.p79wagdgjgx6
 */

class FeedManagementMediator {
    private static final String TAG = "FeedManagementMdtr";
    private ModelList mModelList;

    FeedManagementMediator(Context context, ModelList modelList) {
        mModelList = modelList;
        PropertyModel activityModel = generateListItem(context, R.string.feed_manage_activity,
                R.string.feed_manage_activity_description, this::handleActivityClick);
        PropertyModel interestsModel = generateListItem(context, R.string.feed_manage_interests,
                R.string.feed_manage_interests_description, this::handleInterestsClick);
        PropertyModel hiddenModel = generateListItem(context, R.string.feed_manage_hidden,
                R.string.feed_manage_hidden_description, this::handleHiddenClick);
        PropertyModel followingModel = generateListItem(context, R.string.feed_manage_following,
                R.string.feed_manage_following_description, this::handleFollowingClick);
        // Add the menu items into the menu.
        mModelList.add(new ModelListAdapter.ListItem(
                FeedManagementItemProperties.DEFAULT_ITEM_TYPE, activityModel));
        mModelList.add(new ModelListAdapter.ListItem(
                FeedManagementItemProperties.DEFAULT_ITEM_TYPE, interestsModel));
        mModelList.add(new ModelListAdapter.ListItem(
                FeedManagementItemProperties.DEFAULT_ITEM_TYPE, hiddenModel));
        mModelList.add(new ModelListAdapter.ListItem(
                FeedManagementItemProperties.DEFAULT_ITEM_TYPE, followingModel));
    }

    private PropertyModel generateListItem(
            Context context, int titleResource, int descriptionResource, OnClickListener listener) {
        String title = context.getResources().getString(titleResource);
        String description = context.getResources().getString(descriptionResource);
        return new PropertyModel.Builder(FeedManagementItemProperties.ALL_KEYS)
                .with(FeedManagementItemProperties.TITLE_KEY, title)
                .with(FeedManagementItemProperties.DESCRIPTION_KEY, description)
                .with(FeedManagementItemProperties.ON_CLICK_KEY, listener)
                .build();
    }

    private void handleActivityClick(View view) {
        Log.d(TAG, "Activity click caught.");
        // TODO(petewil): Launch URL for activity.
    }

    private void handleInterestsClick(View view) {
        Log.d(TAG, "Interests click caught.");
        // TODO(petewil): TODO(petewil): Launch URL for interests.
    }

    private void handleHiddenClick(View view) {
        Log.d(TAG, "Hidden click caught.");
        // TODO(petewil): Launch URL for hidden.
    }

    private void handleFollowingClick(View view) {
        Log.d(TAG, "Following click caught.");
        // TODO(petewil): Launch a new activity for the following management page.
    }
}
