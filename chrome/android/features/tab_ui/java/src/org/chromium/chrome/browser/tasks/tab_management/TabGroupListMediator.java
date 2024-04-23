// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CREATION_MILLIS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.PLUS_COUNT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import android.graphics.drawable.Drawable;

import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.bookmarks.PendingRunnable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.AsyncDrawable;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService.Observer;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

import java.util.function.BiConsumer;

/** Populates a {@link ModelList} with an item for each tab group. */
public class TabGroupListMediator {
    private static final WritableObjectPropertyKey[] FAVICON_ORDER = {
        ASYNC_FAVICON_TOP_LEFT,
        ASYNC_FAVICON_TOP_RIGHT,
        ASYNC_FAVICON_BOTTOM_LEFT,
        ASYNC_FAVICON_BOTTOM_RIGHT
    };

    private final ModelList mModelList;
    private final TabGroupModelFilter mFilter;
    private final BiConsumer<GURL, Callback<Drawable>> mFaviconResolver;
    private final TabGroupSyncService mSyncService;
    private final CallbackController mCallbackController = new CallbackController();
    private final PendingRunnable mPendingRefresh =
            new PendingRunnable(
                    TaskTraits.UI_DEFAULT,
                    mCallbackController.makeCancelable(this::repopulateModelList));

    private final TabGroupSyncService.Observer mSyncObserver =
            new Observer() {
                @Override
                public void onInitialized() {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupAdded(SavedTabGroup group) {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupUpdated(SavedTabGroup group) {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupRemoved(LocalTabGroupId localId) {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupRemoved(String syncId) {
                    mPendingRefresh.post();
                }
            };

    /**
     * @param modelList Side effect is adding items to this list.
     * @param filter Used to read current tab groups.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param syncService Used to fetch synced copy of tab groups.
     */
    public TabGroupListMediator(
            ModelList modelList,
            TabGroupModelFilter filter,
            BiConsumer<GURL, Callback<Drawable>> faviconResolver,
            TabGroupSyncService syncService) {
        mModelList = modelList;
        mFilter = filter;
        mFaviconResolver = faviconResolver;
        mSyncService = syncService;
        mSyncService.addObserver(mSyncObserver);
        repopulateModelList();
    }

    /** Clean up observers used by this class. */
    public void destroy() {
        mSyncService.removeObserver(mSyncObserver);
        mCallbackController.destroy();
    }

    private void repopulateModelList() {
        mModelList.clear();

        for (String groupId : mSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = mSyncService.getGroup(groupId);
            PropertyModel.Builder builder = new PropertyModel.Builder(ALL_KEYS);

            int numberOfTabs = savedTabGroup.savedTabs.size();
            int numberOfCorners = FAVICON_ORDER.length;
            int standardCorners = numberOfCorners - 1;
            for (int i = 0; i < standardCorners; i++) {
                if (numberOfTabs > i) {
                    builder.with(
                            FAVICON_ORDER[i], buildAsyncDrawable(savedTabGroup.savedTabs.get(i)));
                } else {
                    break;
                }
            }
            if (numberOfTabs == numberOfCorners) {
                builder.with(
                        FAVICON_ORDER[standardCorners],
                        buildAsyncDrawable(savedTabGroup.savedTabs.get(standardCorners)));
            } else if (numberOfTabs > numberOfCorners) {
                builder.with(PLUS_COUNT, numberOfTabs - standardCorners);
            }

            if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                builder.with(COLOR_INDEX, savedTabGroup.color);
            }

            String userTitle = savedTabGroup.title;
            Pair<String, Integer> titleData = new Pair<>(userTitle, numberOfTabs);
            builder.with(TITLE_DATA, titleData);

            builder.with(CREATION_MILLIS, savedTabGroup.creationTimeMs);

            // TODO(b:324934166): Supply open/delete runnables.
            // builder.with(TabGroupRowProperties.OPEN_RUNNABLE, null);
            // builder.with(TabGroupRowProperties.DELETE_RUNNABLE, null);

            PropertyModel propertyModel = builder.build();
            ListItem listItem = new ListItem(0, propertyModel);
            mModelList.add(listItem);
        }
    }

    private AsyncDrawable buildAsyncDrawable(SavedTabGroupTab tab) {
        return (Callback<Drawable> callback) -> mFaviconResolver.accept(tab.url, callback);
    }
}
