// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.webfeed.WebFeedFaviconFetcher;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.Toast;

/**
 * Sets up the model, adapter, and mediator for FollowManagement surface. It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class FollowManagementCoordinator {
    private AppCompatActivity mActivity;
    private final View mView;

    public FollowManagementCoordinator(Activity activity) {
        mActivity = (AppCompatActivity) activity;
        ModelList listItems = new ModelList();

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(listItems);
        // Register types for both the full and empty states.
        adapter.registerType(
                FollowManagementItemProperties.DEFAULT_ITEM_TYPE,
                new LayoutViewBuilder<FollowManagementItemView>(R.layout.follow_management_item),
                FollowManagementItemViewBinder::bind);
        adapter.registerType(
                FollowManagementItemProperties.EMPTY_ITEM_TYPE,
                new LayoutViewBuilder<LinearLayout>(R.layout.follow_management_empty_state),
                (unusedModel, unusedView, unusedKey) -> {});
        adapter.registerType(
                FollowManagementItemProperties.LOADING_ITEM_TYPE,
                new LayoutViewBuilder<LinearLayout>(R.layout.feed_spinner),
                (unusedModel, unusedView, unusedKey) -> {});

        // Inflate the XML for the activity.
        mView = LayoutInflater.from(activity).inflate(R.layout.follow_management_activity, null);
        RecyclerView recyclerView = mView.findViewById(R.id.follow_management_list);
        // With the recycler view, we need to explicitly set a layout manager.
        LinearLayoutManager manager = new LinearLayoutManager(activity);
        recyclerView.setLayoutManager(manager);
        recyclerView.setAdapter(adapter);

        new FollowManagementMediator(
                activity, listItems, new MediatorObserver(), WebFeedFaviconFetcher.createDefault());
    }

    public View getView() {
        return mView;
    }

    private class MediatorObserver implements FollowManagementMediator.Observer {
        @Override
        public void networkConnectionError() {
            Toast.makeText(mActivity, R.string.feed_follow_no_connection_error, Toast.LENGTH_LONG)
                    .show();
        }

        @Override
        public void otherOperationError() {
            Toast.makeText(mActivity, R.string.feed_follow_unknown_error, Toast.LENGTH_LONG).show();
        }
    }
}
