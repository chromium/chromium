// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabListEditorUngroupAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorUngroupActionUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    @Mock private TabGroupModelFilter mGroupFilter;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;

    private MockTabModel mTabModel;
    private TabListEditorAction mAction;

    @Before
    public void setUp() {
        mAction =
                TabListEditorUngroupAction.createAction(
                        RuntimeEnvironment.application,
                        ShowMode.MENU_ONLY,
                        ButtonType.TEXT,
                        IconPosition.START);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mGroupFilter.getTabModel()).thenReturn(mTabModel);
        when(mGroupFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        mAction.configure(() -> mGroupFilter, mSelectionDelegate, mDelegate, false);
    }

    @Test
    public void testInherentActionProperties() {
        Assert.assertEquals(
                R.id.tab_list_editor_ungroup_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(
                R.plurals.tab_selection_editor_ungroup_tabs,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertEquals(
                R.plurals.accessibility_tab_selection_editor_ungroup_tabs,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    public void testUngroupActionDisabled() {
        List<TabListEditorItemSelectionId> itemIds = new ArrayList<>();
        mAction.onSelectionStateChange(itemIds);
        Assert.assertEquals(
                false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    public void testUngroupActionWithTabs() throws Exception {
        List<Integer> tabIds = Arrays.asList(5, 3, 7);
        List<TabListEditorItemSelectionId> itemIds =
                Arrays.asList(
                        TabListEditorItemSelectionId.createTabId(5),
                        TabListEditorItemSelectionId.createTabId(3),
                        TabListEditorItemSelectionId.createTabId(7));
        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }
        when(mGroupFilter.getRelatedTabList(anyInt())).thenReturn(tabs);
        Set<TabListEditorItemSelectionId> itemIdsSet = new LinkedHashSet<>(itemIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(itemIdsSet);

        mAction.onSelectionStateChange(itemIds);
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer =
                new ActionObserver() {
                    @Override
                    public void preProcessSelectedTabs(List<Tab> tabs) {
                        helper.notifyCalled();
                    }
                };
        mAction.addActionObserver(observer);

        assertTrue(mAction.perform());
        verify(mTabUngrouper).ungroupTabs(tabs, /* trailing= */ true, /* allowDialog= */ true);
        verify(mDelegate).hideByAction();

        helper.waitForOnly();
        mAction.removeActionObserver(observer);

        assertTrue(mAction.perform());
        verify(mTabUngrouper, times(2))
                .ungroupTabs(tabs, /* trailing= */ true, /* allowDialog= */ true);
        verify(mDelegate, times(2)).hideByAction();
        Assert.assertEquals(1, helper.getCallCount());
    }
}
