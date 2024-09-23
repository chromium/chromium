// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link HistoricalTabModelObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
public class HistoricalTabModelObserverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private HistoricalTabSaver mHistoricalTabSaver;

    private HistoricalTabModelObserver mObserver;

    @Before
    public void setUp() {
        when(mTabGroupModelFilter.isTabGroupHiding(any())).thenReturn(false);
        when(mTabGroupModelFilter.isTabInTabGroup(any())).thenReturn(false);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(anyInt())).thenReturn(-1);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getComprehensiveModel()).thenReturn(mTabModel);

        mObserver = new HistoricalTabModelObserver(mTabGroupModelFilter, mHistoricalTabSaver);
        verify(mTabGroupModelFilter).addObserver(mObserver);

        mContext = spy(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @After
    public void tearDown() {
        mObserver.destroy();
        verify(mTabGroupModelFilter).removeObserver(mObserver);
    }

    @Test
    public void testEmpty() {
        mObserver.onFinishingMultipleTabClosure(new ArrayList<>(), /* canRestore= */ true);

        verifyNoMoreInteractions(mHistoricalTabSaver);
    }

    @Test
    public void testMultipleTabs_NotRestorable() {
        MockTab mockTab = createMockTab(0);

        mObserver.onFinishingMultipleTabClosure(
                Collections.singletonList(mockTab), /* canRestore= */ false);

        verifyNoMoreInteractions(mHistoricalTabSaver);
    }

    @Test
    public void testSingleTab() {
        MockTab mockTab = createMockTab(0);

        mObserver.onFinishingMultipleTabClosure(
                Collections.singletonList(mockTab), /* canRestore= */ true);

        verify(mHistoricalTabSaver).createHistoricalTab(eq(mockTab));
    }

    @Test
    public void testTabGroupWithSingleTab_NotUndoable() {
        int rootId = 123;
        MockTab mockTab = createMockTab(rootId);
        Token tabGroupId = new Token(1L, 2L);
        String title = "bar";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab});

        mObserver.onFinishingMultipleTabClosure(
                Collections.singletonList(mockTab), /* canRestore= */ true);

        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();

        assertEquals(1, entries.size());
        HistoricalEntry group = entries.get(0);
        assertFalse(group.isSingleTab());
        assertEquals(1, group.getTabs().size());
        assertEquals(rootId, group.getRootId());
        assertEquals(tabGroupId, group.getTabGroupId());
        assertEquals(title, group.getGroupTitle());
        assertEquals(color, group.getGroupColor());
        assertEquals(mockTab, group.getTabs().get(0));
    }

    @Test
    public void testTabGroupWithSingleTab_Undoable() {
        int rootId = 123;
        MockTab mockTab = createMockTab(rootId);
        Token tabGroupId = new Token(1L, 2L);
        String title = "bar";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab});
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(rootId)).thenReturn(1);
        when(mTabGroupModelFilter.tabGroupExistsForRootId(rootId)).thenReturn(false);
        when(mTabGroupModelFilter.isTabInTabGroup(mockTab)).thenReturn(false);

        mObserver.onFinishingMultipleTabClosure(
                Collections.singletonList(mockTab), /* canRestore= */ true);

        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();

        assertEquals(1, entries.size());
        HistoricalEntry group = entries.get(0);
        assertFalse(group.isSingleTab());
        assertEquals(1, group.getTabs().size());
        assertEquals(rootId, group.getRootId());
        assertEquals(tabGroupId, group.getTabGroupId());
        assertEquals(title, group.getGroupTitle());
        assertEquals(color, group.getGroupColor());
        assertEquals(mockTab, group.getTabs().get(0));
    }

    @Test
    public void testMultipleTabs() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        Tab[] tabList = new Tab[] {mockTab0, mockTab1, mockTab2};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();
        assertEquals(tabList.length, entries.size());
        for (int i = 0; i < tabList.length; i++) {
            HistoricalEntry entry = entries.get(i);
            assertEquals(1, entry.getTabs().size());
            assertEquals(tabList[i], entry.getTabs().get(0));
        }
    }

    @Test
    public void testSubsetOfGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        MockTab[] tabList = new MockTab[] {mockTab0, mockTab1, mockTab2};
        Token tabGroupId = new Token(1L, 243L);
        createGroup(tabGroupId, title, color, tabList);

        List<Tab> closingTabList = List.of(mockTab1, mockTab2);
        mObserver.onFinishingMultipleTabClosure(closingTabList, /* canRestore= */ true);

        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();

        assertEquals(2, entries.size());
        HistoricalEntry entryTab1 = entries.get(0);
        HistoricalEntry entryTab2 = entries.get(1);
        assertTrue(entryTab1.isSingleTab());
        assertTrue(entryTab2.isSingleTab());
        assertEquals(mockTab1, entryTab1.getTabs().get(0));
        assertEquals(mockTab2, entryTab2.getTabs().get(0));
    }

    @Test
    public void testSingleGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        MockTab[] tabList = new MockTab[] {mockTab0, mockTab1, mockTab2};
        Token tabGroupId = new Token(1L, 243L);
        final int rootId = createGroup(tabGroupId, title, color, tabList);

        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single group
        // entry.
        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();

        assertEquals(1, entries.size());
        HistoricalEntry group = entries.get(0);
        assertFalse(group.isSingleTab());
        assertEquals(tabList.length, group.getTabs().size());
        assertEquals(rootId, group.getRootId());
        assertEquals(tabGroupId, group.getTabGroupId());
        assertEquals(title, group.getGroupTitle());
        assertEquals(color, group.getGroupColor());
        for (int i = 0; i < tabList.length; i++) {
            assertEquals(tabList[i], group.getTabs().get(i));
        }
    }

    @Test
    public void testSingleTabInGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab1});

        MockTab[] tabList = new MockTab[] {mockTab0, mockTab1};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single tab
        // entry.
        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();
        assertEquals(2, entries.size());

        HistoricalEntry tab0 = entries.get(0);
        assertEquals(1, tab0.getTabs().size());
        assertEquals(mockTab0, tab0.getTabs().get(0));

        HistoricalEntry tab1 = entries.get(1);
        assertEquals(1, tab1.getTabs().size());
        assertEquals(tabGroupId, tab1.getTabGroupId());
        assertEquals(mockTab1, tab1.getTabs().get(0));
    }

    @Test
    public void testTabGroupHiding() {
        MockTab mockTab0 = createMockTab(0);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab0});
        when(mTabGroupModelFilter.isTabGroupHiding(tabGroupId)).thenReturn(true);
        when(mTabGroupModelFilter.getLazyAllTabGroupIdsInComprehensiveModel(any()))
                .thenReturn(LazyOneshotSupplier.fromValue(new HashSet<>()));

        MockTab[] tabList = new MockTab[] {mockTab0};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single tab
        // entry.
        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();
        assertEquals(0, entries.size());
    }

    @Test
    public void testTabGroupHidingTwoPhases_SingleTabs() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab0, mockTab1});
        when(mTabGroupModelFilter.isTabGroupHiding(tabGroupId)).thenReturn(true);
        when(mTabGroupModelFilter.getLazyAllTabGroupIdsInComprehensiveModel(any()))
                .thenReturn(LazyOneshotSupplier.fromValue(Set.of(tabGroupId)));

        MockTab[] tabList = new MockTab[] {mockTab0};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // Close tab 0 first, even though the group is hiding there are still other tabs in the
        // comprehensive model for the group so treat as a separate closure.
        verify(mHistoricalTabSaver).createHistoricalTab(eq(mockTab0));

        // Close tab 1, since it is part of the last event "closing" the tab group and it is hiding
        // the group so no entry should be created.
        tabList = new MockTab[] {mockTab1};
        createGroup(tabGroupId, title, color, tabList);
        when(mTabGroupModelFilter.getLazyAllTabGroupIdsInComprehensiveModel(any()))
                .thenReturn(LazyOneshotSupplier.fromValue(new HashSet<>()));
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), true);

        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();
        assertEquals(0, entries.size());
    }

    @Test
    public void testTabGroupHidingTwoPhases_SingleTabThenTwo() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab0, mockTab1, mockTab2});
        when(mTabGroupModelFilter.isTabGroupHiding(tabGroupId)).thenReturn(true);
        when(mTabGroupModelFilter.getLazyAllTabGroupIdsInComprehensiveModel(any()))
                .thenReturn(LazyOneshotSupplier.fromValue(Set.of(tabGroupId)));

        MockTab[] tabList = new MockTab[] {mockTab0};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // Close tab 0 first, even though the group is hiding there are still other tabs in the
        // comprehensive model for the group so treat as a separate closure.
        verify(mHistoricalTabSaver).createHistoricalTab(eq(mockTab0));

        // Close tab 1, and tab 2, since it is part of the last event "closing" the tab group and it
        // is hiding the group so no entry should be created.
        tabList = new MockTab[] {mockTab1, mockTab2};
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab1, mockTab2});
        when(mTabGroupModelFilter.getLazyAllTabGroupIdsInComprehensiveModel(any()))
                .thenReturn(LazyOneshotSupplier.fromValue(new HashSet<>()));
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), true);

        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();
        assertEquals(0, entries.size());
    }

    @Test
    public void testMultipleTabsAndGroups() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);
        MockTab mockTab3 = createMockTab(3);
        MockTab mockTab4 = createMockTab(4);
        MockTab mockTab5 = createMockTab(5);

        final String groupTitle1 = "foo";
        @TabGroupColorId int groupColor1 = TabGroupColorId.GREY;
        MockTab[] groupTabs1 = new MockTab[] {mockTab3, mockTab5};
        Token tabGroupId1 = new Token(3L, 4L);
        final int rootId1 = createGroup(tabGroupId1, groupTitle1, groupColor1, groupTabs1);

        final String groupTitle2 = "Bar";
        @TabGroupColorId int groupColor2 = TabGroupColorId.BLUE;
        MockTab[] groupTabs2 = new MockTab[] {mockTab1, mockTab2};
        Token tabGroupId2 = new Token(3L, 4L);
        final int rootId2 = createGroup(tabGroupId2, groupTitle2, groupColor2, groupTabs2);

        Tab[] tabList = new Tab[] {mockTab0, mockTab2, mockTab3, mockTab4, mockTab1, mockTab5};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single group
        // entry.
        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();
        assertEquals(4, entries.size());

        // First tab in list is ungrouped mockTab0.
        HistoricalEntry historicalTab0 = entries.get(0);
        assertEquals(1, historicalTab0.getTabs().size());
        assertEquals(mockTab0, historicalTab0.getTabs().get(0));

        // Second tab in list is mockTab2 for tabGroup2. This grabs mockTab1 to this position.
        HistoricalEntry historicalGroup2 = entries.get(1);
        assertFalse(historicalGroup2.isSingleTab());
        assertEquals(2, historicalGroup2.getTabs().size());
        assertEquals(rootId2, historicalGroup2.getRootId());
        assertEquals(tabGroupId2, historicalGroup2.getTabGroupId());
        assertEquals(groupTitle2, historicalGroup2.getGroupTitle());
        assertEquals(groupColor2, historicalGroup2.getGroupColor());
        assertEquals(mockTab2, historicalGroup2.getTabs().get(0));
        assertEquals(mockTab1, historicalGroup2.getTabs().get(1));

        // Third tab in list is mockTab3 for tabGroup1. This grabs mockTab5 to this position.
        HistoricalEntry historicalGroup1 = entries.get(2);
        assertFalse(historicalGroup1.isSingleTab());
        assertEquals(2, historicalGroup1.getTabs().size());
        assertEquals(rootId1, historicalGroup1.getRootId());
        assertEquals(tabGroupId1, historicalGroup1.getTabGroupId());
        assertEquals(groupTitle1, historicalGroup1.getGroupTitle());
        assertEquals(groupColor1, historicalGroup1.getGroupColor());
        assertEquals(mockTab3, historicalGroup1.getTabs().get(0));
        assertEquals(mockTab5, historicalGroup1.getTabs().get(1));

        // Fourth tab in list is ungrouped mockTab4.
        HistoricalEntry historicalTab4 = entries.get(3);
        assertEquals(1, historicalTab4.getTabs().size());
        assertEquals(mockTab4, historicalTab4.getTabs().get(0));
    }

    private MockTab createMockTab(int id) {
        MockTab mockTab = new MockTab(id, mProfile);
        mockTab.setRootId(id);
        return mockTab;
    }

    /**
     * Creates a group.
     *
     * @param tabGroupId The tab group id.
     * @param title Group title.
     * @param color Group color.
     * @param tabList List of tabs in group.
     * @return ID of the group.
     */
    private int createGroup(
            @Nullable Token tabGroupId,
            @Nullable String title,
            @TabGroupColorId int color,
            MockTab[] tabList) {
        assert tabList.length != 0;

        final int rootId = tabList[0].getId();
        when(mTabGroupModelFilter.getTabGroupTitle(rootId)).thenReturn(title);
        when(mTabGroupModelFilter.getTabGroupColorWithFallback(rootId)).thenReturn(color);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(rootId)).thenReturn(tabList.length);
        when(mTabGroupModelFilter.tabGroupExistsForRootId(rootId)).thenReturn(true);
        for (MockTab tab : tabList) {
            tab.setRootId(rootId);
            tab.setTabGroupId(tabGroupId);
            when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
        }
        return rootId;
    }
}
