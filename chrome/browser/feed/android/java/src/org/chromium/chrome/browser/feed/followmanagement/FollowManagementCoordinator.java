// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.feed.webfeed.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
/**
 * Sets up the model, adapter, and mediator for FollowManagement surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class FollowManagementCoordinator {
    private FollowManagementMediator mMediator;
    private Activity mActivity;
    private final View mView;

    public FollowManagementCoordinator(Activity activity) {
        mActivity = activity;
        ModelList listItems = new ModelList();

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(listItems);
        adapter.registerType(FollowManagementItemProperties.DEFAULT_ITEM_TYPE,
                new LayoutViewBuilder<FollowManagementItemView>(R.layout.follow_management_item),
                FollowManagementItemViewBinder::bind);

        // Inflate the XML for the activity.
        mView = LayoutInflater.from(activity).inflate(R.layout.follow_management_activity, null);
        RecyclerView recyclerView = (RecyclerView) mView.findViewById(R.id.follow_management_list);
        // With the recycler view, we need to explicitly set a layout manager.
        LinearLayoutManager manager = new LinearLayoutManager(activity);
        recyclerView.setLayoutManager(manager);
        recyclerView.setAdapter(adapter);

        // Set up a handler for the header to act as a back button.
        ImageView backArrowView = (ImageView) mView.findViewById(R.id.follow_management_back_arrow);
        backArrowView.setOnClickListener(this::handleBackArrowClick);

        mMediator = new FollowManagementMediator(activity, listItems, adapter,
                new LargeIconBridge(Profile.getLastUsedRegularProfile()));
    }

    public View getView() {
        return mView;
    }

    private void handleBackArrowClick(View view) {
        // Navigate back.
        mActivity.finish();
    }
}
