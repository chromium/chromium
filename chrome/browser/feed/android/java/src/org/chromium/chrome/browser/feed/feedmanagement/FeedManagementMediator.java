// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import android.content.Context;
import android.view.View;

import org.chromium.base.Log;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

// The Mediator is responsible for click handling.
class FeedManagementMediator {
    private static final String TAG = "FeedMgmtMediator";
    private ModelList mModelList;

    FeedManagementMediator(Context context, ModelList modelList) {
        mModelList = modelList;
        // TODO(petewil): Generate all the list items.
        PropertyModel itemModel = generateListItem("Title", "Subtitle");
        mModelList.add(new ModelListAdapter.ListItem(
                FeedManagementItemProperties.ListItemType.DEFAULT, itemModel));
    }

    private PropertyModel generateListItem(String title, String subtitle) {
        return new PropertyModel.Builder(FeedManagementItemProperties.ALL_KEYS)
                .with(FeedManagementItemProperties.TITLE_KEY, title)
                .with(FeedManagementItemProperties.DESCRIPTION_KEY, subtitle)
                .with(FeedManagementItemProperties.ON_CLICK_KEY, (view) -> handleClick(view))
                .build();
    }

    private void handleClick(View view) {
        // Do some click logic here. This would typically be done in the mediator.
        Log.d(TAG, "Feed Management menu click caught.");
    }
}
