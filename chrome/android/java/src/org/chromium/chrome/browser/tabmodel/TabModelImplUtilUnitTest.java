// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

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
    @Mock private TabModelObserver mTabModelObserver;

    private ObserverList<TabModelObserver> mObservers;
    private Set<Integer> mSelectedTabs;

    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();
    private int mNextTabId;

    @Before
    public void setUp() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        lenient().when(mNextTabPolicySupplier.get()).thenReturn(NextTabPolicy.HIERARCHICAL);
        lenient().when(mTabModelDelegate.getModel(false)).thenReturn(mTabModel);
        lenient().when(mTabModelDelegate.getModel(true)).thenReturn(mOtherTabModel);
        mNextTabId = 0;
        mObservers = new ObserverList<>();
        mObservers.addObserver(mTabModelObserver);
        mSelectedTabs = new HashSet<>();
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

    private void setUpTabsInTabModel(TabModel model, List<Tab> allTabs) {
        lenient().when(model.getCount()).thenReturn(allTabs.size());
        lenient().when(model.getTabAt(anyInt())).thenAnswer(inv -> allTabs.get(inv.getArgument(0)));
        lenient()
                .when(model.getTabAtChecked(anyInt()))
                .thenAnswer(inv -> allTabs.get(inv.getArgument(0)));
        lenient().when(model.indexOf(any())).thenAnswer(inv -> allTabs.indexOf(inv.getArgument(0)));
        // Use a new instance of the iterator each time.
        lenient().when(model.iterator()).thenAnswer(inv -> allTabs.iterator());
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
        setUpTabsInTabModel(mOtherTabModel, Collections.singletonList(incognitoTab));
        when(mOtherTabModel.index()).thenReturn(0);

        Tab normalTab = createTab();
        setUpTabsInTabModel(mTabModel, Collections.singletonList(normalTab));
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

        setUpTabsInTabModel(mTabModel, List.of(tab0, tab1, tab2));

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
        setUpTabsInTabModel(mOtherTabModel, Collections.singletonList(incognitoTab0));

        Tab tab0 = createTab();
        Tab tab1 = createTab();
        setUpTabsInTabModel(mTabModel, List.of(tab0, tab1));

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
        setUpTabsInTabModel(mTabModel, List.of(tab0, tab1, tab2));

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
        setUpTabsInTabModel(mTabModel, Collections.singletonList(tab0));

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

        setUpTabsInTabModel(mTabModel, List.of(tab0, tab1, tab2, tab3));

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
        setUpTabsInTabModel(mTabModel, List.of(tab0, tab1, tab2, tab3));

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
        setUpTabsInTabModel(mTabModel, List.of(tab0, tab1));

        setCurrentTab(tab0);
        List<Tab> closingTabs = List.of(tab0, tab1);
        assertNull(getNextTabIfClosed(mTabModel, closingTabs, false, TabCloseType.MULTIPLE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testSetTabsMultiSelected_Add() {
        Set<Integer> tabsToAdd = new HashSet<>(Arrays.asList(1, 2, 3));
        TabModelImplUtil.setTabsMultiSelected(tabsToAdd, true, mSelectedTabs, mObservers);

        assertTrue(mSelectedTabs.containsAll(tabsToAdd));
        verify(mTabModelObserver, times(1)).onTabsSelectionChanged();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testSetTabsMultiSelected_Remove() {
        mSelectedTabs.addAll(Arrays.asList(1, 2, 3, 4));
        Set<Integer> tabsToRemove = new HashSet<>(Arrays.asList(2, 4));
        TabModelImplUtil.setTabsMultiSelected(tabsToRemove, false, mSelectedTabs, mObservers);

        assertFalse(mSelectedTabs.contains(2));
        assertFalse(mSelectedTabs.contains(4));
        assertTrue(mSelectedTabs.contains(1));
        assertTrue(mSelectedTabs.contains(3));
        verify(mTabModelObserver, times(1)).onTabsSelectionChanged();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testClearMultiSelection_WithNotification() {
        mSelectedTabs.addAll(Arrays.asList(1, 2, 3));
        TabModelImplUtil.clearMultiSelection(true, mSelectedTabs, mObservers);

        assertTrue(mSelectedTabs.isEmpty());
        verify(mTabModelObserver, times(1)).onTabsSelectionChanged();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testClearMultiSelection_WithoutNotification() {
        mSelectedTabs.addAll(Arrays.asList(1, 2, 3));
        TabModelImplUtil.clearMultiSelection(false, mSelectedTabs, mObservers);

        assertTrue(mSelectedTabs.isEmpty());
        verify(mTabModelObserver, never()).onTabsSelectionChanged();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testIsTabMultiSelected() {
        Tab currentTab = createTab();
        int currentTabId = currentTab.getId();

        when(mTabModel.index()).thenReturn(0);
        when(mTabModel.getTabAt(0)).thenReturn(currentTab);

        int otherSelectedTabId = 20;
        mSelectedTabs.add(otherSelectedTabId);

        // Test for a tab in the multi-selection set.
        assertTrue(
                "Tab explicitly added to the set should be selected.",
                TabModelImplUtil.isTabMultiSelected(otherSelectedTabId, mSelectedTabs, mTabModel));

        // Test for the currently active tab.
        assertTrue(
                "The active tab should always be considered selected.",
                TabModelImplUtil.isTabMultiSelected(currentTabId, mSelectedTabs, mTabModel));

        // Test for a tab that is not selected.
        int unselectedTabId = 30;
        assertFalse(
                "A tab not in the set and not active should not be selected.",
                TabModelImplUtil.isTabMultiSelected(unselectedTabId, mSelectedTabs, mTabModel));
    }
}
