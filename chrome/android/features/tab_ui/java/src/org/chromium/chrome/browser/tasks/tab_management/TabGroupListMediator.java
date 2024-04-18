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
import org.chromium.base.Token;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.bookmarks.PendingRunnable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.AsyncDrawable;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.ArrayList;
import java.util.List;

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
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final CallbackController mCallbackController = new CallbackController();
    private final PendingRunnable mPendingRefresh =
            new PendingRunnable(
                    TaskTraits.UI_DEFAULT,
                    mCallbackController.makeCancelable(this::repopulateModelList));

    private final TabGroupModelFilterObserver mFilterObserver =
            new TabGroupModelFilterObserver() {
                @Override
                public void didMergeTabToGroup(Tab movedTab, int selectedTabIdInGroup) {
                    mPendingRefresh.post();
                }

                @Override
                public void didMoveTabGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    mPendingRefresh.post();
                }

                @Override
                public void didMoveWithinGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    mPendingRefresh.post();
                }

                @Override
                public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                    mPendingRefresh.post();
                }

                @Override
                public void didCreateGroup(
                        List<Tab> tabs,
                        List<Integer> tabOriginalIndex,
                        List<Integer> tabOriginalRootId,
                        List<Token> tabOriginalTabGroupId,
                        String destinationGroupTitle,
                        int destinationGroupColorId) {
                    mPendingRefresh.post();
                }

                @Override
                public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                    mPendingRefresh.post();
                }
            };

    private TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void onFinishingTabClosure(Tab tab) {
                    mPendingRefresh.post();
                }

                @Override
                public void onFinishingMultipleTabClosure(List<Tab> tabs) {
                    mPendingRefresh.post();
                }

                @Override
                public void didAddTab(
                        Tab tab, int type, int creationState, boolean markedForSelection) {
                    mPendingRefresh.post();
                }

                @Override
                public void tabClosureUndone(Tab tab) {
                    mPendingRefresh.post();
                }

                @Override
                public void allTabsClosureUndone() {
                    mPendingRefresh.post();
                }

                @Override
                public void tabPendingClosure(Tab tab) {
                    mPendingRefresh.post();
                }

                @Override
                public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                    mPendingRefresh.post();
                }

                @Override
                public void tabClosureCommitted(Tab tab) {
                    mPendingRefresh.post();
                }

                @Override
                public void tabRemoved(Tab tab) {
                    mPendingRefresh.post();
                }
            };

    /**
     * @param modelList Side effect is adding items to this list.
     * @param filter Used to read current tab groups.
     * @param tabListFaviconProvider Used to fetch favicon images for some tabs.
     */
    public TabGroupListMediator(
            ModelList modelList,
            TabGroupModelFilter filter,
            TabListFaviconProvider tabListFaviconProvider) {
        mModelList = modelList;
        mFilter = filter;
        mTabListFaviconProvider = tabListFaviconProvider;
        mFilter.addTabGroupObserver(mFilterObserver);
        mFilter.addObserver(mTabModelObserver);
        // TODO(b:324935209): Watch for title and color changes.
        repopulateModelList();
    }

    /** Clean up observers used by this class. */
    public void destroy() {
        mFilter.removeTabGroupObserver(mFilterObserver);
        mFilter.removeObserver(mTabModelObserver);
        mCallbackController.destroy();
    }

    private List<Tab> getAllRootTabs() {
        List<Tab> rootTabList = new ArrayList<>();

        int groupCount = mFilter.getCount();
        for (int i = 0; i < groupCount; i++) {
            // These tabs are either single tabs, or the last shown tab in a group.
            Tab tab = mFilter.getTabAt(i);

            if (!mFilter.isTabInTabGroup(tab)) {
                continue;
            }

            if (tab.getId() != tab.getRootId()) {
                tab = TabModelUtils.getTabById(mFilter.getTabModel(), tab.getRootId());
            }

            rootTabList.add(tab);
        }

        return rootTabList;
    }

    private void repopulateModelList() {
        mModelList.clear();

        // TODO(b:324935209): Use sync data instead of active tab groups.
        for (Tab rootTab : getAllRootTabs()) {
            PropertyModel.Builder builder = new PropertyModel.Builder(ALL_KEYS);
            int tabId = rootTab.getId();
            List<Tab> relatedTabs = mFilter.getRelatedTabList(tabId);
            int numberOfTabs = relatedTabs.size();
            int numberOfCorners = FAVICON_ORDER.length;
            int standardCorners = numberOfCorners - 1;
            for (int i = 0; i < standardCorners; i++) {
                if (relatedTabs.size() > i) {
                    builder.with(FAVICON_ORDER[i], buildAsyncDrawable(relatedTabs.get(i)));
                } else {
                    break;
                }
            }
            if (relatedTabs.size() == numberOfCorners) {
                builder.with(
                        FAVICON_ORDER[standardCorners],
                        buildAsyncDrawable(relatedTabs.get(standardCorners)));
            } else if (numberOfTabs > numberOfCorners) {
                builder.with(PLUS_COUNT, numberOfTabs - standardCorners);
            }

            if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                @TabGroupColorId int colorIndex = mFilter.getTabGroupColor(tabId);
                builder.with(COLOR_INDEX, colorIndex);
            }

            String userTitle = mFilter.getTabGroupTitle(tabId);
            Pair<String, Integer> titleData = new Pair<>(userTitle, numberOfTabs);
            builder.with(TITLE_DATA, titleData);

            // TODO(b:324935209): Use tab group creation time instead.
            long lastModified = rootTab.getLastNavigationCommittedTimestampMillis();
            builder.with(CREATION_MILLIS, lastModified);

            // TODO(b:324934166): Supply open/delete runnables.
            // builder.with(TabGroupRowProperties.OPEN_RUNNABLE, null);
            // builder.with(TabGroupRowProperties.DELETE_RUNNABLE, null);

            PropertyModel propertyModel = builder.build();
            ListItem listItem = new ListItem(0, propertyModel);
            mModelList.add(listItem);
        }
    }

    private AsyncDrawable buildAsyncDrawable(Tab tab) {
        return (Callback<Drawable> callback) ->
                mTabListFaviconProvider.getFaviconDrawableForUrlAsync(
                        tab.getUrl(), /* isIncognito= */ false, callback);
    }
}
