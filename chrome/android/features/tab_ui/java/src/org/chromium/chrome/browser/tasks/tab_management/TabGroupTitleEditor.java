// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.EmptyTabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class hosts logic related to edit tab group title. Concrete class that extends this abstract
 * class needs to specify the title storage/fetching implementation as well as handle {@link
 * PropertyModel} update.
 */
public abstract class TabGroupTitleEditor {
    private final Context mContext;
    private final ObservableSupplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final TabModelObserver mTabModelObserver;
    private final TabGroupModelFilter.Observer mFilterObserver;
    private final ValueChangedCallback<TabModelFilter> mCurrentTabModelFilterObserver =
            new ValueChangedCallback<>(this::onTabModelFilterChanged);

    public TabGroupTitleEditor(
            Context context, ObservableSupplier<TabModelFilter> tabModelFilterSupplier) {
        mContext = context;
        mCurrentTabModelFilterSupplier = tabModelFilterSupplier;

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        int tabRootId = tab.getRootId();
                        // If the group becomes a single tab after closing or we are closing a
                        // group, delete the stored title.
                        if (((TabGroupModelFilter) mCurrentTabModelFilterSupplier.get())
                                        .getRelatedTabListForRootId(tabRootId)
                                        .size()
                                == 1) {
                            deleteTabGroupTitle(tabRootId);
                        }
                    }
                };

        mFilterObserver =
                new EmptyTabGroupModelFilterObserver() {
                    @Override
                    public void willMergeTabToGroup(Tab movedTab, int newRootId) {
                        String sourceGroupTitle = getTabGroupTitle(getRootId(movedTab));
                        String targetGroupTitle = getTabGroupTitle(newRootId);
                        if (sourceGroupTitle == null) return;
                        // If the target group has no title but the source group has a title,
                        // handover the stored title to the group after merge.
                        if (targetGroupTitle == null) {
                            storeTabGroupTitle(newRootId, sourceGroupTitle);
                        }
                    }

                    @Override
                    public void willMoveTabOutOfGroup(Tab movedTab, int newRootId) {
                        String title = getTabGroupTitle(getRootId(movedTab));
                        if (title == null) return;
                        // If the group size is 2, i.e. the group becomes a single tab after
                        // ungroup, delete the stored title.
                        if (mCurrentTabModelFilterSupplier
                                        .get()
                                        .getRelatedTabList(movedTab.getId())
                                        .size()
                                == 2) {
                            deleteTabGroupTitle(getRootId(movedTab));
                            return;
                        }
                        // If the root tab in group is moved out, re-assign the title to the new
                        // root tab in group.
                        if (getRootId(movedTab) != newRootId) {
                            deleteTabGroupTitle(getRootId(movedTab));
                            storeTabGroupTitle(newRootId, title);
                        }
                    }

                    private int getRootId(Tab tab) {
                        return tab.getRootId();
                    }
                };

        mCurrentTabModelFilterObserver.onResult(
                mCurrentTabModelFilterSupplier.addObserver(mCurrentTabModelFilterObserver));
    }

    /**
     * @param context Context for accessing resources.
     * @param numRelatedTabs The number of related tabs.
     * @return the default title for the tab group.
     */
    public static String getDefaultTitle(Context context, int numRelatedTabs) {
        return context.getResources()
                .getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder,
                        numRelatedTabs,
                        numRelatedTabs);
    }

    /**
     * @param newTitle the new title.
     * @param numRelatedTabs the number of related tabs in the group.
     * @return whether the newTitle is a match for the default string.
     */
    public boolean isDefaultTitle(String newTitle, int numRelatedTabs) {
        // TODO(crbug/1419842): Consider broadening this check for differing numbers of related
        // tabs. This is difficult due to this being a translated plural string.
        return newTitle.equals(getDefaultTitle(mContext, numRelatedTabs));
    }

    /**
     * This method uses {@code title} to update the {@link PropertyModel} of the group which
     * contains {@code tab}. Concrete class need to specify how to update title in
     * {@link PropertyModel}.
     *
     * @param tab     The {@link Tab} whose relevant tab group's title will be updated.
     * @param title   The tab group title to update.
     */
    protected abstract void updateTabGroupTitle(Tab tab, String title);

    /**
     * This method uses tab group root ID as a reference to store tab group title.
     *
     * @param tabRootId    The tab root ID of the tab group whose title will be stored.
     * @param title        The tab group title to store.
     */
    protected abstract void storeTabGroupTitle(int tabRootId, String title);

    /**
     * This method uses tab group root ID as a reference to delete specific stored tab group title.
     * @param tabRootId   The tab root ID whose related tab group title will be deleted.
     */
    protected abstract void deleteTabGroupTitle(int tabRootId);

    /**
     * This method uses tab group root ID to fetch stored tab group title.
     * @param tabRootId  The tab root ID whose related tab group title will be fetched.
     * @return The stored title of the related group.
     */
    protected abstract String getTabGroupTitle(int tabRootId);

    /** Destroy any members that needs clean up. */
    public void destroy() {
        removeTabModelFilterObservers(mCurrentTabModelFilterSupplier.get());
        mCurrentTabModelFilterSupplier.removeObserver(mCurrentTabModelFilterObserver);
    }

    private void onTabModelFilterChanged(TabModelFilter newFilter, TabModelFilter oldFilter) {
        removeTabModelFilterObservers(oldFilter);

        if (newFilter != null) {
            TabGroupModelFilter newGroupFilter = (TabGroupModelFilter) newFilter;
            newGroupFilter.addObserver(mTabModelObserver);
            newGroupFilter.addTabGroupObserver(mFilterObserver);
        }
    }

    private void removeTabModelFilterObservers(TabModelFilter filter) {
        if (filter != null) {
            TabGroupModelFilter groupFilter = (TabGroupModelFilter) filter;
            groupFilter.removeObserver(mTabModelObserver);
            groupFilter.removeTabGroupObserver(mFilterObserver);
        }
    }
}
