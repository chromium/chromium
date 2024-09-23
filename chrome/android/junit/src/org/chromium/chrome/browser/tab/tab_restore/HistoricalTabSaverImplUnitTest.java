// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link HistoricalTabSaverImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HistoricalTabSaverImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    private final ObservableSupplierImpl<TabModel> mSecondaryTabModelSupplier =
            new ObservableSupplierImpl<>();

    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private TabModel mTabModel;
    @Mock private TabModel mSecondaryTabModel;
    @Mock private HistoricalTabSaverImpl.Natives mHistoricalTabSaverJni;

    private HistoricalTabSaverImpl mHistoricalTabSaver;

    @Before
    public void setUp() {
        mJniMocker.mock(HistoricalTabSaverImplJni.TEST_HOOKS, mHistoricalTabSaverJni);
        mHistoricalTabSaver = new HistoricalTabSaverImpl(mTabModel);
        mHistoricalTabSaver.ignoreUrlSchemesForTesting(true);

        Mockito.when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        mSecondaryTabModelSupplier.set(mSecondaryTabModel);
    }

    @After
    public void tearDown() {
        mHistoricalTabSaver.destroy();
    }

    /** Tests nothing is saved for an empty group. */
    @Test
    public void testCreateHistoricalGroup_Empty() {
        HistoricalEntry group =
                new HistoricalEntry(
                        0,
                        new Token(1L, 2L),
                        "Foo",
                        TabGroupColorId.GREY,
                        Arrays.asList(new Tab[0]));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        verifyNoMoreInteractions(mHistoricalTabSaverJni);
    }

    /** Tests nothing is saved for an empty bulk closure. */
    @Test
    public void testCreateHistoricalBulk_Empty() {
        ArrayList<HistoricalEntry> entries = new ArrayList<>();
        mHistoricalTabSaver.createHistoricalBulkClosure(entries);

        verifyNoMoreInteractions(mHistoricalTabSaverJni);
    }

    /** Tests nothing is saved for an incognito tab closure. */
    @Test
    public void testCreateHistoricalBulk_Incognito() {
        Tab tab = new MockTab(0, mIncognitoProfile);
        mHistoricalTabSaver.createHistoricalTab(tab);

        verifyNoMoreInteractions(mHistoricalTabSaverJni);
    }

    /** Tests nothing is saved if the secondary model has it. */
    @Test
    public void testCreateHistoricalBulk_SkipsTabsInSecondaryModel() {
        Tab tab = new MockTab(0, mProfile);
        doReturn(tab).when(mSecondaryTabModel).getTabById(tab.getId());
        mHistoricalTabSaver.addSecodaryTabModelSupplier(mSecondaryTabModelSupplier);
        mHistoricalTabSaver.createHistoricalTab(tab);

        verifyNoMoreInteractions(mHistoricalTabSaverJni);
    }

    /** Tests collapsing a group with a single tab into a single tab entry. */
    @Test
    public void testCreateHistoricalTab_FromGroup_WithoutTabGroupId() {
        Tab tab = new MockTab(0, mProfile);

        HistoricalEntry group =
                new HistoricalEntry(
                        0, null, "Foo", TabGroupColorId.GREY, Arrays.asList(new Tab[] {tab}));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        ByteBuffer buf = ByteBuffer.allocateDirect(0);
        verify(mHistoricalTabSaverJni).createHistoricalTab(tab, buf, -1);
    }

    /**
     * Tests collapsing a group with a single tab into a single tab entry with non null web contents
     * state buffer.
     */
    @Test
    public void testCreateHistoricalTab_FromGroup_WithoutTabGroupId_NonNullBuffer() {
        ByteBuffer buf = ByteBuffer.allocateDirect(3);
        WebContentsState tempState = new WebContentsState(buf);
        tempState.setVersion(1);

        MockTab tab = MockTab.createAndInitialize(0, mProfile);
        TabTestUtils.setWebContentsState(tab, tempState);

        HistoricalEntry group =
                new HistoricalEntry(
                        0, null, "Foo", TabGroupColorId.GREY, Arrays.asList(new Tab[] {tab}));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        verify(mHistoricalTabSaverJni).createHistoricalTab(tab, buf, 1);
    }

    /** Tests collapsing a bulk closure with a single tab into a single tab entry. */
    @Test
    public void testCreateHistoricalTab_FromBulk() {
        Tab tab = new MockTab(0, mProfile);

        mHistoricalTabSaver.createHistoricalBulkClosure(
                Collections.singletonList(new HistoricalEntry(tab)));

        ByteBuffer buf = ByteBuffer.allocateDirect(0);
        verify(mHistoricalTabSaverJni).createHistoricalTab(tab, buf, -1);
    }

    /**
     * Tests collapsing a bulk closure with a single tab into a single tab entry with non null web
     * contents state buffer.
     */
    @Test
    public void testCreateHistoricalTab_FromBulk_NonNullBuffer() {
        ByteBuffer buf = ByteBuffer.allocateDirect(3);
        WebContentsState tempState = new WebContentsState(buf);
        tempState.setVersion(1);

        MockTab tab = MockTab.createAndInitialize(0, mProfile);
        TabTestUtils.setWebContentsState(tab, tempState);

        mHistoricalTabSaver.createHistoricalBulkClosure(
                Collections.singletonList(new HistoricalEntry(tab)));

        verify(mHistoricalTabSaverJni).createHistoricalTab(tab, buf, 1);
    }

    /** Tests a bulk closure is collapsed to a group if there is just a group. */
    @Test
    public void testCreateHistoricalGroup_FromBulk() {
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mProfile);

        // Also test duplicates are allowed.
        Tab[] tabList = new Tab[] {tab0, tab1};
        Token tabGroupId = new Token(728L, 324789L);
        HistoricalEntry group =
                new HistoricalEntry(
                        0, tabGroupId, "Foo", TabGroupColorId.GREY, Arrays.asList(tabList));
        mHistoricalTabSaver.createHistoricalBulkClosure(Collections.singletonList(group));

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf, buf};
        int[] versions = new int[] {-1, -1};
        verify(mHistoricalTabSaverJni)
                .createHistoricalGroup(
                        eq(mTabModel),
                        eq(tabGroupId),
                        eq(""),
                        eq("Foo"),
                        eq(TabGroupColorId.GREY),
                        eq(tabList),
                        eq(buffers),
                        eq(versions));
    }

    /** Tests incognito tabs are removed and collapse to a single tab. */
    @Test
    public void testCreateHistoricalGroup_FromGroupWithIncognito_SingleTabGroupSupported() {
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mIncognitoProfile);

        // Also test duplicates are allowed.
        Tab[] tabList = new Tab[] {tab0, tab1};
        Token tabGroupId = new Token(1L, 2L);
        HistoricalEntry group =
                new HistoricalEntry(
                        0, tabGroupId, "Foo", TabGroupColorId.GREY, Arrays.asList(tabList));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf};
        int[] versions = new int[] {-1};
        verify(mHistoricalTabSaverJni)
                .createHistoricalGroup(
                        eq(mTabModel),
                        eq(tabGroupId),
                        eq(""),
                        eq("Foo"),
                        eq(TabGroupColorId.GREY),
                        eq(new Tab[] {tab0}),
                        eq(buffers),
                        eq(versions));
    }

    /** Tests that collapsing is ignored if the tab has a tab group ID. */
    @Test
    public void testCreateHistoricalGroup_FromSingleTabGroup() {
        Tab tab = new MockTab(0, mProfile);

        Token tabGroupId = new Token(1L, 2L);
        HistoricalEntry group =
                new HistoricalEntry(
                        0,
                        new Token(1L, 2L),
                        "Foo",
                        TabGroupColorId.GREY,
                        Arrays.asList(new Tab[] {tab}));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf};
        int[] versions = new int[] {-1};
        verify(mHistoricalTabSaverJni)
                .createHistoricalGroup(
                        eq(mTabModel),
                        eq(tabGroupId),
                        eq(""),
                        eq("Foo"),
                        eq(TabGroupColorId.GREY),
                        eq(new Tab[] {tab}),
                        eq(buffers),
                        eq(versions));
    }

    /** Tests incognito tabs are removed and maintain a group. */
    @Test
    public void testCreateHistoricalGroup_FromGroupWithIncognito() {
        Tab tab0 = new MockTab(0, mProfile);
        Tab tab1 = new MockTab(1, mIncognitoProfile);
        Tab tab2 = new MockTab(2, mProfile);

        // Also test duplicates are allowed.
        Tab[] tabList = new Tab[] {tab0, tab1, tab2};
        Token tabGroupId = new Token(4L, 5L);
        HistoricalEntry group =
                new HistoricalEntry(
                        0, tabGroupId, "Foo", TabGroupColorId.GREY, Arrays.asList(tabList));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf, buf};
        int[] versions = new int[] {-1, -1};
        verify(mHistoricalTabSaverJni)
                .createHistoricalGroup(
                        eq(mTabModel),
                        eq(tabGroupId),
                        eq(""),
                        eq("Foo"),
                        eq(TabGroupColorId.GREY),
                        eq(new Tab[] {tab0, tab2}),
                        eq(buffers),
                        eq(versions));
    }

    /** Tests duplicates are allowed. */
    @Test
    public void testCreateHistoricalGroup_FromGroupWithDuplicates() {
        Tab tab0 = new MockTab(0, mProfile);

        // Also test duplicates are allowed.
        Tab[] tabList = new Tab[] {tab0, tab0, tab0};
        Token tabGroupId = new Token(4L, 5L);
        HistoricalEntry group =
                new HistoricalEntry(
                        0, tabGroupId, "Foo", TabGroupColorId.GREY, Arrays.asList(tabList));
        mHistoricalTabSaver.createHistoricalTabOrGroup(group);

        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf, buf, buf};
        int[] versions = new int[] {-1, -1, -1};
        verify(mHistoricalTabSaverJni)
                .createHistoricalGroup(
                        eq(mTabModel),
                        eq(tabGroupId),
                        eq(""),
                        eq("Foo"),
                        eq(TabGroupColorId.GREY),
                        eq(tabList),
                        eq(buffers),
                        eq(versions));
    }

    /** Tests a bulk closure of tabs including some invalid entries. */
    @Test
    public void testCreateHistoricalBulk_AllTabsWithInvalid() {
        Tab tab0 = new MockTab(0, mIncognitoProfile);
        Tab tab1 = new MockTab(1, mProfile);
        Tab tab2 = new MockTab(2, mProfile);

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
        verify(mHistoricalTabSaverJni)
                .createHistoricalBulkClosure(
                        eq(mTabModel),
                        eq(new int[0]),
                        eq(new Token[0]),
                        eq(new String[0]),
                        eq(new String[0]),
                        eq(new int[0]),
                        eq(new int[] {Tab.INVALID_TAB_ID, Tab.INVALID_TAB_ID, Tab.INVALID_TAB_ID}),
                        eq(new Tab[] {tab1, tab2, tab2}),
                        eq(buffers),
                        eq(versions));
    }

    /** Tests a bulk closure of tabs and groups including some invalid entries. */
    @Test
    public void testCreateHistoricalBulk_MixedWithInvalid() {
        // Tab.
        Tab tab0 = new MockTab(0, mProfile);
        // Incognito tab.
        Tab tab1 = new MockTab(1, mIncognitoProfile);
        // Incognito group.
        Tab tab2 = new MockTab(2, mIncognitoProfile);
        Tab tab3 = new MockTab(3, mIncognitoProfile);
        // Group.
        Tab tab4 = new MockTab(4, mProfile);
        Tab tab5 = new MockTab(5, mIncognitoProfile);
        Tab tab6 = new MockTab(6, mProfile);
        // Tab.
        Tab tab7 = new MockTab(7, mProfile);
        // Group collapse to tab.
        Tab tab8 = new MockTab(8, mProfile);
        Tab tab9 = new MockTab(9, mIncognitoProfile);
        // Group.
        Tab tab10 = new MockTab(10, mProfile);
        Tab tab11 = new MockTab(11, mProfile);

        // Also test duplicates are allowed.
        List<HistoricalEntry> entries = new ArrayList<>();
        entries.add(new HistoricalEntry(tab0));
        entries.add(new HistoricalEntry(tab1));
        entries.add(
                new HistoricalEntry(
                        0,
                        new Token(27839L, 4789L),
                        "Incognito",
                        TabGroupColorId.GREY,
                        Arrays.asList(new Tab[] {tab2, tab3})));
        Token tabGroupId1 = new Token(789L, 3289L);
        entries.add(
                new HistoricalEntry(
                        1,
                        tabGroupId1,
                        "Group 1",
                        TabGroupColorId.GREY,
                        Arrays.asList(new Tab[] {tab4, tab5, tab6})));
        entries.add(new HistoricalEntry(tab7));
        Token tabGroupId2 = new Token(347389L, 47893L);
        entries.add(
                new HistoricalEntry(
                        2,
                        tabGroupId2,
                        "Group 2",
                        TabGroupColorId.BLUE,
                        Arrays.asList(new Tab[] {tab8, tab9})));
        Token tabGroupId3 = new Token(289L, 7489L);
        entries.add(
                new HistoricalEntry(
                        3,
                        tabGroupId3,
                        "Group 3",
                        TabGroupColorId.RED,
                        Arrays.asList(new Tab[] {tab10, tab11})));
        mHistoricalTabSaver.createHistoricalBulkClosure(entries);

        int[] rootIds = new int[] {1, 2, 3};
        Token[] tabGroupIds = new Token[] {tabGroupId1, tabGroupId2, tabGroupId3};
        String[] groupTitles = new String[] {"Group 1", "Group 2", "Group 3"};
        int[] groupColors =
                new int[] {TabGroupColorId.GREY, TabGroupColorId.BLUE, TabGroupColorId.RED};
        int[] perTabRootIds = new int[] {Tab.INVALID_TAB_ID, 1, 1, Tab.INVALID_TAB_ID, 2, 3, 3};
        Tab[] tabs = new Tab[] {tab0, tab4, tab6, tab7, tab8, tab10, tab11};

        String[] savedTabGroupIds = new String[] {"", "", ""};
        byte[] bytes = new byte[0];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        ByteBuffer[] buffers = new ByteBuffer[] {buf, buf, buf, buf, buf, buf, buf};
        int[] versions = new int[] {-1, -1, -1, -1, -1, -1, -1};
        verify(mHistoricalTabSaverJni)
                .createHistoricalBulkClosure(
                        eq(mTabModel),
                        eq(rootIds),
                        eq(tabGroupIds),
                        eq(savedTabGroupIds),
                        eq(groupTitles),
                        eq(groupColors),
                        eq(perTabRootIds),
                        eq(tabs),
                        eq(buffers),
                        eq(versions));
    }
}
