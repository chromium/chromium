// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** A mostly stubbed implementation of {@link TabGroupModelFilter} for unit tests. */
@NullMarked
public class StubTabGroupModelFilterImpl implements TabGroupModelFilterInternal {
    private final TabModelInternal mTabModel;
    private final TabUngrouper mTabUngrouper;

    public StubTabGroupModelFilterImpl(TabModelInternal tabModel, TabUngrouper tabUngrouper) {
        mTabModel = tabModel;
        mTabUngrouper = tabUngrouper;
    }

    @Override
    public void destroy() {}

    @Override
    public boolean closeTabs(TabClosureParams tabClosureParams) {
        return mTabModel.closeTabs(tabClosureParams);
    }

    @Override
    public void markTabStateInitialized() {}

    @Override
    public void moveTabOutOfGroupInDirection(int sourceTabId, boolean trailing) {}

    @Override
    public void addTabGroupObserver(TabGroupModelFilterObserver observer) {}

    @Override
    public void removeTabGroupObserver(TabGroupModelFilterObserver observer) {}

    @Override
    public TabModel getTabModel() {
        return mTabModel;
    }

    @Override
    public List<Tab> getRepresentativeTabList() {
        List<Tab> tabs = new ArrayList<>();
        for (Tab tab : mTabModel) {
            tabs.add(tab);
        }
        return tabs;
    }

    @Override
    public int getIndividualTabAndGroupCount() {
        return mTabModel.getCount();
    }

    @Override
    public int getCurrentRepresentativeTabIndex() {
        return TabModel.INVALID_TAB_INDEX;
    }

    @Override
    public @Nullable Tab getCurrentRepresentativeTab() {
        int index = mTabModel.index();
        if (index == TabModel.INVALID_TAB_INDEX) return null;
        return mTabModel.getTabAt(index);
    }

    @Override
    public @Nullable Tab getRepresentativeTabAt(int index) {
        return mTabModel.getTabAt(index);
    }

    @Override
    public int representativeIndexOf(@Nullable Tab tab) {
        return mTabModel.indexOf(tab);
    }

    @Override
    public int getTabGroupCount() {
        return 0;
    }

    @Override
    public int getTabCountForGroup(@Nullable Token tabGroupId) {
        return 0;
    }

    @Override
    public boolean tabGroupExists(@Nullable Token tabGroupId) {
        return false;
    }

    @Override
    public List<Tab> getRelatedTabList(@TabId int tabId) {
        return Collections.singletonList(mTabModel.getTabById(tabId));
    }

    @Override
    public List<Tab> getTabsInGroup(@Nullable Token tabGroupId) {
        return Collections.emptyList();
    }

    @Override
    public boolean isTabInTabGroup(Tab tab) {
        return false;
    }

    @Override
    public int getIndexOfTabInGroup(Tab tab) {
        return TabModel.INVALID_TAB_INDEX;
    }

    @Override
    public @TabId int getGroupLastShownTabId(@Nullable Token tabGroupId) {
        return Tab.INVALID_TAB_ID;
    }

    @Override
    public void moveRelatedTabs(@TabId int id, int newIndex) {
        mTabModel.moveTab(id, newIndex);
    }

    @Override
    public boolean willMergingCreateNewGroup(List<Tab> tabsToMerge) {
        return false;
    }

    @Override
    public void createSingleTabGroup(Tab tab) {}

    @Override
    public void createTabGroupForTabGroupSync(List<Tab> tabs, Token tabGroupId) {}

    @Override
    public void mergeTabsToGroup(
            @TabId int sourceTabId, @TabId int destinationTabId, boolean skipUpdateTabModel) {}

    @Override
    public void mergeListOfTabsToGroup(
            List<Tab> tabs,
            Tab destinationTab,
            @Nullable Integer indexInGroup,
            @MergeNotificationType int notify) {}

    @Override
    public TabUngrouper getTabUngrouper() {
        return mTabUngrouper;
    }

    @Override
    public void performUndoGroupOperation(UndoGroupMetadata undoGroupMetadata) {}

    @Override
    public void undoGroupOperationExpired(UndoGroupMetadata undoGroupMetadata) {}

    @Override
    public Set<Token> getAllTabGroupIds() {
        return Collections.emptySet();
    }

    @Override
    public int getValidPosition(Tab tab, int proposedPosition) {
        return proposedPosition;
    }

    @Override
    public boolean isTabModelRestored() {
        return true;
    }

    @Override
    public boolean isTabGroupHiding(@Nullable Token tabGroupId) {
        return false;
    }

    @Override
    public LazyOneshotSupplier<Set<Token>> getLazyAllTabGroupIds(
            List<Tab> tabsToExclude, boolean includePendingClosures) {
        return LazyOneshotSupplier.fromValue(Collections.emptySet());
    }

    @Override
    public @Nullable String getTabGroupTitle(Token tabGroupId) {
        return null;
    }

    @Override
    public @Nullable String getTabGroupTitle(Tab groupedTab) {
        return null;
    }

    @Override
    public void setTabGroupTitle(Token tabGroupId, @Nullable String title) {}

    @Override
    public void deleteTabGroupTitle(Token tabGroupId) {}

    @Override
    public int getTabGroupColor(Token tabGroupId) {
        return TabGroupColorUtils.INVALID_COLOR_ID;
    }

    @Override
    public @TabGroupColorId int getTabGroupColorWithFallback(Token tabGroupId) {
        return TabGroupColorId.GREY;
    }

    @Override
    public @TabGroupColorId int getTabGroupColorWithFallback(Tab groupedTab) {
        return TabGroupColorId.GREY;
    }

    @Override
    public void setTabGroupColor(Token tabGroupId, @TabGroupColorId int color) {}

    @Override
    public void deleteTabGroupColor(Token tabGroupId) {}

    @Override
    public boolean getTabGroupCollapsed(Token tabGroupId) {
        return false;
    }

    @Override
    public void setTabGroupCollapsed(Token tabGroupId, boolean isCollapsed, boolean animate) {}

    @Override
    public void deleteTabGroupCollapsed(Token tabGroupId) {}

    @Override
    public void addObserver(TabModelObserver observer) {
        mTabModel.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        mTabModel.removeObserver(observer);
    }
}
