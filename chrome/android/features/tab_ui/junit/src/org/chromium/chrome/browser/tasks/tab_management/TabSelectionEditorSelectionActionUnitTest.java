// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ShowMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link TabSelectionEditorSelectionAction}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSelectionEditorSelectionActionUnitTest {
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock
    private ActionDelegate mDelegate;
    private MockTabModel mTabModel;
    private TabSelectionEditorAction mAction;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mContext.setTheme(org.chromium.chrome.tab_ui.R.style.Theme_BrowserUI_DayNight);
        boolean isIncognito = false;
        mAction = TabSelectionEditorSelectionAction.createAction(mContext, ShowMode.IF_ROOM,
                ButtonType.ICON_AND_TEXT, IconPosition.END, isIncognito);
        mTabModel = spy(new MockTabModel(isIncognito, null));
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        mAction.configure(mTabModelSelector, mSelectionDelegate, mDelegate);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        Assert.assertEquals(R.id.tab_selection_editor_selection_menu_item,
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(R.string.tab_selection_editor_select_all,
                mAction.getPropertyModel().get(
                        TabSelectionEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(null,
                mAction.getPropertyModel().get(
                        TabSelectionEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID));
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ICON));
        Assert.assertNull(
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ICON_TINT));
        Assert.assertTrue(
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.SKIP_ICON_TINT));
    }

    @Test
    @SmallTest
    public void testTitleStateChange() {
        // For this test we will assume there are 2 tabs.
        List<Integer> selectedTabIds = new ArrayList<>();

        // 0 tabs of 2 selected.
        when(mDelegate.areAllTabsSelected()).thenReturn(false);
        mAction.onSelectionStateChange(selectedTabIds);
        Assert.assertEquals(R.string.tab_selection_editor_select_all,
                mAction.getPropertyModel().get(
                        TabSelectionEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                0, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));

        // 1 tab of 2 selected.
        selectedTabIds.add(0);
        mAction.onSelectionStateChange(selectedTabIds);
        Assert.assertEquals(R.string.tab_selection_editor_select_all,
                mAction.getPropertyModel().get(
                        TabSelectionEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                1, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));

        // 2 tabs of 2 selected.
        selectedTabIds.add(1);
        when(mDelegate.areAllTabsSelected()).thenReturn(true);
        mAction.onSelectionStateChange(selectedTabIds);
        Assert.assertEquals(R.string.tab_selection_editor_deselect_all,
                mAction.getPropertyModel().get(
                        TabSelectionEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                2, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    public void testSelectedAll_FromNoneSelected() {
        List<Integer> selectedTabIds = new ArrayList<>();
        when(mDelegate.areAllTabsSelected()).thenReturn(false);

        mAction.onSelectionStateChange(selectedTabIds);
        mAction.perform();
        verify(mDelegate).selectAll();
    }

    @Test
    @SmallTest
    public void testSelectedAll_FromSomeSelected() {
        List<Integer> selectedTabIds = new ArrayList<>();
        selectedTabIds.add(5);
        selectedTabIds.add(3);
        selectedTabIds.add(7);
        when(mDelegate.areAllTabsSelected()).thenReturn(false);

        mAction.onSelectionStateChange(selectedTabIds);
        mAction.perform();
        verify(mDelegate).selectAll();
    }

    @Test
    @SmallTest
    public void testDeselectedAll_FromAllSelected() {
        List<Integer> selectedTabIds = new ArrayList<>();
        selectedTabIds.add(5);
        selectedTabIds.add(3);
        selectedTabIds.add(7);
        when(mDelegate.areAllTabsSelected()).thenReturn(true);

        mAction.onSelectionStateChange(selectedTabIds);
        mAction.perform();
        verify(mDelegate).deselectAll();
    }
}
