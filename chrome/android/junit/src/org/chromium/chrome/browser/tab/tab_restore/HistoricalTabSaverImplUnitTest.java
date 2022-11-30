// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Unit tests for {@link HistoricalTabSaverImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoricalTabSaverImplUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private TabModel mTabModel;
    @Mock
    private HistoricalTabSaverImpl.Natives mHistoricalTabSaverJni;

    private HistoricalTabSaverImpl mHistoricalTabSaver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(HistoricalTabSaverImplJni.TEST_HOOKS, mHistoricalTabSaverJni);
        mHistoricalTabSaver = new HistoricalTabSaverImpl(mTabModel);
        mHistoricalTabSaver.ignoreUrlSchemesForTesting(true);
    }

    /**
     * Tests nothing is saved for an empty group.
     */
    @Test
    public void testCreateHistoricalGroup_Empty() {
        HistoricalEntry group = new HistoricalEntry(0, "Foo", Arrays.asList(new Tab[0]));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        verifyNoMoreInteractions(mHistoricalTabSaverJni);
    }

    /**
     * Tests nothing is saved for an empty bulk closure.
     */
    @Test
    public void testCreateHistoricalBulk_Empty() {
        ArrayList<HistoricalEntry> entries = new ArrayList<>();
        mHistoricalTabSaver.createHistoricalBulkClosure(entries);

        verifyNoMoreInteractions(mHistoricalTabSaverJni);
    }

    /**
     * Tests nothing is saved for an incognito tab closure.
     */
    @Test
    public void testCreateHistoricalBulk_Incognito() {
        Tab tab = new MockTab(0, true);
        mHistoricalTabSaver.createHistoricalTab(tab);

        verifyNoMoreInteractions(mHistoricalTabSaverJni);
    }

    /**
     * Tests collapsing a group with a single tab into a single tab entry.
     */
    @Test
    public void testCreateHistoricalTab_FromGroup() {
        Tab tab = new MockTab(0, false);

        HistoricalEntry group = new HistoricalEntry(0, "Foo", Arrays.asList(new Tab[] {tab}));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        verify(mHistoricalTabSaverJni, times(1)).createHistoricalTab(tab, buf, -1);
    }

    /**
     * Tests collapsing a group with a single tab into a single tab entry with non null web contents
     * state buffer.
     */
    @Test
    public void testCreateHistoricalTab_FromGroup_NonNullBuffer() {
        byte[] bytes = new byte[3];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        WebContentsState tempState = new WebContentsState(buf);
        tempState.setVersion(1);

        MockTab tab = new MockTab(0, false);
        CriticalPersistedTabData tempData = new CriticalPersistedTabData(tab);
        tempData.setWebContentsState(tempState);
        tab = (MockTab) MockTab.initializeWithCriticalPersistedTabData(tab, tempData);

        HistoricalEntry group = new HistoricalEntry(0, "Foo", Arrays.asList(new Tab[] {tab}));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        verify(mHistoricalTabSaverJni, times(1)).createHistoricalTab(tab, buf, 1);
    }

    /**
     * Tests collapsing a bulk closure with a single tab into a single tab entry.
     */
    @Test
    public void testCreateHistoricalTab_FromBulk() {
        Tab tab = new MockTab(0, false);

        mHistoricalTabSaver.createHistoricalBulkClosure(
                Collections.singletonList(new HistoricalEntry(tab)));

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        verify(mHistoricalTabSaverJni, times(1)).createHistoricalTab(tab, buf, -1);
    }

    /**
     * Tests collapsing a bulk closure with a single tab into a single tab entry with non null web
     * contents state buffer.
     */
    @Test
    public void testCreateHistoricalTab_FromBulk_NonNullBuffer() {
        byte[] bytes = new byte[3];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        WebContentsState tempState = new WebContentsState(buf);
        tempState.setVersion(1);

        MockTab tab = new MockTab(0, false);
        CriticalPersistedTabData tempData = new CriticalPersistedTabData(tab);
        tempData.setWebContentsState(tempState);
        tab = (MockTab) MockTab.initializeWithCriticalPersistedTabData(tab, tempData);

        mHistoricalTabSaver.createHistoricalBulkClosure(
                Collections.singletonList(new HistoricalEntry(tab)));

        verify(mHistoricalTabSaverJni, times(1)).createHistoricalTab(tab, buf, 1);
    }

    /**
     * Tests a bulk closure is collapsed to a group if there is just a group.
     */
    @Test
    public void testCreateHistoricalGroup_FromBulk() {
        Tab tab0 = new MockTab(0, false);
        Tab tab1 = new MockTab(1, false);

        // Also test duplicates are allowed.
        Tab[] tabList = new Tab[] {tab0, tab1};
        HistoricalEntry group = new HistoricalEntry(0, "Foo", Arrays.asList(tabList));
        mHistoricalTabSaver.createHistoricalBulkClosure(Collections.singletonList(group));

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf, buf};
        int[] versions = new int[] {-1, -1};
        verify(mHistoricalTabSaverJni, times(1))
                .createHistoricalGroup(
                        eq(mTabModel), eq("Foo"), eq(tabList), eq(buffers), eq(versions));
    }

    /**
     * Tests incognito tabs are removed and collapse to a single tab.
     */
    @Test
    public void testCreateHistoricalTab_FromGroupWithIncognito() {
        Tab tab0 = new MockTab(0, false);
        Tab tab1 = new MockTab(1, true);

        // Also test duplicates are allowed.
        Tab[] tabList = new Tab[] {tab0, tab1};
        HistoricalEntry group = new HistoricalEntry(0, "Foo", Arrays.asList(tabList));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        verify(mHistoricalTabSaverJni, times(1)).createHistoricalTab(tab0, buf, -1);
    }

    /**
     * Tests incognito tabs are removed and maintain a group.
     */
    @Test
    public void testCreateHistoricalGroup_FromGroupWithIncognito() {
        Tab tab0 = new MockTab(0, false);
        Tab tab1 = new MockTab(1, true);
        Tab tab2 = new MockTab(2, false);

        // Also test duplicates are allowed.
        Tab[] tabList = new Tab[] {tab0, tab1, tab2};
        HistoricalEntry group = new HistoricalEntry(0, "Foo", Arrays.asList(tabList));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf, buf};
        int[] versions = new int[] {-1, -1};
        verify(mHistoricalTabSaverJni, times(1))
                .createHistoricalGroup(eq(mTabModel), eq("Foo"), eq(new Tab[] {tab0, tab2}),
                        eq(buffers), eq(versions));
    }

    /**
     * Tests duplicates are allowed.
     */
    @Test
    public void testCreateHistoricalGroup_FromGroupWithDuplicates() {
        Tab tab0 = new MockTab(0, false);

        // Also test duplicates are allowed.
        Tab[] tabList = new Tab[] {tab0, tab0, tab0};
        HistoricalEntry group = new HistoricalEntry(0, "Foo", Arrays.asList(tabList));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf, buf, buf};
        int[] versions = new int[] {-1, -1, -1};
        verify(mHistoricalTabSaverJni, times(1))
                .createHistoricalGroup(
                        eq(mTabModel), eq("Foo"), eq(tabList), eq(buffers), eq(versions));
    }

    /**
     * Tests a bulk closure of tabs including some invalid entries.
     */
    @Test
    public void testCreateHistoricalBulk_AllTabsWithInvalid() {
        Tab tab0 = new MockTab(0, true);
        Tab tab1 = new MockTab(1, false);
        Tab tab2 = new MockTab(2, false);

        // Also test duplicates are allowed.
        List<HistoricalEntry> entries = new ArrayList<>();
        entries.add(new HistoricalEntry(tab0));
        entries.add(new HistoricalEntry(tab1));
        entries.add(new HistoricalEntry(tab2));
        entries.add(new HistoricalEntry(tab2));
        mHistoricalTabSaver.createHistoricalBulkClosure(entries);

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf, buf, buf};
        int[] versions = new int[] {-1, -1, -1};
        verify(mHistoricalTabSaverJni, times(1))
                .createHistoricalBulkClosure(eq(mTabModel), eq(new int[0]), eq(new String[0]),
                        eq(new int[] {Tab.INVALID_TAB_ID, Tab.INVALID_TAB_ID, Tab.INVALID_TAB_ID}),
                        eq(new Tab[] {tab1, tab2, tab2}), eq(buffers), eq(versions));
    }

    /**
     * Tests a bulk closure of tabs and groups including some invalid entries.
     */
    @Test
    public void testCreateHistoricalBulk_MixedWithInvalid() {
        // Tab.
        Tab tab0 = new MockTab(0, false);
        // Incognito tab.
        Tab tab1 = new MockTab(1, true);
        // Incognito group.
        Tab tab2 = new MockTab(2, true);
        Tab tab3 = new MockTab(3, true);
        // Group.
        Tab tab4 = new MockTab(4, false);
        Tab tab5 = new MockTab(5, true);
        Tab tab6 = new MockTab(6, false);
        // Tab.
        Tab tab7 = new MockTab(7, false);
        // Group collapse to tab.
        Tab tab8 = new MockTab(8, false);
        Tab tab9 = new MockTab(9, true);
        // Group.
        Tab tab10 = new MockTab(10, false);
        Tab tab11 = new MockTab(11, false);

        // Also test duplicates are allowed.
        List<HistoricalEntry> entries = new ArrayList<>();
        entries.add(new HistoricalEntry(tab0));
        entries.add(new HistoricalEntry(tab1));
        entries.add(new HistoricalEntry(0, "Incognito", Arrays.asList(new Tab[] {tab2, tab3})));
        entries.add(new HistoricalEntry(1, "Group 1", Arrays.asList(new Tab[] {tab4, tab5, tab6})));
        entries.add(new HistoricalEntry(tab7));
        entries.add(new HistoricalEntry(2, "Group 2", Arrays.asList(new Tab[] {tab8, tab9})));
        entries.add(new HistoricalEntry(3, "Group 3", Arrays.asList(new Tab[] {tab10, tab11})));
        mHistoricalTabSaver.createHistoricalBulkClosure(entries);

        int[] groupIds = new int[] {1, 3};
        String[] groupTitles = new String[] {"Group 1", "Group 3"};
        int[] perTabGroupIds =
                new int[] {Tab.INVALID_TAB_ID, 1, 1, Tab.INVALID_TAB_ID, Tab.INVALID_TAB_ID, 3, 3};
        Tab[] tabs = new Tab[] {tab0, tab4, tab6, tab7, tab8, tab10, tab11};

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf, buf, buf, buf, buf, buf, buf};
        int[] versions = new int[] {-1, -1, -1, -1, -1, -1, -1};
        verify(mHistoricalTabSaverJni, times(1))
                .createHistoricalBulkClosure(eq(mTabModel), eq(groupIds), eq(groupTitles),
                        eq(perTabGroupIds), eq(tabs), eq(buffers), eq(versions));
    }
}
