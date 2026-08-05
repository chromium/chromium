// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/** Unit tests for {@link TabListEditorPinAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorPinActionUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;

    private MockTabModel mTabModel;
    private TabListEditorAction mAction;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mAction =
                TabListEditorPinAction.createAction(
                        context, ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START);
        mTabModel = spy(new MockTabModel(mProfile, null));
        mAction.configure(() -> mTabModel, mSelectionDelegate, mDelegate, false);
    }

    @Test
    public void testInherentActionProperties() {
        assertEquals(
                R.id.tab_list_editor_pin_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        assertEquals(
                R.plurals.tab_selection_editor_pin_tabs,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        assertTrue(mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        assertEquals(
                R.plurals.accessibility_tab_selection_editor_pin_tabs,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        assertNotNull(mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    public void testActionDisabled_NoTabs() {
        mAction.onSelectionStateChange(Collections.emptyList());
        assertFalse(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    public void testActionEnabled_Pinning() {
        List<Integer> tabIds = Arrays.asList(5, 3, 7);
        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            Tab tab = mTabModel.addTab(id);
            tabs.add(tab);
        }

        Set<TabListEditorItemSelectionId> itemIdsSet =
                tabIds.stream()
                        .map(TabListEditorItemSelectionId::createTabId)
                        .collect(Collectors.toSet());
        when(mSelectionDelegate.getSelectedItems()).thenReturn(itemIdsSet);

        mAction.onSelectionStateChange(new ArrayList<>(itemIdsSet));
        assertTrue(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
        assertEquals(
                R.plurals.tab_selection_editor_pin_tabs,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        assertEquals(
                R.drawable.ic_keep_24dp,
                Shadows.shadowOf(mAction.getPropertyModel().get(TabListEditorActionProperties.ICON))
                        .getCreatedFromResId());

        for (Tab tab : tabs) {
            doAnswer(
                            (invocation) -> {
                                tab.setIsPinned(true);
                                return null;
                            })
                    .when(mTabModel)
                    .pinTab(tab.getId(), false);
        }

        assertTrue(mAction.perform());

        for (Tab tab : tabs) {
            verify(mTabModel).pinTab(tab.getId(), false);
        }

        // After pinning, the action should be to unpin.
        assertEquals(
                R.plurals.tab_selection_editor_unpin_tabs,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        assertEquals(
                R.drawable.ic_keep_off_24dp,
                Shadows.shadowOf(mAction.getPropertyModel().get(TabListEditorActionProperties.ICON))
                        .getCreatedFromResId());
    }

    @Test
    public void testActionEnabled_Unpinning() {
        List<Integer> tabIds = Arrays.asList(5, 3, 7);
        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            Tab tab = mTabModel.addTab(id);
            tab.setIsPinned(true);
            tabs.add(tab);
        }

        Set<TabListEditorItemSelectionId> itemIdsSet =
                tabIds.stream()
                        .map(TabListEditorItemSelectionId::createTabId)
                        .collect(Collectors.toSet());
        when(mSelectionDelegate.getSelectedItems()).thenReturn(itemIdsSet);

        mAction.onSelectionStateChange(new ArrayList<>(itemIdsSet));
        assertTrue(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
        assertEquals(
                R.plurals.tab_selection_editor_unpin_tabs,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        assertEquals(
                R.drawable.ic_keep_off_24dp,
                Shadows.shadowOf(mAction.getPropertyModel().get(TabListEditorActionProperties.ICON))
                        .getCreatedFromResId());

        for (Tab tab : tabs) {
            doAnswer(
                            (invocation) -> {
                                tab.setIsPinned(false);
                                return null;
                            })
                    .when(mTabModel)
                    .unpinTab(tab.getId());
        }

        assertTrue(mAction.perform());

        for (Tab tab : tabs) {
            verify(mTabModel).unpinTab(tab.getId());
        }

        // After unpinning, the action should be to pin.
        assertEquals(
                R.plurals.tab_selection_editor_pin_tabs,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        assertEquals(
                R.drawable.ic_keep_24dp,
                Shadows.shadowOf(mAction.getPropertyModel().get(TabListEditorActionProperties.ICON))
                        .getCreatedFromResId());
    }

    @Test
    public void testActionDisabled_MixedState() {
        List<Integer> tabIds = Arrays.asList(5, 3, 7);
        for (int i = 0; i < tabIds.size(); i++) {
            int id = tabIds.get(i);
            Tab tab = mTabModel.addTab(id);
            tab.setIsPinned(i % 2 == 0);
        }

        Set<TabListEditorItemSelectionId> itemIdsSet =
                tabIds.stream()
                        .map(TabListEditorItemSelectionId::createTabId)
                        .collect(Collectors.toSet());
        when(mSelectionDelegate.getSelectedItems()).thenReturn(itemIdsSet);

        mAction.onSelectionStateChange(new ArrayList<>(itemIdsSet));
        assertFalse(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
    }

    @Test
    public void testActionDisabled_WithTabGroups() {
        List<Integer> tabIds = Arrays.asList(5, 3, 7);

        for (int id : tabIds) {
            mTabModel.addTab(id);
        }
        mTabModel.getTabAt(1).setTabGroupId(new Token(1L, 2L));

        Set<TabListEditorItemSelectionId> itemIdsSet =
                tabIds.stream()
                        .map(TabListEditorItemSelectionId::createTabId)
                        .collect(Collectors.toSet());
        when(mSelectionDelegate.getSelectedItems()).thenReturn(itemIdsSet);

        mAction.onSelectionStateChange(new ArrayList<>(itemIdsSet));
        assertFalse(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
    }

    @Test
    public void testShouldHideEditorAfterAction() {
        assertTrue(mAction.shouldHideEditorAfterAction());
    }

    @Test
    public void testActionOrdering_Pinning() {
        // Create tabs in order: tab5 at index 0, tab3 at index 1, tab7 at index 2.
        Tab tab5 = mTabModel.addTab(5);
        Tab tab3 = mTabModel.addTab(3);
        Tab tab7 = mTabModel.addTab(7);

        // Select tabs in non-chronological order: tab7, tab5, tab3.
        List<TabListEditorItemSelectionId> selectedItems =
                Arrays.asList(
                        TabListEditorItemSelectionId.createTabId(7),
                        TabListEditorItemSelectionId.createTabId(5),
                        TabListEditorItemSelectionId.createTabId(3));
        when(mSelectionDelegate.getSelectedItems()).thenReturn(Set.copyOf(selectedItems));

        mAction.onSelectionStateChange(selectedItems);
        assertTrue(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));

        assertTrue(mAction.perform());

        // Verify pinning occurred in ascending TabModel index order: tab5 (0), tab3 (1), tab7 (2).
        InOrder inOrder = inOrder(mTabModel);
        inOrder.verify(mTabModel).pinTab(5, false);
        inOrder.verify(mTabModel).pinTab(3, false);
        inOrder.verify(mTabModel).pinTab(7, false);
    }

    @Test
    public void testActionOrdering_Unpinning() {
        // Create pinned tabs in order: tab5 at index 0, tab3 at index 1, tab7 at index 2.
        Tab tab5 = mTabModel.addTab(5);
        tab5.setIsPinned(true);
        Tab tab3 = mTabModel.addTab(3);
        tab3.setIsPinned(true);
        Tab tab7 = mTabModel.addTab(7);
        tab7.setIsPinned(true);

        // Select tabs in non-chronological order: tab7, tab5, tab3.
        List<TabListEditorItemSelectionId> selectedItems =
                Arrays.asList(
                        TabListEditorItemSelectionId.createTabId(7),
                        TabListEditorItemSelectionId.createTabId(5),
                        TabListEditorItemSelectionId.createTabId(3));
        when(mSelectionDelegate.getSelectedItems()).thenReturn(Set.copyOf(selectedItems));

        mAction.onSelectionStateChange(selectedItems);
        assertTrue(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));

        assertTrue(mAction.perform());

        // Verify unpinning occurred in descending TabModel index order: tab7 (2), tab3 (1), tab5
        // (0).
        // Since unpinTab inserts each tab at the start of unpinned tabs, reversing the unpin order
        // ensures tab5 ends up first, followed by tab3, then tab7.
        InOrder inOrder = inOrder(mTabModel);
        inOrder.verify(mTabModel).unpinTab(7);
        inOrder.verify(mTabModel).unpinTab(3);
        inOrder.verify(mTabModel).unpinTab(5);
    }
}
