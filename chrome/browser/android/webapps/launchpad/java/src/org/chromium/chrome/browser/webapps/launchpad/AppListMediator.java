// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.BitmapDrawable;

import org.chromium.components.browser_ui.widget.tile.TileViewProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;
import org.chromium.webapk.lib.client.WebApkNavigationClient;

import java.util.List;

/**
 * Mediator class for launchpad app list. Handles updating the list model for current
 * app list.
 */
class AppListMediator {
    private final Context mContext;
    private final AppListCoordinator mCoordinator;
    private final ModelList mListModel;
    private final List<LaunchpadItem> mLaunchpadItems;

    AppListMediator(Context context, AppListCoordinator coordinator, ModelList listModel,
            List<LaunchpadItem> items) {
        mContext = context;
        mCoordinator = coordinator;
        mListModel = listModel;
        mLaunchpadItems = items;

        populateData();
    }

    void destroy() {
        mLaunchpadItems.clear();
    }

    private void populateData() {
        for (LaunchpadItem item : mLaunchpadItems) {
            PropertyModel tileModel = new PropertyModel(TileViewProperties.ALL_KEYS);
            tileModel.set(TileViewProperties.TITLE, item.shortName);
            tileModel.set(TileViewProperties.TITLE_LINES, 1);
            tileModel.set(TileViewProperties.CONTENT_DESCRIPTION, item.shortName);
            tileModel.set(TileViewProperties.ICON, new BitmapDrawable(item.icon));
            tileModel.set(TileViewProperties.ON_CLICK, v -> clickItem(item));
            tileModel.set(TileViewProperties.ON_LONG_CLICK, v -> mCoordinator.showMenu(item));

            mListModel.add(new ListItem(AppListCoordinator.DEFAULT_TILE_TYPE, tileModel));
        }
    }

    private void clickItem(LaunchpadItem item) {
        Intent launchIntent = WebApkNavigationClient.createLaunchWebApkIntent(
                item.packageName, item.url, false /* forceNavigation */);
        try {
            mContext.startActivity(launchIntent);
        } catch (ActivityNotFoundException e) {
            Toast.makeText(mContext, R.string.open_webapk_failed, Toast.LENGTH_SHORT).show();
        }
    }
}
