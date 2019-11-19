// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.util.Pair;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFeatureUtilities;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelFilter;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.tab_groups.EmptyTabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Mediator for business logic for the tab grid. This class should be initialized with a list of
 * tabs and a TabModel to observe for changes and should not have any logic around what the list
 * signifies.
 * TODO(yusufo): Move some of the logic here to a parent component to make the above true.
 */
class TabListMediator {
    // Comparator to sort Tabs in descending order of the last shown time.
    private static final Comparator<Tab> LAST_SHOWN_COMPARATOR =
            (a, b) -> (Long.compare(b.getTimestampMillis(), a.getTimestampMillis()));

    private boolean mVisible;
    private boolean mShownIPH;

    /**
     * An interface to get the thumbnails to be shown inside the tab grid cards.
     */
    public interface ThumbnailProvider {
        /**
         * @see TabContentManager#getTabThumbnailWithCallback
         */
        void getTabThumbnailWithCallback(
                Tab tab, Callback<Bitmap> callback, boolean forceUpdate, boolean writeToCache);
    }

    /**
     * An interface to get the title to be used for a tab.
     */
    public interface TitleProvider { String getTitle(Tab tab); }

    /**
     * An interface to handle requests about updating TabGridDialog.
     */
    public interface TabGridDialogHandler {
        /**
         * This method updates the status of the ungroup bar in TabGridDialog.
         *
         * @param status The status in {@link TabGridDialogParent.UngroupBarStatus} that the ungroup
         *         bar should be updated to.
         */
        void updateUngroupBarStatus(@TabGridDialogParent.UngroupBarStatus int status);

        /**
         * This method updates the content of the TabGridDialog.
         *
         * @param tabId The id of the {@link Tab} that is used to update TabGridDialog.
         */
        void updateDialogContent(int tabId);
    }

    /**
     * The object to set to {@link TabProperties#THUMBNAIL_FETCHER} for the TabGridViewBinder to
     * obtain the thumbnail asynchronously.
     */
    static class ThumbnailFetcher {
        static Callback<Bitmap> sBitmapCallbackForTesting;
        static int sFetchCountForTesting;
        private ThumbnailProvider mThumbnailProvider;
        private Tab mTab;
        private boolean mForceUpdate;
        private boolean mWriteToCache;

        ThumbnailFetcher(
                ThumbnailProvider provider, Tab tab, boolean forceUpdate, boolean writeToCache) {
            mThumbnailProvider = provider;
            mTab = tab;
            mForceUpdate = forceUpdate;
            mWriteToCache = writeToCache;
        }

        void fetch(Callback<Bitmap> callback) {
            Callback<Bitmap> forking = (bitmap) -> {
                if (sBitmapCallbackForTesting != null) sBitmapCallbackForTesting.onResult(bitmap);
                callback.onResult(bitmap);
            };
            sFetchCountForTesting++;
            mThumbnailProvider.getTabThumbnailWithCallback(
                    mTab, forking, mForceUpdate, mWriteToCache);
        }
    }

    /**
     * An interface to show IPH for a tab.
     */
    public interface IphProvider { void showIPH(View anchor); }

    private final IphProvider mIphProvider = new IphProvider() {
        private static final int IPH_DELAY_MS = 1000;

        @Override
        public void showIPH(View anchor) {
            if (mShownIPH) return;
            mShownIPH = true;

            new Handler().postDelayed(
                    ()
                            -> TabGroupUtils.maybeShowIPH(
                                    FeatureConstants.TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE,
                                    anchor),
                    IPH_DELAY_MS);
        }
    };

    /**
     * An interface to get the onClickListener for "Create group" button.
     */
    public interface CreateGroupButtonProvider {
        /**
         * @return {@link TabActionListener} to create tab group. If the given {@link Tab} is not
         * able to create group, return null;
         */
        @Nullable
        TabActionListener getCreateGroupButtonOnClickListener(Tab tab);
    }

    /**
     * An interface to get a SelectionDelegate that contains the selected items for a selectable
     * tab list.
     */
    public interface SelectionDelegateProvider { SelectionDelegate getSelectionDelegate(); }

    /**
     * An interface to get the onClickListener when clicking on a grid card.
     */
    interface GridCardOnClickListenerProvider {
        /**
         * @return {@link TabActionListener} to open Tab Grid dialog.
         * If the given {@link Tab} is not able to create group, return null;
         */
        @Nullable
        TabActionListener openTabGridDialog(Tab tab);

        /**
         * Run additional actions on tab selection.
         * @param tabId The ID of selected {@link Tab}.
         */
        void onTabSelecting(int tabId);
    }

    @IntDef({TabClosedFrom.TAB_STRIP, TabClosedFrom.GRID_TAB_SWITCHER,
            TabClosedFrom.GRID_TAB_SWITCHER_GROUP})
    @Retention(RetentionPolicy.SOURCE)
    private @interface TabClosedFrom {
        int TAB_STRIP = 0;
        // int TAB_GRID_SHEET = 1;  // Obsolete
        int GRID_TAB_SWITCHER = 2;
        int GRID_TAB_SWITCHER_GROUP = 3;
        int NUM_ENTRIES = 4;
    }

    private static final String TAG = "TabListMediator";
    private static Map<Integer, Integer> sTabClosedFromMapTabClosedFromMap = new HashMap<>();

    private final Context mContext;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabListModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final ThumbnailProvider mThumbnailProvider;
    private final TabActionListener mTabClosedListener;
    private final TitleProvider mTitleProvider;
    private final CreateGroupButtonProvider mCreateGroupButtonProvider;
    private final SelectionDelegateProvider mSelectionDelegateProvider;
    private final GridCardOnClickListenerProvider mGridCardOnClickListenerProvider;
    private final TabGridDialogHandler mTabGridDialogHandler;
    private final String mComponentName;
    private boolean mActionsOnAllRelatedTabs;
    private ComponentCallbacks mComponentCallbacks;
    private TabGridItemTouchHelperCallback mTabGridItemTouchHelperCallback;
    private int mNextTabId = Tab.INVALID_TAB_ID;
    private boolean mTabRestoreCompleted;
    private @UiType int mUiType;

    private final TabActionListener mTabSelectedListener = new TabActionListener() {
        @Override
        public void run(int tabId) {
            if (mModel.indexFromId(tabId) == TabModel.INVALID_TAB_INDEX) return;

            mNextTabId = tabId;

            if (!mActionsOnAllRelatedTabs) {
                Tab currentTab = mTabModelSelector.getCurrentTab();
                Tab newlySelectedTab =
                        TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId);

                // We filtered the tab switching related metric for components that takes actions on
                // all related tabs (e.g. GTS) because that component can switch to different
                // TabModel before switching tabs, while this class only contains information for
                // all tabs that are in the same TabModel, more specifically:
                //   * For Tabs.TabOffsetOfSwitch, we do not want to log anything if the user
                //     switched from normal to incognito or vice-versa.
                //   * For MobileTabSwitched, as compared to the VTS, we need to account for
                //     MobileTabReturnedToCurrentTab action. This action is defined as return to the
                //     same tab as before entering the component, and we don't have this information
                //     here.
                recordUserSwitchedTab(currentTab, newlySelectedTab);
            }
            if (mGridCardOnClickListenerProvider != null) {
                mGridCardOnClickListenerProvider.onTabSelecting(tabId);
            } else {
                mTabModelSelector.getCurrentModel().setIndex(
                        TabModelUtils.getTabIndexById(mTabModelSelector.getCurrentModel(), tabId),
                        TabSelectionType.FROM_USER);
            }
        }

        /**
         * Records MobileTabSwitched for the component. Also, records Tabs.TabOffsetOfSwitch but
         * only when fromTab and toTab are within the same group. This method only records UMA
         * for components other than TabSwitcher.
         *
         * @param fromTab The previous selected tab.
         * @param toTab The new selected tab.
         */
        private void recordUserSwitchedTab(Tab fromTab, Tab toTab) {
            int fromFilterIndex = mTabModelSelector.getTabModelFilterProvider()
                                          .getCurrentTabModelFilter()
                                          .indexOf(fromTab);
            int toFilterIndex = mTabModelSelector.getTabModelFilterProvider()
                                        .getCurrentTabModelFilter()
                                        .indexOf(toTab);

            RecordUserAction.record("MobileTabSwitched." + mComponentName);

            if (fromFilterIndex != toFilterIndex) return;

            int fromIndex = TabModelUtils.getTabIndexById(
                    mTabModelSelector.getCurrentModel(), fromTab.getId());
            int toIndex = TabModelUtils.getTabIndexById(
                    mTabModelSelector.getCurrentModel(), toTab.getId());

            RecordHistogram.recordSparseHistogram(
                    "Tabs.TabOffsetOfSwitch." + mComponentName, fromIndex - toIndex);
        }
    };

    private final TabActionListener mSelectableTabOnClickListener = new TabActionListener() {
        @Override
        public void run(int tabId) {
            int index = mModel.indexFromId(tabId);
            if (index == TabModel.INVALID_TAB_INDEX) return;
            boolean selected = mModel.get(index).model.get(TabProperties.IS_SELECTED);
            if (selected) {
                RecordUserAction.record("TabMultiSelect.TabUnselected");
            } else {
                RecordUserAction.record("TabMultiSelect.TabSelected");
            }
            mModel.get(index).model.set(TabProperties.IS_SELECTED, !selected);
        }
    };

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onDidStartNavigation(Tab tab, NavigationHandle navigationHandle) {
            if (NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) return;
            if (navigationHandle.isSameDocument() || !navigationHandle.isInMainFrame()) return;
            if (mModel.indexFromId(tab.getId()) == TabModel.INVALID_TAB_INDEX) return;
            mModel.get(mModel.indexFromId(tab.getId()))
                    .model.set(TabProperties.FAVICON,
                            mTabListFaviconProvider.getDefaultFaviconDrawable(tab.isIncognito()));
        }

        @Override
        public void onTitleUpdated(Tab updatedTab) {
            int index = mModel.indexFromId(updatedTab.getId());
            if (index == TabModel.INVALID_TAB_INDEX) return;
            mModel.get(index).model.set(TabProperties.TITLE, getLatestTitleForTab(updatedTab));
        }

        @Override
        public void onFaviconUpdated(Tab updatedTab, Bitmap icon) {
            updateFaviconForTab(updatedTab, icon);
        }
    };

    private final TabModelObserver mTabModelObserver;

    private TabGroupTitleEditor mTabGroupTitleEditor;

    private TabGroupModelFilter.Observer mTabGroupObserver;

    /**
     * Interface for implementing a {@link Runnable} that takes a tabId for a generic action.
     */
    public interface TabActionListener { void run(int tabId); }

    /**
     * Construct the Mediator with the given Models and observing hooks from the given
     * ChromeActivity.
     * @param context The context used to get some configuration information.
     * @param model The Model to keep state about a list of {@link Tab}s.
     * @param tabModelSelector {@link TabModelSelector} that will provide and receive signals about
     *                                                 the tabs concerned.
     * @param thumbnailProvider {@link ThumbnailProvider} to provide screenshot related details.
     * @param titleProvider {@link TitleProvider} for a given tab's title to show.
     * @param tabListFaviconProvider Provider for all favicon related drawables.
     * @param actionOnRelatedTabs Whether tab-related actions should be operated on all related
     *                            tabs.
     * @param createGroupButtonProvider {@link CreateGroupButtonProvider} to provide "Create group"
     *                                   button information. It's null when "Create group" is not
     *                                   possible.
     * @param selectionDelegateProvider Provider for a {@link SelectionDelegate} that is used for
     *                                  a selectable list. It's null when selection is not possible.
     * @param gridCardOnClickListenerProvider Provides the onClickListener for opening dialog when
     *                                        click on a grid card.
     * @param dialogHandler A handler to handle requests about updating TabGridDialog.
     * @param componentName This is a unique string to identify different components.
     * @param uiType The type of UI this mediator should be building.
     */
    public TabListMediator(Context context, TabListModel model, TabModelSelector tabModelSelector,
            @Nullable ThumbnailProvider thumbnailProvider, @Nullable TitleProvider titleProvider,
            TabListFaviconProvider tabListFaviconProvider, boolean actionOnRelatedTabs,
            @Nullable CreateGroupButtonProvider createGroupButtonProvider,
            @Nullable SelectionDelegateProvider selectionDelegateProvider,
            @Nullable GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            @Nullable TabGridDialogHandler dialogHandler, String componentName,
            @UiType int uiType) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mThumbnailProvider = thumbnailProvider;
        mModel = model;
        mTabListFaviconProvider = tabListFaviconProvider;
        mComponentName = componentName;
        mTitleProvider = titleProvider != null ? titleProvider : Tab::getTitle;
        mCreateGroupButtonProvider = createGroupButtonProvider;
        mSelectionDelegateProvider = selectionDelegateProvider;
        mGridCardOnClickListenerProvider = gridCardOnClickListenerProvider;
        mTabGridDialogHandler = dialogHandler;
        mActionsOnAllRelatedTabs = actionOnRelatedTabs;
        mUiType = uiType;

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                mNextTabId = Tab.INVALID_TAB_ID;
                if (tab.getId() == lastId) return;
                int oldIndex = mModel.indexFromId(lastId);
                if (oldIndex != TabModel.INVALID_TAB_INDEX) {
                    mModel.get(oldIndex).model.set(TabProperties.IS_SELECTED, false);
                }
                int newIndex = mModel.indexFromId(tab.getId());
                if (newIndex == TabModel.INVALID_TAB_INDEX) return;

                mModel.get(newIndex).model.set(TabProperties.IS_SELECTED, true);
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                onTabAdded(tab, !mActionsOnAllRelatedTabs);
                if (sTabClosedFromMapTabClosedFromMap.containsKey(tab.getId())) {
                    @TabClosedFrom
                    int from = sTabClosedFromMapTabClosedFromMap.get(tab.getId());
                    switch (from) {
                        case TabClosedFrom.TAB_STRIP:
                            RecordUserAction.record("TabStrip.UndoCloseTab");
                            break;
                        case TabClosedFrom.GRID_TAB_SWITCHER:
                            RecordUserAction.record("GridTabSwitch.UndoCloseTab");
                            break;
                        case TabClosedFrom.GRID_TAB_SWITCHER_GROUP:
                            RecordUserAction.record("GridTabSwitcher.UndoCloseTabGroup");
                            break;
                        default:
                            assert false
                                : "tabClosureUndone for tab that closed from an unknown UI";
                    }
                    sTabClosedFromMapTabClosedFromMap.remove(tab.getId());
                }
            }

            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                if (!mTabRestoreCompleted) return;
                onTabAdded(tab, !mActionsOnAllRelatedTabs);
                if (type == TabLaunchType.FROM_RESTORE && mActionsOnAllRelatedTabs) {
                    // When tab is restored after restoring stage (e.g. exiting multi-window mode),
                    // we need to update related property models.
                    TabModelFilter filter = mTabModelSelector.getTabModelFilterProvider()
                                                    .getCurrentTabModelFilter();
                    int index = filter.indexOf(tab);
                    if (index == TabList.INVALID_TAB_INDEX) return;
                    Tab currentGroupSelectedTab = filter.getTabAt(index);

                    assert mModel.indexFromId(currentGroupSelectedTab.getId()) == index;

                    updateTab(index, currentGroupSelectedTab,
                            mModel.get(index).model.get(TabProperties.IS_SELECTED), false, false);
                }
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                if (mModel.indexFromId(tab.getId()) == TabModel.INVALID_TAB_INDEX) return;
                mModel.removeAt(mModel.indexFromId(tab.getId()));
            }

            @Override
            public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                if (mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter()
                                instanceof TabGroupModelFilter) {
                    return;
                }
                onTabMoved(newIndex, curIndex);
            }

            @Override
            public void tabRemoved(Tab tab) {
                if (mModel.indexFromId(tab.getId()) == TabModel.INVALID_TAB_INDEX) return;
                mModel.removeAt(mModel.indexFromId(tab.getId()));
            }

            @Override
            public void restoreCompleted() {
                mTabRestoreCompleted = true;
            }
        };

        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        if (mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter) {
            mTabGroupObserver = new EmptyTabGroupModelFilterObserver() {
                @Override
                public void didMoveWithinGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    if (tabModelNewIndex == tabModelOldIndex) return;

                    int curPosition = mModel.indexFromId(movedTab.getId());
                    TabModel tabModel = mTabModelSelector.getCurrentModel();

                    if (!isValidMovePosition(curPosition)) return;

                    Tab destinationTab = tabModel.getTabAt(tabModelNewIndex > tabModelOldIndex
                                    ? tabModelNewIndex - 1
                                    : tabModelNewIndex + 1);

                    int newPosition = mModel.indexFromId(destinationTab.getId());
                    if (!isValidMovePosition(newPosition)) return;
                    mModel.move(curPosition, newPosition);
                }

                @Override
                public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                    assert !(mActionsOnAllRelatedTabs && mTabGridDialogHandler != null);
                    TabGroupModelFilter filter =
                            (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                                    .getCurrentTabModelFilter();
                    boolean isUngroupingLastTabInGroup =
                            filter.getTabAt(prevFilterIndex).getId() == movedTab.getId();
                    if (mActionsOnAllRelatedTabs) {
                        if (isUngroupingLastTabInGroup) {
                            return;
                        }
                        Tab currentSelectedTab = mTabModelSelector.getCurrentTab();
                        int index = TabModelUtils.getTabIndexById(
                                mTabModelSelector.getTabModelFilterProvider()
                                        .getCurrentTabModelFilter(),
                                movedTab.getId());
                        addTabInfoToModel(
                                movedTab, index, currentSelectedTab.getId() == movedTab.getId());
                        boolean isSelected = mTabModelSelector.getCurrentTabId()
                                == filter.getTabAt(prevFilterIndex).getId();
                        updateTab(prevFilterIndex, filter.getTabAt(prevFilterIndex), isSelected,
                                true, false);
                    } else {
                        int curIndex = mModel.indexFromId(movedTab.getId());
                        if (!isValidMovePosition(curIndex)) return;
                        mModel.removeAt(curIndex);
                        if (mTabGridDialogHandler != null) {
                            mTabGridDialogHandler.updateDialogContent(isUngroupingLastTabInGroup
                                            ? Tab.INVALID_TAB_ID
                                            : filter.getTabAt(prevFilterIndex).getId());
                        }
                    }
                }

                @Override
                public void didMergeTabToGroup(Tab movedTab, int selectedTabIdInGroup) {
                    if (!mActionsOnAllRelatedTabs) return;
                    Pair<Integer, Integer> positions =
                            mModel.getIndexesForMergeToGroup(mTabModelSelector.getCurrentModel(),
                                    getRelatedTabsForId(movedTab.getId()));
                    int srcIndex = positions.second;
                    int desIndex = positions.first;

                    if (!isValidMovePosition(srcIndex) || !isValidMovePosition(desIndex)) return;
                    mModel.removeAt(srcIndex);
                    if (getRelatedTabsForId(movedTab.getId()).size() == 2) {
                        // When users use drop-to-merge to create a group.
                        RecordUserAction.record("TabGroup.Created.DropToMerge");
                    } else {
                        RecordUserAction.record("TabGrid.Drag.DropToMerge");
                    }

                    desIndex = srcIndex > desIndex ? desIndex : desIndex - 1;
                    Tab newSelectedTab = mTabModelSelector.getTabModelFilterProvider()
                                                 .getCurrentTabModelFilter()
                                                 .getTabAt(desIndex);
                    boolean isSelected = mTabModelSelector.getCurrentTab() == newSelectedTab;
                    updateTab(desIndex, newSelectedTab, isSelected, true, false);
                }

                @Override
                public void didMoveTabGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    if (!mActionsOnAllRelatedTabs || tabModelNewIndex == tabModelOldIndex) return;
                    TabGroupModelFilter filter =
                            (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                                    .getCurrentTabModelFilter();
                    List<Tab> relatedTabs = getRelatedTabsForId(movedTab.getId());
                    Tab currentGroupSelectedTab =
                            TabGroupUtils.getSelectedTabInGroupForTab(mTabModelSelector, movedTab);
                    TabModel tabModel = mTabModelSelector.getCurrentModel();
                    int curPosition = mModel.indexFromId(currentGroupSelectedTab.getId());
                    if (curPosition == TabModel.INVALID_TAB_INDEX) {
                        // Sync TabListModel with updated TabGroupModelFilter.
                        int indexToUpdate = filter.indexOf(tabModel.getTabAt(tabModelOldIndex));
                        mModel.updateTabListModelIdForGroup(currentGroupSelectedTab, indexToUpdate);
                        curPosition = mModel.indexFromId(currentGroupSelectedTab.getId());
                    }
                    if (!isValidMovePosition(curPosition)) return;

                    // Find the tab which was in the destination index before this move. Use that
                    // tab to figure out the new position.
                    int destinationTabIndex = tabModelNewIndex > tabModelOldIndex
                            ? tabModelNewIndex - relatedTabs.size()
                            : tabModelNewIndex + 1;
                    Tab destinationTab = tabModel.getTabAt(destinationTabIndex);
                    Tab destinationGroupSelectedTab = TabGroupUtils.getSelectedTabInGroupForTab(
                            mTabModelSelector, destinationTab);
                    int newPosition = mModel.indexFromId(destinationGroupSelectedTab.getId());
                    if (newPosition == TabModel.INVALID_TAB_INDEX) {
                        int indexToUpdate = filter.indexOf(destinationTab)
                                + (tabModelNewIndex > tabModelOldIndex ? 1 : -1);
                        mModel.updateTabListModelIdForGroup(
                                destinationGroupSelectedTab, indexToUpdate);
                        newPosition = mModel.indexFromId(destinationGroupSelectedTab.getId());
                    }
                    if (!isValidMovePosition(newPosition)) return;

                    mModel.move(curPosition, newPosition);
                }

                @Override
                public void didCreateGroup(
                        List<Tab> tabs, List<Integer> tabOriginalIndex, boolean isSameGroup) {}
            };

            ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                     false))
                    .addTabGroupObserver(mTabGroupObserver);
            ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                     true))
                    .addTabGroupObserver(mTabGroupObserver);
        }

        // TODO(meiliang): follow up with unit tests to test the close signal is sent correctly with
        // the recommendedNextTab.
        mTabClosedListener = new TabActionListener() {
            @Override
            public void run(int tabId) {
                // TODO(crbug.com/990698): Consider disabling all touch events during animation.
                if (mModel.indexFromId(tabId) == TabModel.INVALID_TAB_INDEX) return;

                RecordUserAction.record("MobileTabClosed." + mComponentName);

                if (mActionsOnAllRelatedTabs) {
                    List<Tab> related = getRelatedTabsForId(tabId);
                    if (related.size() > 1) {
                        onGroupClosedFrom(tabId);
                        mTabModelSelector.getCurrentModel().closeMultipleTabs(related, true);
                        return;
                    }
                }
                onTabClosedFrom(tabId, mComponentName);

                Tab currentTab = mTabModelSelector.getCurrentTab();
                Tab closingTab =
                        TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId);
                Tab nextTab = currentTab == closingTab ? getNextTab(tabId) : null;

                mTabModelSelector.getCurrentModel().closeTab(
                        closingTab, nextTab, false, false, true);
            }

            private Tab getNextTab(int closingTabId) {
                int closingTabIndex = mModel.indexFromId(closingTabId);

                if (closingTabIndex == TabModel.INVALID_TAB_INDEX) {
                    assert false;
                    return null;
                }

                int nextTabId = Tab.INVALID_TAB_ID;
                if (mModel.size() > 1) {
                    nextTabId = closingTabIndex == 0
                            ? mModel.get(closingTabIndex + 1).model.get(TabProperties.TAB_ID)
                            : mModel.get(closingTabIndex - 1).model.get(TabProperties.TAB_ID);
                }

                return TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), nextTabId);
            }
        };

        if (FeatureUtilities.isTabGroupsAndroidUiImprovementsEnabled()) {
            mTabGroupTitleEditor = new TabGroupTitleEditor(mTabModelSelector) {
                @Override
                protected void updateTabGroupTitle(Tab tab, String title) {
                    // Only update title in PropertyModel for tab switcher.
                    if (!mActionsOnAllRelatedTabs) return;
                    Tab currentGroupSelectedTab =
                            TabGroupUtils.getSelectedTabInGroupForTab(mTabModelSelector, tab);
                    int index = mModel.indexFromId(currentGroupSelectedTab.getId());
                    if (index == TabModel.INVALID_TAB_INDEX) return;
                    mModel.get(index).model.set(TabProperties.TITLE, title);
                }

                @Override
                protected void deleteTabGroupTitle(int tabRootId) {
                    TabGroupUtils.deleteTabGroupTitle(tabRootId);
                }

                @Override
                protected String getTabGroupTitle(int tabRootId) {
                    return TabGroupUtils.getTabGroupTitle(tabRootId);
                }

                @Override
                protected void storeTabGroupTitle(int tabRootId, String title) {
                    TabGroupUtils.storeTabGroupTitle(tabRootId, title);
                }
            };
        }
        mTabGridItemTouchHelperCallback =
                new TabGridItemTouchHelperCallback(mModel, mTabModelSelector, mTabClosedListener,
                        mTabGridDialogHandler, mComponentName, mActionsOnAllRelatedTabs);
    }

    private void onTabClosedFrom(int tabId, String fromComponent) {
        @TabClosedFrom
        int from;
        if (fromComponent.equals(TabGroupUiCoordinator.COMPONENT_NAME)) {
            from = TabClosedFrom.TAB_STRIP;
        } else if (fromComponent.equals(TabSwitcherCoordinator.COMPONENT_NAME)) {
            from = TabClosedFrom.GRID_TAB_SWITCHER;
        } else {
            Log.w(TAG, "Attempting to close tab from Unknown UI");
            return;
        }
        sTabClosedFromMapTabClosedFromMap.put(tabId, from);
    }

    private void onGroupClosedFrom(int tabId) {
        sTabClosedFromMapTabClosedFromMap.put(tabId, TabClosedFrom.GRID_TAB_SWITCHER_GROUP);
    }

    void setActionOnAllRelatedTabsForTesting(boolean actionOnAllRelatedTabs) {
        mActionsOnAllRelatedTabs = actionOnAllRelatedTabs;
    }

    private List<Tab> getRelatedTabsForId(int id) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(id);
    }

    private int getIndexOfTab(Tab tab, boolean onlyShowRelatedTabs) {
        int index;
        if (onlyShowRelatedTabs) {
            if (mModel.size() == 0) return TabList.INVALID_TAB_INDEX;
            List<Tab> related = getRelatedTabsForId(mModel.get(0).model.get(TabProperties.TAB_ID));
            index = related.indexOf(tab);
            if (index == -1) return TabList.INVALID_TAB_INDEX;
        } else {
            index = TabModelUtils.getTabIndexById(
                    mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                    tab.getId());
            // TODO(wychen): the title (tab count in the group) is wrong when it's not the last
            //  tab added in the group.
        }
        return index;
    }

    private void onTabAdded(Tab tab, boolean onlyShowRelatedTabs) {
        int index = getIndexOfTab(tab, onlyShowRelatedTabs);
        if (index == TabList.INVALID_TAB_INDEX) return;

        addTabInfoToModel(tab, index, mTabModelSelector.getCurrentTab() == tab);
    }

    private void onTabMoved(int newIndex, int curIndex) {
        // Handle move without groups enabled.
        if (mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof EmptyTabModelFilter) {
            if (!isValidMovePosition(curIndex) || !isValidMovePosition(newIndex)) return;
            mModel.move(curIndex, newIndex);
        }
    }

    private boolean isValidMovePosition(int position) {
        return position != TabModel.INVALID_TAB_INDEX && position < mModel.size();
    }

    /**
     * Hide the blue border for selected tab for the Tab-to-Grid resizing stage.
     * The selected border should re-appear in the final fading-in stage.
     */
    void prepareOverview() {
        if (!TabFeatureUtilities.isTabToGtsAnimationEnabled()
                || !mTabModelSelector.getTabModelFilterProvider()
                            .getCurrentTabModelFilter()
                            .isTabModelRestored()) {
            return;
        }

        assert mVisible;
        int count = 0;
        for (int i = 0; i < mModel.size(); i++) {
            if (mModel.get(i).model.get(TabProperties.IS_SELECTED)) count++;
            mModel.get(i).model.set(TabProperties.IS_SELECTED, false);
        }
        assert (count == 1 || mModel.size() == 0)
            : "There should be exactly one selected tab or no tabs at all when calling "
              + "TabListMediator.prepareOverview()";
    }

    private boolean areTabsUnchanged(@Nullable List<Tab> tabs) {
        if (tabs == null) {
            return mModel.size() == 0;
        }
        if (tabs.size() != mModel.size()) return false;
        for (int i = 0; i < tabs.size(); i++) {
            if (tabs.get(i).getId() != mModel.get(i).model.get(TabProperties.TAB_ID)) return false;
        }
        return true;
    }

    /**
     * Initialize the component with a list of tabs to show in a grid.
     * @param tabs The list of tabs to be shown.
     * @param quickMode Whether to skip capturing the selected live tab for the thumbnail.
     * @param mruMode Whether to sort the Tabs in MRU order.
     * @return Whether the {@link TabListRecyclerView} can be shown quickly.
     */
    boolean resetWithListOfTabs(@Nullable List<Tab> tabs, boolean quickMode, boolean mruMode) {
        List<Tab> tabsList = tabs;
        if (tabs != null && mruMode) {
            // Make a copy to sort since the input may be unmodifiable.
            tabsList = new ArrayList<>(tabs);
            Collections.sort(tabsList, LAST_SHOWN_COMPARATOR);
        }

        mVisible = tabsList != null;
        if (areTabsUnchanged(tabsList)) {
            if (tabsList == null) return true;

            for (int i = 0; i < tabsList.size(); i++) {
                Tab tab = tabsList.get(i);
                boolean isSelected = mTabModelSelector.getCurrentTab() == tab;
                updateTab(i, tab, isSelected, false, quickMode);
            }
            return true;
        }
        mModel.set(new ArrayList<>());
        if (tabsList == null) {
            return true;
        }
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) return false;

        for (int i = 0; i < tabsList.size(); i++) {
            addTabInfoToModel(
                    tabsList.get(i), i, isSelectedTab(tabsList.get(i).getId(), currentTab.getId()));
        }
        return false;
    }

    void postHiding() {
        mVisible = false;
    }

    private boolean isSelectedTab(int tabId, int tabModelSelectedTabId) {
        SelectionDelegate<Integer> selectionDelegate = getTabSelectionDelegate();
        if (selectionDelegate == null) {
            return tabId == tabModelSelectedTabId;
        } else {
            return selectionDelegate.isItemSelected(tabId);
        }
    }

    /**
     * @see TabSwitcherMediator.ResetHandler#softCleanup
     */
    void softCleanup() {
        assert !mVisible;
        for (int i = 0; i < mModel.size(); i++) {
            mModel.get(i).model.set(TabProperties.THUMBNAIL_FETCHER, null);
        }
    }

    private void updateTab(
            int index, Tab tab, boolean isSelected, boolean isUpdatingId, boolean quickMode) {
        if (index < 0 || index >= mModel.size()) return;
        if (isUpdatingId) {
            mModel.get(index).model.set(TabProperties.TAB_ID, tab.getId());
        } else {
            assert mModel.get(index).model.get(TabProperties.TAB_ID) == tab.getId();
        }

        TabActionListener tabSelectedListener;
        if (mGridCardOnClickListenerProvider == null || getRelatedTabsForId(tab.getId()).size() == 1
                || !mActionsOnAllRelatedTabs) {
            tabSelectedListener = mTabSelectedListener;
        } else {
            tabSelectedListener = mGridCardOnClickListenerProvider.openTabGridDialog(tab);

            if (tabSelectedListener == null) {
                tabSelectedListener = mTabSelectedListener;
            }
        }
        mModel.get(index).model.set(TabProperties.TAB_SELECTED_LISTENER, tabSelectedListener);
        mModel.get(index).model.set(
                TabProperties.CREATE_GROUP_LISTENER, getCreateGroupButtonListener(tab, isSelected));
        mModel.get(index).model.set(TabProperties.IS_SELECTED, isSelected);
        mModel.get(index).model.set(TabProperties.TITLE, getLatestTitleForTab(tab));

        updateFaviconForTab(tab, null);
        boolean forceUpdate = isSelected && !quickMode;
        if (mThumbnailProvider != null && mVisible
                && (mModel.get(index).model.get(TabProperties.THUMBNAIL_FETCHER) == null
                        || forceUpdate || isUpdatingId)) {
            ThumbnailFetcher callback = new ThumbnailFetcher(mThumbnailProvider, tab, forceUpdate,
                    forceUpdate && !TabFeatureUtilities.isTabToGtsAnimationEnabled());
            mModel.get(index).model.set(TabProperties.THUMBNAIL_FETCHER, callback);
        }
    }

    /**
     * @return The callback that hosts the logic for swipe and drag related actions.
     */
    ItemTouchHelper.SimpleCallback getItemTouchHelperCallback(final float swipeToDismissThreshold,
            final float mergeThreshold, final float ungroupThreshold, final Profile profile) {
        mTabGridItemTouchHelperCallback.setupCallback(
                swipeToDismissThreshold, mergeThreshold, ungroupThreshold, profile);
        return mTabGridItemTouchHelperCallback;
    }

    void registerOrientationListener(GridLayoutManager manager) {
        // TODO(yuezhanggg): Try to dynamically determine span counts based on screen width,
        // minimum card width and padding.
        mComponentCallbacks = new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration newConfig) {
                updateSpanCountForOrientation(manager, newConfig.orientation);
            }

            @Override
            public void onLowMemory() {}
        };
        mContext.registerComponentCallbacks(mComponentCallbacks);
    }

    /**
     * Update the grid layout span count base on orientation.
     * @param manager     The {@link GridLayoutManager} used to update the span count.
     * @param orientation The orientation base on which we update the span count.
     */
    void updateSpanCountForOrientation(GridLayoutManager manager, int orientation) {
        // When in multi-window mode, the span count is fixed to 2 to keep tab card size reasonable.
        if (MultiWindowUtils.getInstance().isInMultiWindowMode((Activity) mContext)) {
            manager.setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_PORTRAIT);
            return;
        }
        manager.setSpanCount(orientation == Configuration.ORIENTATION_PORTRAIT
                        ? TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_PORTRAIT
                        : TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_LANDSCAPE);
    }

    /**
     * Exposes a {@link TabGroupTitleEditor} to modify the title of a tab group.
     * @return The {@link TabGroupTitleEditor} used to modify the title of a tab group.
     */
    @Nullable
    TabGroupTitleEditor getTabGroupTitleEditor() {
        return mTabGroupTitleEditor;
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (tabModel != null) {
            for (int i = 0; i < tabModel.getCount(); i++) {
                tabModel.getTabAt(i).removeObserver(mTabObserver);
            }
        }
        if (mTabModelObserver != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);
        }
        if (mTabGroupObserver != null) {
            ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                     false))
                    .removeTabGroupObserver(mTabGroupObserver);
            ((TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                     true))
                    .removeTabGroupObserver(mTabGroupObserver);
        }
        if (mComponentCallbacks != null) {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
        }
        if (mTabGroupTitleEditor != null) {
            mTabGroupTitleEditor.destroy();
        }
    }

    private void addTabInfoToModel(final Tab tab, int index, boolean isSelected) {
        boolean showIPH = false;
        if (mActionsOnAllRelatedTabs && !mShownIPH) {
            showIPH = getRelatedTabsForId(tab.getId()).size() > 1;
        }
        TabActionListener tabSelectedListener;
        if (mGridCardOnClickListenerProvider == null || getRelatedTabsForId(tab.getId()).size() == 1
                || !mActionsOnAllRelatedTabs) {
            tabSelectedListener = mTabSelectedListener;
        } else {
            tabSelectedListener = mGridCardOnClickListenerProvider.openTabGridDialog(tab);

            if (tabSelectedListener == null) {
                tabSelectedListener = mTabSelectedListener;
            }
        }

        int selectedTabBackgroundDrawableId = tab.isIncognito()
                ? R.drawable.selected_tab_background_incognito
                : R.drawable.selected_tab_background;

        int tabstripFaviconBackgroundDrawableId = tab.isIncognito()
                ? R.color.favicon_background_color_incognito
                : R.color.favicon_background_color;

        PropertyModel tabInfo =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(TabProperties.TITLE, getLatestTitleForTab(tab))
                        .with(TabProperties.FAVICON,
                                mTabListFaviconProvider.getDefaultFaviconDrawable(
                                        tab.isIncognito()))
                        .with(TabProperties.IS_SELECTED, isSelected)
                        .with(TabProperties.IPH_PROVIDER, showIPH ? mIphProvider : null)
                        .with(TabProperties.TAB_SELECTED_LISTENER, tabSelectedListener)
                        .with(TabProperties.TAB_CLOSED_LISTENER, mTabClosedListener)
                        .with(TabProperties.CREATE_GROUP_LISTENER,
                                getCreateGroupButtonListener(tab, isSelected))
                        .with(TabProperties.ALPHA, 1f)
                        .with(TabProperties.CARD_ANIMATION_STATUS,
                                ClosableTabGridView.AnimationStatus.CARD_RESTORE)
                        .with(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER,
                                mSelectableTabOnClickListener)
                        .with(TabProperties.TAB_SELECTION_DELEGATE, getTabSelectionDelegate())
                        .with(TabProperties.IS_INCOGNITO, tab.isIncognito())
                        .with(TabProperties.SELECTED_TAB_BACKGROUND_DRAWABLE_ID,
                                selectedTabBackgroundDrawableId)
                        .with(TabProperties.TABSTRIP_FAVICON_BACKGROUND_COLOR_ID,
                                tabstripFaviconBackgroundDrawableId)
                        .build();

        if (mUiType == UiType.SELECTABLE) {
            // Incognito in both light/dark theme is the same as non-incognito mode in dark theme.
            // Non-incognito mode and incognito in both light/dark themes in dark theme all look
            // dark.
            ColorStateList checkedDrawableColorList = AppCompatResources.getColorStateList(mContext,
                    tab.isIncognito() ? R.color.default_icon_color_dark
                                      : R.color.default_icon_color_inverse);
            ColorStateList actionButtonBackgroundColorList =
                    AppCompatResources.getColorStateList(mContext,
                            tab.isIncognito() ? R.color.default_icon_color_white
                                              : R.color.default_icon_color);
            // TODO(995876): Update color modern_blue_300 to active_color_dark when the associated
            // bug is landed.
            ColorStateList actionbuttonSelectedBackgroundColorList =
                    AppCompatResources.getColorStateList(mContext,
                            tab.isIncognito() ? R.color.modern_blue_300
                                              : R.color.light_active_color);

            tabInfo.set(TabProperties.CHECKED_DRAWABLE_STATE_LIST, checkedDrawableColorList);
            tabInfo.set(TabProperties.SELECTABLE_TAB_ACTION_BUTTON_BACKGROUND,
                    actionButtonBackgroundColorList);
            tabInfo.set(TabProperties.SELECTABLE_TAB_ACTION_BUTTON_SELECTED_BACKGROUND,
                    actionbuttonSelectedBackgroundColorList);
        }

        if (index >= mModel.size()) {
            mModel.add(new SimpleRecyclerViewAdapter.ListItem(mUiType, tabInfo));
        } else {
            mModel.add(index, new SimpleRecyclerViewAdapter.ListItem(mUiType, tabInfo));
        }

        updateFaviconForTab(tab, null);

        if (mThumbnailProvider != null && mVisible) {
            ThumbnailFetcher callback = new ThumbnailFetcher(mThumbnailProvider, tab, isSelected,
                    isSelected && !TabFeatureUtilities.isTabToGtsAnimationEnabled());
            tabInfo.set(TabProperties.THUMBNAIL_FETCHER, callback);
        }
        tab.addObserver(mTabObserver);
    }

    @Nullable
    private SelectionDelegate<Integer> getTabSelectionDelegate() {
        return mSelectionDelegateProvider == null
                ? null
                : mSelectionDelegateProvider.getSelectionDelegate();
    }

    @Nullable
    private TabActionListener getCreateGroupButtonListener(Tab tab, boolean isSelected) {
        TabActionListener createGroupButtonOnClickListener = null;
        if (isSelected && mCreateGroupButtonProvider != null) {
            createGroupButtonOnClickListener =
                    mCreateGroupButtonProvider.getCreateGroupButtonOnClickListener(tab);
        }
        return createGroupButtonOnClickListener;
    }

    @VisibleForTesting
    String getLatestTitleForTab(Tab tab) {
        String originalTitle = mTitleProvider.getTitle(tab);
        if (!mActionsOnAllRelatedTabs || mTabGroupTitleEditor == null) return originalTitle;
        // If the group degrades to a single tab, delete the stored title.
        if (getRelatedTabsForId(tab.getId()).size() <= 1) {
            return originalTitle;
        }
        String storedTitle = mTabGroupTitleEditor.getTabGroupTitle(tab.getRootId());
        return storedTitle == null ? originalTitle : storedTitle;
    }

    int selectedTabId() {
        if (mNextTabId != Tab.INVALID_TAB_ID) {
            return mNextTabId;
        }

        return mTabModelSelector.getCurrentTabId();
    }

    /**
     * Find the index of the given tab in the {@link TabListRecyclerView}.
     * Note that Tabs may have different index in {@link TabListRecyclerView} and {@link
     * TabModelSelector}, like when {@link resetWithListOfTabs} above is called with MRU mode
     * enabled.
     * @param tabId The given Tab id.
     * @return The index of the Tab in the {@link TabListRecyclerView}.
     */
    int indexOfTab(int tabId) {
        return mModel.indexFromId(tabId);
    }

    @VisibleForTesting
    void updateFaviconForTab(Tab tab, @Nullable Bitmap icon) {
        int modelIndex = mModel.indexFromId(tab.getId());
        if (modelIndex == Tab.INVALID_TAB_ID) return;
        // For tab group card in grid tab switcher, the favicon is set to be null.
        if (mActionsOnAllRelatedTabs && getRelatedTabsForId(tab.getId()).size() > 1) {
            mModel.get(modelIndex).model.set(TabProperties.FAVICON, null);
            return;
        }
        // If there is an available icon, we fetch favicon synchronously; otherwise asynchronously.
        if (icon != null) {
            Drawable drawable = mTabListFaviconProvider.getFaviconForUrlSync(
                    tab.getUrl(), tab.isIncognito(), icon);
            mModel.get(modelIndex).model.set(TabProperties.FAVICON, drawable);
            return;
        }
        Callback<Drawable> faviconCallback = drawable -> {
            assert drawable != null;
            // Need to re-get the index because the original index can be stale when callback is
            // triggered.
            int index = mModel.indexFromId(tab.getId());
            if (index != Tab.INVALID_TAB_ID && drawable != null) {
                mModel.get(index).model.set(TabProperties.FAVICON, drawable);
            }
        };
        mTabListFaviconProvider.getFaviconForUrlAsync(
                tab.getUrl(), tab.isIncognito(), faviconCallback);
    }

    @VisibleForTesting
    void setTabRestoreCompletedForTesting(boolean isRestored) {
        mTabRestoreCompleted = isRestored;
    }

    /**
     * Inserts a special {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} at given index of
     * the current {@link TabListModel}.
     *
     * @param index The index of the {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} to be
     *              inserted.
     * @param uiType The view type the model will bind to.
     * @param model The model that will be bound to a view.
     */
    void addSpecialItemToModel(int index, @UiType int uiType, PropertyModel model) {
        mModel.add(index, new SimpleRecyclerViewAdapter.ListItem(uiType, model));
    }
}
