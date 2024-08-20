// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.Nullable;
import androidx.core.util.Pair;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Contains the logic to set the state of the model and react to actions. */
class TabGroupRowMediator {
    /**
     * Build a property model builder for the associated saved tab group.
     *
     * @param savedTabGroup The state of the tab group.
     * @param faviconResolver Used to fetch favicon images for tabs.
     * @param openRunnable Runnable for when opening the tab group.
     * @param deleteRunnable Runnable for when deleting the tab group.
     */
    public static PropertyModel buildModel(
            SavedTabGroup savedTabGroup,
            FaviconResolver faviconResolver,
            @Nullable Runnable openRunnable,
            @Nullable Runnable deleteRunnable) {
        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        List<SavedTabGroupTab> savedTabs = savedTabGroup.savedTabs;
        int numberOfTabs = savedTabs.size();
        int urlCount = Math.min(TabGroupFaviconCluster.CORNER_COUNT, numberOfTabs);
        List<GURL> urlList = new ArrayList<>();
        for (int i = 0; i < urlCount; i++) {
            urlList.add(savedTabs.get(i).url);
        }
        ClusterData clusterData = new ClusterData(faviconResolver, numberOfTabs, urlList);
        builder.with(TabGroupRowProperties.CLUSTER_DATA, clusterData);

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            builder.with(TabGroupRowProperties.COLOR_INDEX, savedTabGroup.color);
        }

        String userTitle = savedTabGroup.title;
        Pair<String, Integer> titleData = new Pair<>(userTitle, numberOfTabs);
        builder.with(TabGroupRowProperties.TITLE_DATA, titleData);

        builder.with(TabGroupRowProperties.CREATION_MILLIS, savedTabGroup.creationTimeMs);
        builder.with(TabGroupRowProperties.OPEN_RUNNABLE, openRunnable);
        builder.with(TabGroupRowProperties.DELETE_RUNNABLE, deleteRunnable);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)) {
            boolean isShared = (savedTabGroup.collaborationId != null);
            builder.with(TabGroupRowProperties.IS_SHARED, isShared);
        }

        return builder.build();
    }
}
