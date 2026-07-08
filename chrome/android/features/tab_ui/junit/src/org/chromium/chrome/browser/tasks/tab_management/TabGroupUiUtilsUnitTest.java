// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.tab_ui.R;

import java.util.Collection;
import java.util.List;

/** Unit tests for {@link TabGroupUiUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupUiUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabWindowManager mTabWindowManager;

    @Before
    public void setUp() {
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
    }

    @Test
    public void testGetAddToGroupMenuItemString_alreadyInGroup() {
        Token tabGroupId = new Token(1L, 1L);
        assertEquals(
                R.string.menu_move_tab_to_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(mTabModel, tabGroupId));
    }

    @Test
    public void testGetAddToGroupMenuItemString_noTabGroups() {
        when(mTabModel.getTabGroupCount()).thenReturn(0);
        assertEquals(
                R.string.menu_add_tab_to_new_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(
                        mTabModel, /* currentTabGroupId= */ null));
    }

    @Test
    public void testGetAddToGroupMenuItemString_hasTabGroupsInCurrentWindow() {
        when(mTabModel.getTabGroupCount()).thenReturn(1);
        assertEquals(
                R.string.menu_add_tab_to_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(
                        mTabModel, /* currentTabGroupId= */ null));

        Token tabGroupId = new Token(1L, 1L);
        assertEquals(
                R.string.menu_move_tab_to_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(mTabModel, tabGroupId));
    }

    @Test
    public void testGetAddToGroupMenuItemString_nullModel() {
        assertEquals(
                R.string.menu_add_tab_to_new_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(
                        /* tabModel= */ null, /* currentTabGroupId= */ null));
    }

    @Test
    public void testGetAddToGroupMenuItemString_withHasTabGroups() {
        Token tabGroupId = new Token(1L, 1L);
        assertEquals(
                R.string.menu_move_tab_to_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(tabGroupId, /* hasTabGroups= */ true));
        assertEquals(
                R.string.menu_move_tab_to_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(tabGroupId, /* hasTabGroups= */ false));

        assertEquals(
                R.string.menu_add_tab_to_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(
                        /* currentTabGroupId= */ null, /* hasTabGroups= */ true));
        assertEquals(
                R.string.menu_add_tab_to_new_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(
                        /* currentTabGroupId= */ null, /* hasTabGroups= */ false));
    }

    @Test
    public void testHasTabGroups_SingleWindow_HasGroups() {
        when(mTabModel.getTabGroupCount()).thenReturn(1);
        assertTrue(TabGroupUtils.hasTabGroups(mTabModel));
        assertTrue(TabGroupUtils.hasTabGroups(mTabModel, List.of(mTabModelSelector)));
    }

    @Test
    public void testHasTabGroups_SingleWindow_NoGroups() {
        when(mTabModel.getTabGroupCount()).thenReturn(0);
        assertFalse(TabGroupUtils.hasTabGroups(mTabModel));
        assertFalse(TabGroupUtils.hasTabGroups(mTabModel, List.of(mTabModelSelector)));
    }

    @Test
    public void testHasTabGroups_NullModel() {
        assertFalse(TabGroupUtils.hasTabGroups(/* tabModel= */ null));
        assertFalse(
                TabGroupUtils.hasTabGroups(
                        /* tabModel= */ null, (Collection<TabModelSelector>) null));
    }

    @Test
    public void testHasTabGroups_CrossWindow_HasGroupsInOtherWindow() {
        when(mTabModel.getTabGroupCount()).thenReturn(0);
        when(mTabModel.isIncognito()).thenReturn(false);

        TabModelSelector otherSelector = mock(TabModelSelector.class);
        TabModel otherModel = mock(TabModel.class);

        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(otherSelector.getModel(false)).thenReturn(otherModel);
        when(otherModel.getTabGroupCount()).thenReturn(2);

        List<TabModelSelector> selectors = List.of(mTabModelSelector, otherSelector);
        assertTrue(TabGroupUtils.hasTabGroups(mTabModel, selectors));
        assertFalse(TabGroupUtils.hasTabGroups(mTabModel, (Collection<TabModelSelector>) null));
    }
}
