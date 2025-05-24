// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabListEditorSelectionAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorSelectionActionUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;
    @Mock private TabGroupModelFilter mGroupFilter;
    private MockTabModel mTabModel;
    private TabListEditorAction mAction;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).get();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mAction =
                TabListEditorSelectionAction.createAction(
                        mContext, ShowMode.IF_ROOM, ButtonType.ICON_AND_TEXT, IconPosition.END);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mGroupFilter.getTabModel()).thenReturn(mTabModel);
        // TODO(ckitagawa): Add tests for when this is true.
        mAction.configure(() -> mGroupFilter, mSelectionDelegate, mDelegate, false);
    }

    @Test
    public void testInherentActionProperties() {
        Assert.assertEquals(
                R.id.tab_list_editor_selection_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(
                R.string.tab_selection_editor_select_all,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                false,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertEquals(
                null,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID));
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabListEditorActionProperties.ICON_TINT));
    }

    @Test
    public void testTitleStateChange() {
        // For this test we will assume there are 2 tabs.
        List<TabListEditorItemSelectionId> selectedItemIds = new ArrayList<>();

        // 0 tabs of 2 selected.
        when(mDelegate.areAllTabsSelected()).thenReturn(false);
        mAction.onSelectionStateChange(selectedItemIds);
        Assert.assertEquals(
                R.string.tab_selection_editor_select_all,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
        Drawable selectAllIcon =
                mAction.getPropertyModel().get(TabListEditorActionProperties.ICON);

        // 1 tab of 2 selected.
        selectedItemIds.add(TabListEditorItemSelectionId.createTabId(0));
        mAction.onSelectionStateChange(selectedItemIds);
        Assert.assertEquals(
                R.string.tab_selection_editor_select_all,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                1, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
        Assert.assertEquals(
                selectAllIcon,
                mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));

        // 2 tabs of 2 selected.
        selectedItemIds.add(TabListEditorItemSelectionId.createTabId(1));
        when(mDelegate.areAllTabsSelected()).thenReturn(true);
        mAction.onSelectionStateChange(selectedItemIds);
        Assert.assertEquals(
                R.string.tab_selection_editor_deselect_all,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                2, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
        Assert.assertNotEquals(
                selectAllIcon,
                mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    public void testSelectedAll_FromNoneSelected() {
        List<TabListEditorItemSelectionId> selectedItemIds = new ArrayList<>();
        when(mDelegate.areAllTabsSelected()).thenReturn(false);

        mAction.onSelectionStateChange(selectedItemIds);
        mAction.perform();
        verify(mDelegate).selectAll();
    }

    @Test
    public void testSelectedAll_FromSomeSelected() {
        List<TabListEditorItemSelectionId> selectedItemIds = new ArrayList<>();
        selectedItemIds.add(TabListEditorItemSelectionId.createTabId(5));
        selectedItemIds.add(TabListEditorItemSelectionId.createTabId(3));
        selectedItemIds.add(TabListEditorItemSelectionId.createTabId(7));
        when(mDelegate.areAllTabsSelected()).thenReturn(false);

        mAction.onSelectionStateChange(selectedItemIds);
        mAction.perform();
        verify(mDelegate).selectAll();
    }

    @Test
    public void testDeselectedAll_FromAllSelected() {
        List<TabListEditorItemSelectionId> selectedItemIds = new ArrayList<>();
        selectedItemIds.add(TabListEditorItemSelectionId.createTabId(5));
        selectedItemIds.add(TabListEditorItemSelectionId.createTabId(3));
        selectedItemIds.add(TabListEditorItemSelectionId.createTabId(7));
        when(mDelegate.areAllTabsSelected()).thenReturn(true);

        mAction.onSelectionStateChange(selectedItemIds);
        mAction.perform();
        verify(mDelegate).deselectAll();
    }
}
