// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link TabModelImplUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelImplUtilUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private TabModel mOtherTabModel;
    @Mock private TabModelDelegate mTabModelDelegate;
    @Mock private NextTabPolicySupplier mNextTabPolicySupplier;
    @Mock private Profile mProfile;
    @Mock private Profile mOtherProfile;

    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();
    private int mNextTabId;

    @Before
    public void setUp() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        lenient().when(mNextTabPolicySupplier.get()).thenReturn(NextTabPolicy.HIERARCHICAL);
        lenient().when(mTabModelDelegate.getModel(false)).thenReturn(mTabModel);
        lenient().when(mTabModelDelegate.getModel(true)).thenReturn(mOtherTabModel);
        mNextTabId = 0;
    }

    private Tab createTab() {
        return createTab(mProfile, 0, Tab.INVALID_TAB_ID);
    }

    private Tab createTab(long activeTimestampMillis) {
        return createTab(mProfile, activeTimestampMillis, Tab.INVALID_TAB_ID);
    }

    private Tab createTab(long activeTimestampMillis, int parentId) {
        return createTab(mProfile, activeTimestampMillis, parentId);
    }

    private Tab createTab(Profile profile, long activeTimestampMillis, int parentId) {
        MockTab tab = MockTab.createAndInitialize(mNextTabId++, profile);
        tab.setTimestampMillis(activeTimestampMillis);
        tab.setParentId(parentId);
        tab.setIsInitialized(true);
        return tab;
    }

    private void setCurrentTab(Tab tab) {
        mCurrentTabSupplier.set(tab);
    }

    private Tab getNextTabIfClosed(TabModel model, Tab closingTab, boolean uponExit) {
        return getNextTabIfClosed(
                model, Collections.singletonList(closingTab), uponExit, TabCloseType.SINGLE);
    }

    private Tab getNextTabIfClosed(
            TabModel model,
            List<Tab> closingTabs,
            boolean uponExit,
            @TabCloseType int tabCloseType) {
        return TabModelImplUtil.getNextTabIfClosed(
                model,
                mTabModelDelegate,
                mCurrentTabSupplier,
                mNextTabPolicySupplier,
                closingTabs,
                uponExit,
                tabCloseType);
    }

    @Test
    public void testGetNextTabIfClosed_InactiveModel() {
        when(mOtherTabModel.isActiveModel()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mOtherTabModel);

        Tab incognitoTab = createTab(mOtherProfile, 0, Tab.INVALID_TAB_ID);
        when(mOtherTabModel.getTabAt(0)).thenReturn(incognitoTab);
        when(mOtherTabModel.getCount()).thenReturn(1);
        when(mOtherTabModel.index()).thenReturn(0);

        Tab normalTab = createTab();
        when(mTabModel.getTabAt(0)).thenReturn(normalTab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.index()).thenReturn(0);

        setCurrentTab(normalTab);

        assertEquals(incognitoTab, getNextTabIfClosed(mTabModel, normalTab, false));
    }

    @Test
    public void testGetNextTabIfClosed_NotCurrentTab() {
        when(mTabModel.isActiveModel()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);

        Tab tab0 = createTab();
        Tab tab1 = createTab();
        Tab tab2 = createTab();

        setCurrentTab(tab0);
        assertEquals(tab0, getNextTabIfClosed(mTabModel, tab1, false));
        assertEquals(tab0, getNextTabIfClosed(mTabModel, tab2, false));

        setCurrentTab(tab1);
        assertEquals(tab1, getNextTabIfClosed(mTabModel, tab0, false));
        assertEquals(tab1, getNextTabIfClosed(mTabModel, tab2, false));

        setCurrentTab(tab2);
        assertEquals(tab2, getNextTabIfClosed(mTabModel, tab0, false));
        assertEquals(tab2, getNextTabIfClosed(mTabModel, tab1, false));
    }

    @Test
    public void testGetNextTabIfClosed_ParentTab() {
        when(mTabModel.isActiveModel()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);

        Tab tab0 = createTab();
        Tab tab2 = createTab(0, tab0.getId());

        when(mTabModel.getTabById(tab0.getId())).thenReturn(tab0);

        setCurrentTab(tab2);
        assertEquals(tab0, getNextTabIfClosed(mTabModel, tab2, false));
    }

    @Test
    public void testGetNextTabIfClosed_Adjacent() {
        when(mTabModel.isActiveModel()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);

        Tab tab0 = createTab();
        Tab tab1 = createTab();
        Tab tab2 = createTab();

        when(mTabModel.indexOf(tab0)).thenReturn(0);
        when(mTabModel.indexOf(tab1)).thenReturn(1);
        when(mTabModel.indexOf(tab2)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getTabAtChecked(0)).thenReturn(tab0);
        when(mTabModel.getTabAtChecked(1)).thenReturn(tab1);
        when(mTabModel.getTabAtChecked(2)).thenReturn(tab2);

        setCurrentTab(tab0);
        assertEquals(tab1, getNextTabIfClosed(mTabModel, tab0, false));

        setCurrentTab(tab1);
        assertEquals(tab0, getNextTabIfClosed(mTabModel, tab1, false));

        setCurrentTab(tab2);
        assertEquals(tab1, getNextTabIfClosed(mTabModel, tab2, false));
    }

    @Test
    public void testGetNextTabIfClosed_LastIncognitoTab() {
        when(mOtherTabModel.isActiveModel()).thenReturn(true);
        when(mOtherTabModel.isIncognitoBranded()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mOtherTabModel);

        Tab incognitoTab0 = createTab(mOtherProfile, 0, Tab.INVALID_TAB_ID);
        Tab tab0 = createTab();
        Tab tab1 = createTab();

        when(mTabModel.getTabAt(0)).thenReturn(tab0);
        when(mTabModel.getTabAt(1)).thenReturn(tab1);
        when(mTabModel.getCount()).thenReturn(2);

        setCurrentTab(incognitoTab0);

        when(mTabModel.index()).thenReturn(0);
        assertEquals(tab0, getNextTabIfClosed(mOtherTabModel, incognitoTab0, false));

        when(mTabModel.index()).thenReturn(1);
        assertEquals(tab1, getNextTabIfClosed(mOtherTabModel, incognitoTab0, false));
    }

    @Test
    public void testGetNextTabIfClosed_MostRecentTab() {
        when(mTabModel.isActiveModel()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);

        // uponExit overrides parent selection.
        Tab tab0 = createTab(10, Tab.INVALID_TAB_ID);
        Tab tab1 = createTab(200, tab0.getId());
        Tab tab2 = createTab(30, tab0.getId());

        List<Tab> tabs = List.of(tab0, tab1, tab2);
        when(mTabModel.getTabAt(anyInt())).thenAnswer(inv -> tabs.get(inv.getArgument(0)));
        when(mTabModel.getTabAtChecked(anyInt())).thenAnswer(inv -> tabs.get(inv.getArgument(0)));
        when(mTabModel.getCount()).thenReturn(tabs.size());

        setCurrentTab(tab0);
        assertEquals(tab1, getNextTabIfClosed(mTabModel, tab0, true));

        setCurrentTab(tab1);
        assertEquals(tab2, getNextTabIfClosed(mTabModel, tab1, true));

        setCurrentTab(tab2);
        assertEquals(tab1, getNextTabIfClosed(mTabModel, tab2, true));
    }

    @Test
    public void testGetNextTabIfClosed_InvalidSelection() {
        when(mTabModel.isActiveModel()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);

        Tab tab0 = createTab();
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(tab0);
        when(mTabModel.indexOf(tab0)).thenReturn(0);
        when(mTabModel.getTabAtChecked(0)).thenReturn(tab0);

        setCurrentTab(tab0);
        assertNull(getNextTabIfClosed(mTabModel, tab0, false));
    }

    @Test
    public void testGetNextTabIfClosed_Multiple_NotCurrentTab() {
        when(mTabModel.isActiveModel()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);

        Tab tab0 = createTab();
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();

        setCurrentTab(tab0);
        List<Tab> closingTabs = List.of(tab1, tab2);
        assertEquals(
                tab0, getNextTabIfClosed(mTabModel, closingTabs, false, TabCloseType.MULTIPLE));

        setCurrentTab(tab3);
        assertEquals(
                tab3, getNextTabIfClosed(mTabModel, closingTabs, false, TabCloseType.MULTIPLE));
    }

    @Test
    public void testGetNextTabIfClosed_Multiple_Adjacent() {
        when(mTabModel.isActiveModel()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);

        Tab tab0 = createTab();
        Tab tab1 = createTab();
        Tab tab2 = createTab();
        Tab tab3 = createTab();

        when(mTabModel.indexOf(tab0)).thenReturn(0);
        when(mTabModel.indexOf(tab1)).thenReturn(1);
        when(mTabModel.indexOf(tab2)).thenReturn(2);
        when(mTabModel.indexOf(tab3)).thenReturn(3);
        when(mTabModel.getCount()).thenReturn(4);
        when(mTabModel.getTabAtChecked(0)).thenReturn(tab0);
        when(mTabModel.getTabAtChecked(1)).thenReturn(tab1);
        when(mTabModel.getTabAtChecked(2)).thenReturn(tab2);
        when(mTabModel.getTabAtChecked(3)).thenReturn(tab3);

        // Close tabs [1, 2], current is 1. Should select 0.
        setCurrentTab(tab1);
        List<Tab> closingTabs = List.of(tab1, tab2);
        assertEquals(
                tab0, getNextTabIfClosed(mTabModel, closingTabs, false, TabCloseType.MULTIPLE));

        // Close tabs [1, 2], current is 2. Should select 0.
        // The nearby logic depends on the first element of closingTabs.
        setCurrentTab(tab2);
        assertEquals(
                tab0, getNextTabIfClosed(mTabModel, closingTabs, false, TabCloseType.MULTIPLE));

        // Close tabs [0, 1], current is 0. Should select 2.
        setCurrentTab(tab0);
        closingTabs = List.of(tab0, tab1);
        assertEquals(
                tab2, getNextTabIfClosed(mTabModel, closingTabs, false, TabCloseType.MULTIPLE));

        // Close tabs [2, 3], current is 3. Should select 1.
        setCurrentTab(tab3);
        closingTabs = List.of(tab2, tab3);
        assertEquals(
                tab1, getNextTabIfClosed(mTabModel, closingTabs, false, TabCloseType.MULTIPLE));
    }

    @Test
    public void testGetNextTabIfClosed_Multiple_MostRecentTab() {
        when(mTabModel.isActiveModel()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);

        Tab tab0 = createTab(10);
        Tab tab1 = createTab(40);
        Tab tab2 = createTab(20);
        Tab tab3 = createTab(30);

        List<Tab> allTabs = List.of(tab0, tab1, tab2, tab3);
        when(mTabModel.getCount()).thenReturn(allTabs.size());
        when(mTabModel.getTabAt(anyInt())).thenAnswer(inv -> allTabs.get(inv.getArgument(0)));
        when(mTabModel.getTabAtChecked(anyInt()))
                .thenAnswer(inv -> allTabs.get(inv.getArgument(0)));

        setCurrentTab(tab1);
        List<Tab> closingTabs = List.of(tab1, tab2);
        // Remaining tabs are tab0 (ts=10) and tab3 (ts=30). tab3 is most recent.
        assertEquals(tab3, getNextTabIfClosed(mTabModel, closingTabs, true, TabCloseType.MULTIPLE));
    }

    @Test
    public void testGetNextTabIfClosed_Multiple_InvalidSelection() {
        when(mTabModel.isActiveModel()).thenReturn(true);
        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);

        Tab tab0 = createTab();
        Tab tab1 = createTab();
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getTabAtChecked(0)).thenReturn(tab0);
        when(mTabModel.getTabAtChecked(1)).thenReturn(tab1);
        when(mTabModel.indexOf(tab0)).thenReturn(0);

        setCurrentTab(tab0);
        List<Tab> closingTabs = List.of(tab0, tab1);
        assertNull(getNextTabIfClosed(mTabModel, closingTabs, false, TabCloseType.MULTIPLE));
    }
}
