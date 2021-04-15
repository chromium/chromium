// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.feed.webfeed.R;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
/**
 * Sets up the model, adapter, and mediator for FollowManagement surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class FollowManagementCoordinator {
    private FollowManagementMediator mMediator;
    private final View mView;

    public FollowManagementCoordinator(Context context) {
        ModelList listItems = new ModelList();

        // Once the adapter is attached to the ListView, there is no need to hold a reference to it.
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(listItems);
        adapter.registerType(FollowManagementItemProperties.DEFAULT_ITEM_TYPE,
                new LayoutViewBuilder<FollowManagementItemView>(R.layout.follow_management_item),
                FollowManagementItemViewBinder::bind);

        // Inflate the XML.
        mView = LayoutInflater.from(context).inflate(R.layout.follow_management_activity, null);
        RecyclerView recyclerView = (RecyclerView) mView.findViewById(R.id.follow_management_list);
        recyclerView.setAdapter(adapter);

        mMediator = new FollowManagementMediator(context, listItems);
    }

    public View getView() {
        return mView;
    }
}
