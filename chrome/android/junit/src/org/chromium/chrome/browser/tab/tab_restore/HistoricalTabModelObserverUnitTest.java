// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.SharedPreferences;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Unit tests for {@link HistoricalTabModelObserver}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoricalTabModelObserverUnitTest {
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";

    private Context mContext;
    @Mock
    private SharedPreferences mSharedPreferences;
    @Mock
    private TabModel mTabModel;
    @Mock
    private HistoricalTabSaver mHistoricalTabSaver;

    private HistoricalTabModelObserver mObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mObserver = new HistoricalTabModelObserver(mTabModel, mHistoricalTabSaver);
        verify(mTabModel, times(1)).addObserver(mObserver);

        mContext = spy(ContextUtils.getApplicationContext());
        when(mContext.getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE))
                .thenReturn(mSharedPreferences);
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @After
    public void tearDown() {
        mObserver.destroy();
        verify(mTabModel, times(1)).removeObserver(mObserver);
    }

    @Test
    public void testEmpty() {
        mObserver.onFinishingMultipleTabClosure(new ArrayList<Tab>());

        verifyNoMoreInteractions(mHistoricalTabSaver);
    }

    @Test
    public void testSingleTab() {
        MockTab mockTab = createMockTab(0);

        mObserver.onFinishingMultipleTabClosure(Collections.singletonList(mockTab));

        verify(mHistoricalTabSaver, times(1)).createHistoricalTab(eq(mockTab));
    }

    @Test
    public void testMultipleTabs() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        Tab[] tabList = new Tab[] {mockTab0, mockTab1, mockTab2};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList));

        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver, times(1)).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();
        Assert.assertEquals(tabList.length, entries.size());
        for (int i = 0; i < tabList.length; i++) {
            HistoricalEntry entry = entries.get(i);
            Assert.assertEquals(1, entry.getTabs().size());
            Assert.assertEquals(tabList[i], entry.getTabs().get(0));
        }
    }

    @Test
    public void testSingleGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        final String title = "foo";
        MockTab[] tabList = new MockTab[] {mockTab0, mockTab1, mockTab2};
        final int groupId = createGroup(title, tabList);

        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList));

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single group
        // entry.
        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver, times(1)).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();

        Assert.assertEquals(1, entries.size());
        HistoricalEntry group = entries.get(0);
        Assert.assertFalse(group.isSingleTab());
        Assert.assertEquals(tabList.length, group.getTabs().size());
        Assert.assertEquals(groupId, group.getGroupId());
        Assert.assertEquals(title, group.getGroupTitle());
        for (int i = 0; i < tabList.length; i++) {
            Assert.assertEquals(tabList[i], group.getTabs().get(i));
        }
    }

    @Test
    public void testSingleTabInGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);

        final String title = "foo";
        final int groupId = createGroup(title, new MockTab[] {mockTab1});

        when(mSharedPreferences.getString(String.valueOf(groupId), null)).thenReturn(title);

        MockTab[] tabList = new MockTab[] {mockTab0, mockTab1};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList));

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single tab
        // entry.
        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver, times(1)).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();
        Assert.assertEquals(2, entries.size());

        HistoricalEntry tab0 = entries.get(0);
        Assert.assertEquals(1, tab0.getTabs().size());
        Assert.assertEquals(mockTab0, tab0.getTabs().get(0));

        HistoricalEntry tab1 = entries.get(1);
        Assert.assertEquals(1, tab0.getTabs().size());
        Assert.assertEquals(mockTab1, tab1.getTabs().get(0));
    }

    @Test
    public void testMultipleTabsAndGroups() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);
        MockTab mockTab3 = createMockTab(3);
        MockTab mockTab4 = createMockTab(4);
        MockTab mockTab5 = createMockTab(5);

        final String group1Title = "foo";
        MockTab[] group1Tabs = new MockTab[] {mockTab3, mockTab5};
        final int group1Id = createGroup(group1Title, group1Tabs);

        final String group2Title = "Bar";
        MockTab[] group2Tabs = new MockTab[] {mockTab1, mockTab2};
        final int group2Id = createGroup(group2Title, group2Tabs);

        Tab[] tabList = new Tab[] {mockTab0, mockTab2, mockTab3, mockTab4, mockTab1, mockTab5};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList));

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single group
        // entry.
        ArgumentCaptor<List<HistoricalEntry>> arg = ArgumentCaptor.forClass((Class) List.class);
        verify(mHistoricalTabSaver, times(1)).createHistoricalBulkClosure(arg.capture());
        List<HistoricalEntry> entries = arg.getValue();
        Assert.assertEquals(4, entries.size());

        // First tab in list is ungrouped mockTab0.
        HistoricalEntry historicalTab0 = entries.get(0);
        Assert.assertEquals(1, historicalTab0.getTabs().size());
        Assert.assertEquals(mockTab0, historicalTab0.getTabs().get(0));

        // Second tab in list is mockTab2 for tabGroup2. This grabs mockTab1 to this position.
        HistoricalEntry historicalGroup2 = entries.get(1);
        Assert.assertFalse(historicalGroup2.isSingleTab());
        Assert.assertEquals(2, historicalGroup2.getTabs().size());
        Assert.assertEquals(group2Id, historicalGroup2.getGroupId());
        Assert.assertEquals(group2Title, historicalGroup2.getGroupTitle());
        Assert.assertEquals(mockTab2, historicalGroup2.getTabs().get(0));
        Assert.assertEquals(mockTab1, historicalGroup2.getTabs().get(1));

        // Third tab in list is mockTab3 for tabGroup1. This grabs mockTab5 to this position.
        HistoricalEntry historicalGroup1 = entries.get(2);
        Assert.assertFalse(historicalGroup1.isSingleTab());
        Assert.assertEquals(2, historicalGroup1.getTabs().size());
        Assert.assertEquals(group1Id, historicalGroup1.getGroupId());
        Assert.assertEquals(group1Title, historicalGroup1.getGroupTitle());
        Assert.assertEquals(mockTab3, historicalGroup1.getTabs().get(0));
        Assert.assertEquals(mockTab5, historicalGroup1.getTabs().get(1));

        // Fourth tab in list is ungrouped mockTab4.
        HistoricalEntry historicalTab4 = entries.get(3);
        Assert.assertEquals(1, historicalTab4.getTabs().size());
        Assert.assertEquals(mockTab4, historicalTab4.getTabs().get(0));
    }

    private MockTab createMockTab(int id) {
        MockTab mockTab = new MockTab(id, false);
        mockTab.setRootId(id);
        return mockTab;
    }

    /**
     * Creates a group.
     * @param title Group title.
     * @param tabList List of tabs in group.
     * @return ID of the group.
     */
    private int createGroup(String title, MockTab[] tabList) {
        assert tabList.length != 0;

        final int groupId = tabList[0].getId();
        when(mSharedPreferences.getString(String.valueOf(groupId), null)).thenReturn(title);
        for (MockTab tab : tabList) {
            tab.setRootId(groupId);
        }
        return groupId;
    }
}
