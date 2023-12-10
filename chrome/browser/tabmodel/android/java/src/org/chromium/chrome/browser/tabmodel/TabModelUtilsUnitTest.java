// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

/** Tests for {@link TabModelUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelUtilsUnitTest {
    private static final int TAB_ID = 5;
    private static final int INCOGNITO_TAB_ID = 7;
    private static final int UNUSED_TAB_ID = 9;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private Tab mTab;
    @Mock private Tab mIncognitoTab;

    private MockTabModelSelector mTabModelSelector;
    private MockTabModel mTabModel;
    private MockTabModel mIncognitoTabModel;

    @Before
    public void setUp() {
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mIncognitoTab.getId()).thenReturn(INCOGNITO_TAB_ID);
        when(mIncognitoTab.isIncognito()).thenReturn(true);
        mTabModelSelector = new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);

        mTabModel = (MockTabModel) mTabModelSelector.getModel(false);
        mTabModel.addTab(
                mTab,
                TabList.INVALID_TAB_INDEX,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_BACKGROUND);
        mIncognitoTabModel = (MockTabModel) mTabModelSelector.getModel(true);
        mIncognitoTabModel.addTab(
                mIncognitoTab,
                TabList.INVALID_TAB_INDEX,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_BACKGROUND);
    }

    @Test
    @SmallTest
    public void testSelectTabById() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelUtils.selectTabById(mTabModelSelector, TAB_ID, TabSelectionType.FROM_USER, false);
        assertEquals(TAB_ID, mTabModel.getTabAt(mTabModel.index()).getId());
    }

    @Test
    @SmallTest
    public void testSelectTabByIdIncognito() {
        assertEquals(TabList.INVALID_TAB_INDEX, mIncognitoTabModel.index());
        TabModelUtils.selectTabById(
                mTabModelSelector, INCOGNITO_TAB_ID, TabSelectionType.FROM_USER, false);
        assertEquals(
                INCOGNITO_TAB_ID, mIncognitoTabModel.getTabAt(mIncognitoTabModel.index()).getId());
    }

    @Test
    @SmallTest
    public void testSelectTabByIdNoOpInvalidTabId() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelUtils.selectTabById(
                mTabModelSelector, Tab.INVALID_TAB_ID, TabSelectionType.FROM_USER, false);
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
    }

    @Test
    @SmallTest
    public void testSelectTabByIdNoOpNotFound() {
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
        TabModelUtils.selectTabById(
                mTabModelSelector, UNUSED_TAB_ID, TabSelectionType.FROM_USER, false);
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());
    }
}
