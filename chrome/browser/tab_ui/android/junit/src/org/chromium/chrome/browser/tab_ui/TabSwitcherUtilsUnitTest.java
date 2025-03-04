// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Unit tests for {@link TabSwitcherUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSwitcherUtilsUnitTest {
    private static final int TAB_ID_1 = 9;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private LayoutManager mLayoutManager;

    @Before
    public void setUp() {
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(mTab);
        when(mTab.getId()).thenReturn(TAB_ID_1);

        when(mLayoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
    }

    @Test
    public void testFocusTab() {
        TabSwitcherUtils.hideTabSwitcherAndShowTab(TAB_ID_1, mTabModelSelector, mLayoutManager);
        verify(mTabModelSelector).selectModel(false);
        verify(mTabModel).setIndex(eq(0), eq(TabSelectionType.FROM_USER));
        verify(mLayoutManager).showLayout(eq(LayoutType.BROWSING), eq(false));
    }
}
