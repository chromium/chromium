// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

import java.util.Arrays;
import java.util.HashSet;
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
    @Mock private TabModelObserver mTabModelObserver;

    private ObserverList<TabModelObserver> mObservers;
    private Set<Integer> mSelectedTabs;

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

    private Tab createTab(Profile profile, long activeTimestampMillis, int parentId) {
        MockTab tab = MockTab.createAndInitialize(mNextTabId++, profile);
        tab.setTimestampMillis(activeTimestampMillis);
        tab.setParentId(parentId);
        tab.setIsInitialized(true);
        return tab;
    }

    @Test
    public void testSetTabsMultiSelected_Add() {
        Set<Integer> tabsToAdd = new HashSet<>(Arrays.asList(1, 2, 3));
        TabModelImplUtil.setTabsMultiSelected(tabsToAdd, true, mSelectedTabs, mObservers);

        assertTrue(mSelectedTabs.containsAll(tabsToAdd));
        verify(mTabModelObserver, times(1)).onTabsSelectionChanged();
    }

    @Test
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
    public void testClearMultiSelection_WithNotification() {
        mSelectedTabs.addAll(Arrays.asList(1, 2, 3));
        TabModelImplUtil.clearMultiSelection(true, mSelectedTabs, mObservers);

        assertTrue(mSelectedTabs.isEmpty());
        verify(mTabModelObserver, times(1)).onTabsSelectionChanged();
    }

    @Test
    public void testClearMultiSelection_WithoutNotification() {
        mSelectedTabs.addAll(Arrays.asList(1, 2, 3));
        TabModelImplUtil.clearMultiSelection(false, mSelectedTabs, mObservers);

        assertTrue(mSelectedTabs.isEmpty());
        verify(mTabModelObserver, never()).onTabsSelectionChanged();
    }

    @Test
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
