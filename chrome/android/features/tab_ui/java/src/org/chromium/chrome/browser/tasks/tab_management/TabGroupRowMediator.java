// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.AsyncDrawable;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

import java.util.function.BiConsumer;

/** Contains the logic to set the state of the model and react to actions. */
class TabGroupRowMediator {

    private static final WritableObjectPropertyKey[] FAVICON_ORDER = {
        TabGroupRowProperties.ASYNC_FAVICON_TOP_LEFT,
        TabGroupRowProperties.ASYNC_FAVICON_TOP_RIGHT,
        TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_LEFT,
        TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_RIGHT
    };

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
            BiConsumer<GURL, Callback<Drawable>> faviconResolver,
            @Nullable Runnable openRunnable,
            @Nullable Runnable deleteRunnable) {
        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        int numberOfTabs = savedTabGroup.savedTabs.size();
        int numberOfCorners = FAVICON_ORDER.length;
        int standardCorners = numberOfCorners - 1;
        for (int i = 0; i < standardCorners; i++) {
            if (numberOfTabs > i) {
                builder.with(
                        FAVICON_ORDER[i],
                        buildAsyncDrawable(savedTabGroup.savedTabs.get(i), faviconResolver));
            } else {
                break;
            }
        }
        if (numberOfTabs == numberOfCorners) {
            builder.with(
                    FAVICON_ORDER[standardCorners],
                    buildAsyncDrawable(
                            savedTabGroup.savedTabs.get(standardCorners), faviconResolver));
        } else if (numberOfTabs > numberOfCorners) {
            builder.with(TabGroupRowProperties.PLUS_COUNT, numberOfTabs - standardCorners);
        }

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

    private static AsyncDrawable buildAsyncDrawable(
            SavedTabGroupTab tab, BiConsumer<GURL, Callback<Drawable>> faviconResolver) {
        return (Callback<Drawable> callback) -> faviconResolver.accept(tab.url, callback);
    }
}
