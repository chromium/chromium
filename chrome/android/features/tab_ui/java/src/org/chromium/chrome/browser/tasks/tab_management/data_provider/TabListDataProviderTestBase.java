// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Shared test harness for {@link FlatTabListDataProviderUnitTest} and {@link
 * GroupedTabListDataProviderUnitTest}.
 *
 * <p>Provides list-backed {@link TabModel} stubbing, observer capture, and helper methods that
 * mutate the backing tab list and fire {@link TabModelObserver} and {@link TabGroupObserver}
 * callbacks in the order emitted by {@code TabCollectionTabModelImpl}.
 */
abstract class TabListDataProviderTestBase {
    static final @TabId int TAB1_ID = 1;
    static final @TabId int TAB2_ID = 2;
    static final @TabId int TAB3_ID = 3;
    static final @TabId int TAB4_ID = 4;
    static final Token TAB_GROUP_ID = new Token(/* high= */ 1L, /* low= */ 2L);
    static final Token TAB_GROUP_ID_2 = new Token(/* high= */ 3L, /* low= */ 4L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock TabModel mTabModel;
    @Mock TabModel mOtherTabModel;
    @Mock TabListDataObserver mObserver;

    @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor ArgumentCaptor<TabGroupObserver> mTabGroupObserverCaptor;

    final MockTab mTab1 = new MockTab(TAB1_ID, /* profile= */ null);
    final MockTab mTab2 = new MockTab(TAB2_ID, /* profile= */ null);
    final MockTab mTab3 = new MockTab(TAB3_ID, /* profile= */ null);
    final MockTab mTab4 = new MockTab(TAB4_ID, /* profile= */ null);
    final List<Tab> mModelTabs = new ArrayList<>();
    final List<Tab> mOtherModelTabs = new ArrayList<>();
    final Map<Token, Boolean> mCollapsed = new HashMap<>();
    final Set<Integer> mMultiSelectedTabIds = new HashSet<>();
    final SettableNullableObservableSupplier<TabModel> mTabModelSupplier =
            ObservableSuppliers.createNullable();

    @Nullable Tab mSelectedTab;
    TabModelObserver mTabModelObserver;
    TabGroupObserver mTabGroupObserver;
    TabListDataProvider mProvider;

    @Before
    public void setUp() {
        stubBackedTabModel(mTabModel, mModelTabs);
        stubBackedTabModel(mOtherTabModel, mOtherModelTabs);
    }

    /** Makes {@code model} answer index and lookup queries from the live {@code tabs} list. */
    void stubBackedTabModel(TabModel model, List<Tab> tabs) {
        when(model.isTabStateInitialized()).thenReturn(true);
        when(model.iterator()).thenAnswer(invocation -> tabs.iterator());
        when(model.getTabAt(anyInt()))
                .thenAnswer(
                        invocation -> {
                            int index = invocation.getArgument(0);
                            return index >= 0 && index < tabs.size() ? tabs.get(index) : null;
                        });
        when(model.getTabById(anyInt()))
                .thenAnswer(
                        invocation -> {
                            int tabId = invocation.getArgument(0);
                            for (Tab tab : tabs) {
                                if (tab.getId() == tabId) return tab;
                            }
                            return null;
                        });
        when(model.index())
                .thenAnswer(
                        invocation -> {
                            if (mSelectedTab == null) return TabList.INVALID_TAB_INDEX;
                            int index = tabs.indexOf(mSelectedTab);
                            return index < 0 ? TabList.INVALID_TAB_INDEX : index;
                        });
        when(model.getTabGroupCollapsed(any()))
                .thenAnswer(
                        invocation -> {
                            Token tabGroupId = invocation.getArgument(0);
                            return mCollapsed.getOrDefault(tabGroupId, true);
                        });
        when(model.containsTabGroup(any()))
                .thenAnswer(
                        invocation -> {
                            Token tabGroupId = invocation.getArgument(0);
                            if (tabGroupId == null) return false;
                            for (Tab tab : tabs) {
                                if (tabGroupId.equals(tab.getTabGroupId())) return true;
                            }
                            return false;
                        });
        when(model.getTabsInGroup(any()))
                .thenAnswer(
                        invocation -> {
                            Token tabGroupId = invocation.getArgument(0);
                            if (tabGroupId == null) return List.of();
                            List<Tab> groupTabs = new ArrayList<>();
                            for (Tab tab : tabs) {
                                if (tabGroupId.equals(tab.getTabGroupId())) {
                                    groupTabs.add(tab);
                                }
                            }
                            return groupTabs;
                        });
        when(model.getTabCountForGroup(any()))
                .thenAnswer(
                        invocation -> {
                            Token tabGroupId = invocation.getArgument(0);
                            if (tabGroupId == null) return 0;
                            int count = 0;
                            for (Tab tab : tabs) {
                                if (tabGroupId.equals(tab.getTabGroupId())) count++;
                            }
                            return count;
                        });
        when(model.isTabMultiSelected(anyInt()))
                .thenAnswer(
                        invocation -> {
                            int tabId = invocation.getArgument(0);
                            return mMultiSelectedTabIds.contains(tabId);
                        });
    }

    void initProvider(TabListDataProvider provider) {
        mProvider = provider;
        mProvider.addObserver(mObserver);
    }

    void initProviderWithTabs(TabListDataProvider provider, Tab... tabs) {
        setModelTabs(tabs);
        mTabModelSupplier.set(mTabModel);
        initProvider(provider);
        mProvider.requestDataReset();
        captureObserver(mTabModel);
        clearInvocations(mObserver);
    }

    void captureObserver(TabModel model) {
        verify(model).addObserver(mTabModelObserverCaptor.capture());
        mTabModelObserver = mTabModelObserverCaptor.getValue();
        verify(model).addTabGroupObserver(mTabGroupObserverCaptor.capture());
        mTabGroupObserver = mTabGroupObserverCaptor.getValue();
    }

    void setModelTabs(Tab... tabs) {
        mModelTabs.clear();
        mModelTabs.addAll(Arrays.asList(tabs));
    }

    void addTab(Tab tab, int modelIndex) {
        addTabWithLaunchType(tab, modelIndex, TabLaunchType.FROM_CHROME_UI);
    }

    void addTabWithLaunchType(Tab tab, int modelIndex, @TabLaunchType int launchType) {
        mModelTabs.add(modelIndex, tab);
        mTabModelObserver.didAddTab(
                tab,
                launchType,
                TabCreationState.LIVE_IN_FOREGROUND,
                /* markedForSelection= */ false);
    }

    void moveTab(Tab tab, int newIndex) {
        int curIndex = mModelTabs.indexOf(tab);
        mModelTabs.remove(curIndex);
        mModelTabs.add(newIndex, tab);
        mTabModelObserver.didMoveTab(tab, newIndex, curIndex);
    }

    void moveTabWithinGroup(Tab tab, int newIndex) {
        int curIndex = mModelTabs.indexOf(tab);
        mModelTabs.remove(curIndex);
        mModelTabs.add(newIndex, tab);
        mTabGroupObserver.didMoveWithinGroup(tab, curIndex, newIndex);
    }

    void moveTabGroup(Token tabGroupId, int newIndex) {
        List<Tab> groupTabs = new ArrayList<>();
        for (Tab tab : mModelTabs) {
            if (tabGroupId.equals(tab.getTabGroupId())) {
                groupTabs.add(tab);
            }
        }
        int oldIndex = mModelTabs.indexOf(groupTabs.get(0));
        mModelTabs.removeAll(groupTabs);
        mModelTabs.addAll(newIndex, groupTabs);
        mTabGroupObserver.onTabGroupMoved(tabGroupId, oldIndex, newIndex);
    }

    void mergeTabToGroup(Tab tab, Token groupId, int newIndex, boolean isDestinationTab) {
        int curIndex = mModelTabs.indexOf(tab);
        tab.setTabGroupId(groupId);
        if (curIndex != newIndex) {
            mModelTabs.remove(curIndex);
            mModelTabs.add(newIndex, tab);
            mTabModelObserver.didMoveTab(tab, newIndex, curIndex);
        }
        mTabGroupObserver.didMergeTabToGroup(tab, isDestinationTab);
    }

    void ungroupTab(Tab tab, Token oldGroupId, int newIndex) {
        int curIndex = mModelTabs.indexOf(tab);
        tab.setTabGroupId(/* tabGroupId= */ null);
        if (curIndex != newIndex) {
            mModelTabs.remove(curIndex);
            mModelTabs.add(newIndex, tab);
            mTabModelObserver.didMoveTab(tab, newIndex, curIndex);
        }
        mTabGroupObserver.didMoveTabOutOfGroup(tab, oldGroupId);
    }

    void closeTab(Tab tab) {
        mModelTabs.remove(tab);
        mTabModelObserver.didRemoveTabForClosure(tab);
    }

    void removingTabGroup(Token tabGroupId) {
        mTabGroupObserver.onTabGroupRemoving(tabGroupId);
    }

    void removeTabGroup(Token tabGroupId, @DidRemoveTabGroupReason int reason) {
        mTabGroupObserver.didRemoveTabGroup(tabGroupId, reason);
    }

    void undoCloseTab(Tab tab, int modelIndex) {
        mModelTabs.add(modelIndex, tab);
        mTabModelObserver.tabClosureUndone(tab);
    }

    void commitCloseTab(Tab tab) {
        mTabModelObserver.tabClosureCommitted(tab);
    }

    void selectTab(Tab tab, @TabId int prevSelectedTabId) {
        mSelectedTab = tab;
        mTabModelObserver.didSelectTab(tab, TabSelectionType.FROM_USER, prevSelectedTabId);
    }

    void setMultiSelectedTabs(Integer... tabIds) {
        mMultiSelectedTabIds.clear();
        mMultiSelectedTabIds.addAll(List.of(tabIds));
        mTabModelObserver.onTabsSelectionChanged();
    }

    void assertItems(TabListItem... expected) {
        assertEquals(List.of(expected), mProvider.getItemsForTesting());
    }

    void assertResetWithItems(TabListItem... expected) {
        List<TabListItem> expectedList = List.of(expected);
        verify(mObserver).onDataReset(expectedList);
        assertEquals(expectedList, mProvider.getItemsForTesting());
    }

    static void groupTabs(@Nullable Token groupId, Tab... tabs) {
        for (Tab tab : tabs) {
            tab.setTabGroupId(groupId);
        }
    }

    static TabItem item(@TabId int tabId) {
        return new TabItem(
                tabId,
                /* isSelected= */ false,
                /* isPinned= */ false,
                /* isMultiSelected= */ false);
    }

    static TabItem selected(@TabId int tabId) {
        return new TabItem(
                tabId, /* isSelected= */ true, /* isPinned= */ false, /* isMultiSelected= */ false);
    }

    static TabItem pinned(@TabId int tabId) {
        return new TabItem(
                tabId, /* isSelected= */ false, /* isPinned= */ true, /* isMultiSelected= */ false);
    }

    static TabItem multiSelected(@TabId int tabId) {
        return new TabItem(
                tabId, /* isSelected= */ false, /* isPinned= */ false, /* isMultiSelected= */ true);
    }

    static TabGroupItem group(Token tabGroupId, boolean isSelected, boolean isCollapsed) {
        return new TabGroupItem(tabGroupId, isSelected, isCollapsed);
    }
}
