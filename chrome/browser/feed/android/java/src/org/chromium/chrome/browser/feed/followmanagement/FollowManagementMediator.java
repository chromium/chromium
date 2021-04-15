// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.base.Log;
import org.chromium.chrome.browser.feed.webfeed.R;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The MVC pattern Mediator for the Follow Management activity.
 * Design doc here: https://docs.google.com/document/d/1D-ZfhGv9GFLXHYKzAqsaw-LiVhsENRTJC5ZMaZ9z0sQ
 */

class FollowManagementMediator {
    private static final String TAG = "FollowManagementMdtr";
    private ModelList mModelList;

    FollowManagementMediator(Context context, ModelList modelList) {
        mModelList = modelList;
        // TODO(petewil): Fill the list with data.  We use fake data for now until the service is
        // ready.
        PropertyModel activityModel = generateListItem(context, R.string.feed_manage_activity,
                R.string.feed_manage_activity_description, this::handleClick);
        // Add the menu items into the menu.
        mModelList.add(new ModelListAdapter.ListItem(
                FollowManagementItemProperties.DEFAULT_ITEM_TYPE, activityModel));
    }

    // Adapt for our data items.
    private PropertyModel generateListItem(
            Context context, int titleResource, int descriptionResource, OnClickListener listener) {
        String title = context.getResources().getString(titleResource);
        String description = context.getResources().getString(descriptionResource);
        return new PropertyModel.Builder(FollowManagementItemProperties.ALL_KEYS)
                .with(FollowManagementItemProperties.TITLE_KEY, title)
                .with(FollowManagementItemProperties.DESCRIPTION_KEY, description)
                .with(FollowManagementItemProperties.ON_CLICK_KEY, listener)
                .build();
    }

    private void handleClick(View view) {
        Log.d(TAG, "Follow management item click caught.");
        // TODO(petewil): Handle the click and enable or disable the following site for our feed.
    }
}
