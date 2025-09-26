// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * An equivalent to {@link IncognitoTabModelImpl}, but for the {@link TabGroupModelFilter}. When the
 * last incognito tab is closed we fully tear down the associated profile and related {@link
 * TabCollectionTabModelImpl}. However, this class is long lasting and abstracts away this lifecycle
 * complexity to the outside world by always existing regardless of the existence of an OTR profile.
 *
 * <p>This class only exists if {@link TabCollectionTabModelImpl} is in use as the prior
 * implementation of {@link TabGroupModelFilterImpl} already fulfilled this role by wrapping the
 * {@link IncognitoTabModelImpl}.
 *
 * <p>Post {@link TabCollectionTabModelImpl} launching we could merge this class with {@link
 * IncognitoTabModelImpl}.
 */
@NullMarked
public class IncognitoTabGroupModelFilterImpl implements TabGroupModelFilterInternal {
    private final Callback<TabModelInternal> mDelegateModelObserver = this::setDelegateModel;

    private final ObserverList<TabGroupModelFilterObserver> mObservers = new ObserverList<>();
    private final IncognitoTabModelInternal mIncognitoTabModel;
    private final TabUngrouper mTabUngrouperProxy =
            new TabUngrouper() {
                @Override
                public void ungroupTabs(
                        List<Tab> tabs,
                        boolean trailing,
                        boolean allowDialog,
                        @Nullable TabModelActionListener listener) {
                    if (mCurrentFilter == null) return;
                    mCurrentFilter
                            .getTabUngrouper()
                            .ungroupTabs(tabs, trailing, allowDialog, listener);
                }

                @Override
                public void ungroupTabGroup(
                        Token tabGroupId,
                        boolean trailing,
                        boolean allowDialog,
                        @Nullable TabModelActionListener listener) {
                    if (mCurrentFilter == null) return;
                    mCurrentFilter
                            .getTabUngrouper()
                            .ungroupTabGroup(tabGroupId, trailing, allowDialog, listener);
                }
            };

    private @Nullable TabGroupModelFilterInternal mCurrentFilter;

    public IncognitoTabGroupModelFilterImpl(IncognitoTabModelInternal incognitoTabModel) {
        mIncognitoTabModel = incognitoTabModel;
        mIncognitoTabModel.addDelegateModelObserver(mDelegateModelObserver);
    }

    private void setDelegateModel(TabModelInternal tabModel) {
        if (tabModel instanceof TabGroupModelFilterInternal newFilter) {
            mCurrentFilter = newFilter;
            for (TabGroupModelFilterObserver obs : mObservers) {
                mCurrentFilter.addTabGroupObserver(obs);
            }
        } else if (tabModel instanceof EmptyTabModel) {
            mCurrentFilter = null;
        } else {
            assert false : "Not reached";
        }
    }

    /*package*/ @Nullable TabGroupModelFilterInternal getCurrentFilterForTesting() {
        return mCurrentFilter;
    }

    @Override
    public void markTabStateInitialized() {
        // Intentional no-op. Handled before the mCurrentFilter is provided.
    }

    @Override
    public void moveTabOutOfGroupInDirection(int sourceTabId, boolean trailing) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.moveTabOutOfGroupInDirection(sourceTabId, trailing);
    }

    @Override
    public void destroy() {
        // Intentional no-op. Destruction of the mCurrentFilter is not managed by this class.
    }

    @Override
    public boolean closeTabs(TabClosureParams params) {
        // Special case. TabGroupModelFilterInternal and TabModelInternal implement TabCloser (the
        // TabGroupModelFilter implementation can be removed once TabCollection launches). For now
        // we need to be careful to call the method on the IncognitoTabModelImpl as it has some
        // additional logic to prevent the delegate model from being destroyed during a tab closure
        // operation.
        return mIncognitoTabModel.closeTabs(params);
    }

    @Override
    public void addObserver(TabModelObserver observer) {
        mIncognitoTabModel.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        mIncognitoTabModel.removeObserver(observer);
    }

    @Override
    public void addTabGroupObserver(TabGroupModelFilterObserver observer) {
        mObservers.addObserver(observer);
        if (mCurrentFilter != null) {
            mCurrentFilter.addTabGroupObserver(observer);
        }
    }

    @Override
    public void removeTabGroupObserver(TabGroupModelFilterObserver observer) {
        mObservers.removeObserver(observer);
        if (mCurrentFilter != null) {
            mCurrentFilter.removeTabGroupObserver(observer);
        }
    }

    @Override
    public TabModel getTabModel() {
        return mIncognitoTabModel;
    }

    @Override
    public List<Tab> getRepresentativeTabList() {
        if (mCurrentFilter == null) return Collections.emptyList();
        return mCurrentFilter.getRepresentativeTabList();
    }

    @Override
    public int getIndividualTabAndGroupCount() {
        if (mCurrentFilter == null) return 0;
        return mCurrentFilter.getIndividualTabAndGroupCount();
    }

    @Override
    public int getCurrentRepresentativeTabIndex() {
        if (mCurrentFilter == null) return TabModel.INVALID_TAB_INDEX;
        return mCurrentFilter.getCurrentRepresentativeTabIndex();
    }

    @Override
    public @Nullable Tab getCurrentRepresentativeTab() {
        if (mCurrentFilter == null) return null;
        return mCurrentFilter.getCurrentRepresentativeTab();
    }

    @Override
    public @Nullable Tab getRepresentativeTabAt(int index) {
        if (mCurrentFilter == null) return null;
        return mCurrentFilter.getRepresentativeTabAt(index);
    }

    @Override
    public int representativeIndexOf(@Nullable Tab tab) {
        if (mCurrentFilter == null) return TabModel.INVALID_TAB_INDEX;
        return mCurrentFilter.representativeIndexOf(tab);
    }

    @Override
    public int getTabGroupCount() {
        if (mCurrentFilter == null) return 0;
        return mCurrentFilter.getTabGroupCount();
    }

    @Override
    public int getTabCountForGroup(@Nullable Token tabGroupId) {
        if (mCurrentFilter == null) return 0;
        return mCurrentFilter.getTabCountForGroup(tabGroupId);
    }

    @Override
    public boolean tabGroupExists(@Nullable Token tabGroupId) {
        if (mCurrentFilter == null) return false;
        return mCurrentFilter.tabGroupExists(tabGroupId);
    }

    @Override
    public List<Tab> getRelatedTabList(@TabId int tabId) {
        if (mCurrentFilter == null) return Collections.emptyList();
        return mCurrentFilter.getRelatedTabList(tabId);
    }

    @Override
    public List<Tab> getTabsInGroup(@Nullable Token tabGroupId) {
        if (mCurrentFilter == null) return Collections.emptyList();
        return mCurrentFilter.getTabsInGroup(tabGroupId);
    }

    @Override
    public boolean isTabInTabGroup(Tab tab) {
        if (mCurrentFilter == null) return false;
        return mCurrentFilter.isTabInTabGroup(tab);
    }

    @Override
    public int getIndexOfTabInGroup(Tab tab) {
        if (mCurrentFilter == null) return TabModel.INVALID_TAB_INDEX;
        return mCurrentFilter.getIndexOfTabInGroup(tab);
    }

    @Override
    @TabId
    public int getGroupLastShownTabId(@Nullable Token tabGroupId) {
        if (mCurrentFilter == null) return Tab.INVALID_TAB_ID;
        return mCurrentFilter.getGroupLastShownTabId(tabGroupId);
    }

    @Override
    public void moveRelatedTabs(@TabId int id, int newIndex) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.moveRelatedTabs(id, newIndex);
    }

    @Override
    public boolean willMergingCreateNewGroup(List<Tab> tabsToMerge) {
        if (mCurrentFilter == null) return false;
        return mCurrentFilter.willMergingCreateNewGroup(tabsToMerge);
    }

    @Override
    public void createSingleTabGroup(@TabId int tabId) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.createSingleTabGroup(tabId);
    }

    @Override
    public void createSingleTabGroup(Tab tab) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.createSingleTabGroup(tab);
    }

    @Override
    public void createTabGroupForTabGroupSync(List<Tab> tabs, Token tabGroupId) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.createTabGroupForTabGroupSync(tabs, tabGroupId);
    }

    @Override
    public void mergeTabsToGroup(@TabId int sourceTabId, @TabId int destinationTabId) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.mergeTabsToGroup(sourceTabId, destinationTabId);
    }

    @Override
    public void mergeTabsToGroup(
            @TabId int sourceTabId, @TabId int destinationTabId, boolean skipUpdateTabModel) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.mergeTabsToGroup(sourceTabId, destinationTabId, skipUpdateTabModel);
    }

    @Override
    public void mergeListOfTabsToGroup(
            List<Tab> tabs,
            Tab destinationTab,
            @Nullable Integer indexInGroup,
            @MergeNotificationType int notify) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.mergeListOfTabsToGroup(tabs, destinationTab, indexInGroup, notify);
    }

    @Override
    public TabUngrouper getTabUngrouper() {
        return mTabUngrouperProxy;
    }

    @Override
    public void performUndoGroupOperation(UndoGroupMetadata undoGroupMetadata) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.performUndoGroupOperation(undoGroupMetadata);
    }

    @Override
    public void undoGroupOperationExpired(UndoGroupMetadata undoGroupMetadata) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.undoGroupOperationExpired(undoGroupMetadata);
    }

    @Override
    public Set<Token> getAllTabGroupIds() {
        if (mCurrentFilter == null) return Collections.emptySet();
        return mCurrentFilter.getAllTabGroupIds();
    }

    @Override
    public int getValidPosition(Tab tab, int proposedPosition) {
        if (mCurrentFilter == null) return proposedPosition;
        return mCurrentFilter.getValidPosition(tab, proposedPosition);
    }

    @Override
    public boolean isTabModelRestored() {
        if (mCurrentFilter == null) return false;
        return mCurrentFilter.isTabModelRestored();
    }

    @Override
    public boolean isTabGroupHiding(@Nullable Token tabGroupId) {
        if (mCurrentFilter == null) return false;
        return mCurrentFilter.isTabGroupHiding(tabGroupId);
    }

    @Override
    public LazyOneshotSupplier<Set<Token>> getLazyAllTabGroupIds(
            List<Tab> tabsToExclude, boolean includePendingClosures) {
        if (mCurrentFilter == null) return LazyOneshotSupplier.fromValue(Collections.emptySet());
        return mCurrentFilter.getLazyAllTabGroupIds(tabsToExclude, includePendingClosures);
    }

    @Override
    public @Nullable String getTabGroupTitle(Token tabGroupId) {
        if (mCurrentFilter == null) return null;
        return mCurrentFilter.getTabGroupTitle(tabGroupId);
    }

    @Override
    public @Nullable String getTabGroupTitle(Tab groupedTab) {
        if (mCurrentFilter == null) return null;
        return mCurrentFilter.getTabGroupTitle(groupedTab);
    }

    @Override
    public void setTabGroupTitle(Token tabGroupId, @Nullable String title) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.setTabGroupTitle(tabGroupId, title);
    }

    @Override
    public void deleteTabGroupTitle(Token tabGroupId) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.deleteTabGroupTitle(tabGroupId);
    }

    @Override
    public int getTabGroupColor(Token tabGroupId) {
        if (mCurrentFilter == null) return TabGroupColorUtils.INVALID_COLOR_ID;
        return mCurrentFilter.getTabGroupColor(tabGroupId);
    }

    @Override
    public @TabGroupColorId int getTabGroupColorWithFallback(Token tabGroupId) {
        if (mCurrentFilter == null) return TabGroupColorId.GREY;
        return mCurrentFilter.getTabGroupColorWithFallback(tabGroupId);
    }

    @Override
    public @TabGroupColorId int getTabGroupColorWithFallback(Tab groupedTab) {
        if (mCurrentFilter == null) return TabGroupColorId.GREY;
        return mCurrentFilter.getTabGroupColorWithFallback(groupedTab);
    }

    @Override
    public void setTabGroupColor(Token tabGroupId, @TabGroupColorId int color) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.setTabGroupColor(tabGroupId, color);
    }

    @Override
    public void deleteTabGroupColor(Token tabGroupId) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.deleteTabGroupColor(tabGroupId);
    }

    @Override
    public boolean getTabGroupCollapsed(Token tabGroupId) {
        if (mCurrentFilter == null) return false;
        return mCurrentFilter.getTabGroupCollapsed(tabGroupId);
    }

    @Override
    public void setTabGroupCollapsed(Token tabGroupId, boolean isCollapsed, boolean animate) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.setTabGroupCollapsed(tabGroupId, isCollapsed, animate);
    }

    @Override
    public void deleteTabGroupCollapsed(Token tabGroupId) {
        if (mCurrentFilter == null) return;
        mCurrentFilter.deleteTabGroupCollapsed(tabGroupId);
    }
}
