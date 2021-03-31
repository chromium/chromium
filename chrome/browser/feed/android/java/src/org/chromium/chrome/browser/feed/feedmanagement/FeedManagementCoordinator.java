// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import org.chromium.chrome.browser.feed.webfeed.R; // TODO(petewil): move to feed.feedmanagement?
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
/**
 * Sets up the model, adapter, and mediator for FeedManagement surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class FeedManagementCoordinator {
    private FeedManagementMediator mMediator;
    private final View mView;

    public FeedManagementCoordinator(Context context) {
        ModelList listItems = new ModelList();

        // Once this is attached to the ListView, there is no need to hold a reference to it.
        ModelListAdapter adapter = new ModelListAdapter(listItems);
        adapter.registerType(FeedManagementItemProperties.DEFAULT_ITEM_TYPE,
                new LayoutViewBuilder<FeedManagementItemView>(R.layout.feed_management_list_item),
                FeedManagementItemViewBinder::bind);

        // Inflate the XML.
        mView = LayoutInflater.from(context).inflate(R.layout.feed_management_activity, null);
        ListView listView = (ListView) mView.findViewById(R.id.feed_management_menu);
        listView.setAdapter(adapter);

        mMediator = new FeedManagementMediator(context, listItems);
    }

    public View getView() {
        return mView;
    }
}
