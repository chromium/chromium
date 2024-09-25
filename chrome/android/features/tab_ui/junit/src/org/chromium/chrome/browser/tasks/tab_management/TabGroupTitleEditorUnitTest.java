// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;


import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;

import java.util.HashMap;
import java.util.Map;

/** Tests for {@link TabGroupTitleEditor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupTitleEditorUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock TabGroupModelFilter mTabGroupModelFilter;

    private Context mContext;
    private Map<String, String> mStorage;
    private TabGroupTitleEditor mTabGroupTitleEditor;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mStorage = new HashMap<>();
        mTabGroupTitleEditor =
                new TabGroupTitleEditor(mContext) {
                    @Override
                    protected void updateTabGroupTitle(Tab tab, String title) {}

                    @Override
                    protected void storeTabGroupTitle(int tabRootId, String title) {
                        mStorage.put(String.valueOf(tabRootId), title);
                    }

                    @Override
                    protected void deleteTabGroupTitle(int tabRootId) {
                        mStorage.remove(String.valueOf(tabRootId));
                    }

                    @Override
                    protected String getTabGroupTitle(int tabRootId) {
                        return mStorage.get(String.valueOf(tabRootId));
                    }
                };
    }

    @Test
    public void testDefaultTitle() {
        int relatedTabCount = 5;

        String expectedTitle =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.bottom_tab_grid_title_placeholder,
                                relatedTabCount,
                                relatedTabCount);
        assertEquals(expectedTitle, TabGroupTitleEditor.getDefaultTitle(mContext, relatedTabCount));
    }

    @Test
    public void testIsDefaultTitle() {
        int fourTabsCount = 4;
        String fourTabsTitle = TabGroupTitleEditor.getDefaultTitle(mContext, fourTabsCount);
        assertTrue(mTabGroupTitleEditor.isDefaultTitle(fourTabsTitle, fourTabsCount));
        assertFalse(mTabGroupTitleEditor.isDefaultTitle(fourTabsTitle, 3));
        assertFalse(mTabGroupTitleEditor.isDefaultTitle("Foo", fourTabsCount));
    }

    @Test
    public void testGetDisplayableTitle_Explicit() {
        String title = "t1";
        when(mTabGroupModelFilter.getTabGroupTitle(anyInt())).thenReturn(title);
        assertEquals(
                title, TabGroupTitleEditor.getDisplayableTitle(mContext, mTabGroupModelFilter, 12));
    }

    @Test
    public void testGetDisplayableTitle_Fallback() {
        int tabCount = 4567;
        when(mTabGroupModelFilter.getTabGroupTitle(anyInt())).thenReturn("");
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(anyInt())).thenReturn(tabCount);
        String title = TabGroupTitleEditor.getDisplayableTitle(mContext, mTabGroupModelFilter, 12);
        assertTrue(title.contains(String.valueOf(tabCount)));
    }
}
