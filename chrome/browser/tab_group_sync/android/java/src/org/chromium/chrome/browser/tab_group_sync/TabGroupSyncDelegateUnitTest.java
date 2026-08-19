// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertArrayEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncDelegate.Deps;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

import java.util.Collections;
import java.util.List;

/** Unit tests for the {@link TabGroupSyncDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabWindowManager mTabWindowManager;

    private TabGroupSyncDelegate mDelegate;

    @Before
    public void setUp() {
        TabGroupSyncDelegate.Deps deps = new Deps(mTabWindowManager);
        mDelegate = TabGroupSyncDelegate.create(5, deps);
        verify(mTabWindowManager).addObserver(eq(mDelegate));
    }

    @After
    public void tearDown() {
        mDelegate.destroy();
        verify(mTabWindowManager).removeObserver(eq(mDelegate));
    }

    @Test
    public void testBasic() {}

    @Test
    public void testGetSelectedTabs_empty() {
        when(mTabWindowManager.getAllTabModelSelectors()).thenReturn(Collections.emptyList());

        assertArrayEquals(new int[0], mDelegate.getSelectedTabs());
    }

    @Test
    public void testGetSelectedTabs_singleWindow() {
        TabModelSelector selector = createMockTabModelSelector(1);
        when(mTabWindowManager.getAllTabModelSelectors()).thenReturn(List.of(selector));

        assertArrayEquals(new int[] {1}, mDelegate.getSelectedTabs());
    }

    @Test
    public void testGetSelectedTabs_multipleWindows() {
        TabModelSelector selector1 = createMockTabModelSelector(1);
        TabModelSelector selector2 = createMockTabModelSelector(2);
        when(mTabWindowManager.getAllTabModelSelectors()).thenReturn(List.of(selector1, selector2));

        assertArrayEquals(new int[] {1, 2}, mDelegate.getSelectedTabs());
    }

    @Test
    public void testGetSelectedTabs_invalidTabIdFiltered() {
        TabModelSelector selector1 = createMockTabModelSelector(Tab.INVALID_TAB_ID);
        TabModelSelector selector2 = createMockTabModelSelector(2);
        when(mTabWindowManager.getAllTabModelSelectors()).thenReturn(List.of(selector1, selector2));

        assertArrayEquals(new int[] {2}, mDelegate.getSelectedTabs());
    }

    @Test
    public void testGetSelectedTabs_nullModel() {
        TabModelSelector selector1 = createMockTabModelSelectorWithNullModel();
        TabModelSelector selector2 = createMockTabModelSelector(2);
        when(mTabWindowManager.getAllTabModelSelectors()).thenReturn(List.of(selector1, selector2));

        assertArrayEquals(new int[] {2}, mDelegate.getSelectedTabs());
    }

    private TabModelSelector createMockTabModelSelector(int selectedTabId) {
        TabModelSelector selector = mock(TabModelSelector.class);
        TabModel model = mock(TabModel.class);
        when(selector.getModel(/* incognito= */ false)).thenReturn(model);
        if (selectedTabId != Tab.INVALID_TAB_ID) {
            when(model.index()).thenReturn(0);
            Tab tab = mock(Tab.class);
            when(tab.getId()).thenReturn(selectedTabId);
            when(model.getTabAt(0)).thenReturn(tab);
        } else {
            when(model.index()).thenReturn(TabModel.INVALID_TAB_INDEX);
        }
        return selector;
    }

    private TabModelSelector createMockTabModelSelectorWithNullModel() {
        TabModelSelector selector = mock(TabModelSelector.class);
        when(selector.getModel(/* incognito= */ false)).thenReturn(null);
        return selector;
    }
}
