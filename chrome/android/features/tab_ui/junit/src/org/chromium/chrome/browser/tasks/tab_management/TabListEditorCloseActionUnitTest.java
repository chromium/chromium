// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorActionUnitTestHelper.TabIdGroup;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorActionUnitTestHelper.TabListHolder;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;
import java.util.stream.Collectors;

/** Unit tests for {@link TabListEditorCloseAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorCloseActionUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mGroupFilter;
    @Mock private TabRemover mTabRemover;
    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;

    private MockTabModel mTabModel;
    private TabListEditorAction mAction;

    @Before
    public void setUp() {
        mAction =
                TabListEditorCloseAction.createAction(
                        ApplicationProvider.getApplicationContext(),
                        ShowMode.MENU_ONLY,
                        ButtonType.TEXT,
                        IconPosition.START);
        mTabModel = spy(new MockTabModel(mProfile, null));
        mTabModel.setTabRemoverForTesting(mTabRemover);
        when(mGroupFilter.getTabModel()).thenReturn(mTabModel);
    }

    private void configure(boolean actionOnRelatedTabs) {
        mAction.configure(() -> mGroupFilter, mSelectionDelegate, mDelegate, actionOnRelatedTabs);
    }

    @Test
    public void testInherentActionProperties() {
        assertEquals(
                R.id.tab_list_editor_close_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        assertEquals(
                R.plurals.tab_selection_editor_close_tabs,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        assertEquals(
                true,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        assertEquals(
                R.plurals.accessibility_tab_selection_editor_close_tabs,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        assertNotNull(mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    public void testCloseActionNoTabs() {
        configure(false);
        mAction.onSelectionStateChange(Collections.emptyList());
        assertEquals(false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    public void testCloseActionWithOneTab() {
        configure(false);
        List<Integer> tabIds = Arrays.asList(5, 3, 7);
        List<Tab> tabs =
                tabIds.stream().map(id -> mTabModel.addTab(id)).collect(Collectors.toList());
        Set<TabListEditorItemSelectionId> itemIdsSet =
                Collections.singleton(TabListEditorItemSelectionId.createTabId(3));
        when(mSelectionDelegate.getSelectedItems()).thenReturn(itemIdsSet);

        mAction.onSelectionStateChange(Arrays.asList(TabListEditorItemSelectionId.createTabId(3)));
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(1, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        assertTrue(mAction.perform());

        verify(mTabRemover)
                .closeTabs(
                        TabClosureParams.closeTabs(List.of(tabs.get(1))).build(),
                        /* allowDialog= */ true);
        verify(mDelegate).hideByAction();
    }

    @Test
    public void testCloseActionWithTabs() throws TimeoutException {
        configure(false);
        List<Integer> tabIds = Arrays.asList(5, 3, 7);
        List<TabListEditorItemSelectionId> itemIds =
                Arrays.asList(
                        TabListEditorItemSelectionId.createTabId(5),
                        TabListEditorItemSelectionId.createTabId(3),
                        TabListEditorItemSelectionId.createTabId(7));
        List<Tab> tabs =
                tabIds.stream().map(id -> mTabModel.addTab(id)).collect(Collectors.toList());
        Set<TabListEditorItemSelectionId> itemIdsSet = new LinkedHashSet<>(itemIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(itemIdsSet);

        mAction.onSelectionStateChange(itemIds);
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer = tabs1 -> helper.notifyCalled();
        mAction.addActionObserver(observer);

        assertTrue(mAction.perform());

        verify(mTabRemover)
                .closeTabs(TabClosureParams.closeTabs(tabs).build(), /* allowDialog= */ true);
        verify(mDelegate).hideByAction();

        helper.waitForOnly();
        mAction.removeActionObserver(observer);

        assertTrue(mAction.perform());

        verify(mTabRemover, times(2))
                .closeTabs(TabClosureParams.closeTabs(tabs).build(), /* allowDialog= */ true);
        verify(mDelegate, times(2)).hideByAction();
        assertEquals(1, helper.getCallCount());
    }

    @Test
    public void testCloseActionWithTabGroups_ActionOnRelatedTabs() {
        final boolean actionOnRelatedTabs = true;
        configure(actionOnRelatedTabs);
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {0},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5, 3},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {4},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {8, 7, 6},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {1},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        /* tabGroupSyncService= */ null,
                        mSelectionDelegate,
                        tabIdGroups,
                        true);

        assertEquals(3, holder.getSelectedTabs().size());
        assertEquals(5, holder.getSelectedTabs().get(0).getId());
        assertEquals(8, holder.getSelectedTabs().get(1).getId());
        assertEquals(1, holder.getSelectedTabs().get(2).getId());
        mAction.onSelectionStateChange(holder.getSelectedItemIds());
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(6, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        assertEquals(6, holder.getSelectedAndRelatedTabs().size());
        assertEquals(5, holder.getSelectedAndRelatedTabs().get(0).getId());
        assertEquals(3, holder.getSelectedAndRelatedTabs().get(1).getId());
        assertEquals(8, holder.getSelectedAndRelatedTabs().get(2).getId());
        assertEquals(7, holder.getSelectedAndRelatedTabs().get(3).getId());
        assertEquals(6, holder.getSelectedAndRelatedTabs().get(4).getId());
        assertEquals(1, holder.getSelectedAndRelatedTabs().get(5).getId());

        assertTrue(mAction.perform());

        verify(mTabRemover)
                .closeTabs(
                        TabClosureParams.closeTabs(holder.getSelectedAndRelatedTabs())
                                .hideTabGroups(true)
                                .build(),
                        /* allowDialog= */ true);
        verify(mDelegate).hideByAction();
    }

    @Test
    public void testCloseActionWithTabGroups_NoActionOnRelatedTabs() {
        final boolean actionOnRelatedTabs = false;
        configure(actionOnRelatedTabs);
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {0},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5, 3},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {4},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {8, 7, 6},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {1},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        /* tabGroupSyncService= */ null,
                        mSelectionDelegate,
                        tabIdGroups,
                        true);

        assertEquals(3, holder.getSelectedTabs().size());
        assertEquals(5, holder.getSelectedTabs().get(0).getId());
        assertEquals(8, holder.getSelectedTabs().get(1).getId());
        assertEquals(1, holder.getSelectedTabs().get(2).getId());
        mAction.onSelectionStateChange(holder.getSelectedItemIds());
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        assertTrue(mAction.perform());

        verify(mTabRemover)
                .closeTabs(
                        TabClosureParams.closeTabs(holder.getSelectedTabs()).build(),
                        /* allowDialog= */ true);
        verify(mDelegate).hideByAction();
    }
}
