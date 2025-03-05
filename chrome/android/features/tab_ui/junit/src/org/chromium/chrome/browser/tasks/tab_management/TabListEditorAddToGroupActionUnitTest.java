// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType.TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition.START;
import static org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode.MENU_ONLY;
import static org.chromium.chrome.browser.tasks.tab_management.TabListEditorActionProperties.DESTROYABLE;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import androidx.annotation.StringRes;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link TabListEditorAddToGroupAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorAddToGroupActionUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    @Mock private TabGroupListBottomSheetCoordinator mCoordinator;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private Supplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;
    @Mock private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock private TabListEditorAction.ActionDelegate mActionDelegate;
    @Mock private Drawable mDrawable;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    private TabListEditorAddToGroupAction mAction;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilterSupplier.get()).thenReturn(mTabGroupModelFilter);
        mAction =
                new TabListEditorAddToGroupAction(
                        mActivity,
                        mTabGroupCreationDialogManager,
                        MENU_ONLY,
                        TEXT,
                        START,
                        mDrawable,
                        (a, b, c, d, e, f) -> mCoordinator);
        mAction.configure(mTabGroupModelFilterSupplier, mSelectionDelegate, mActionDelegate, false);
    }

    @Test
    public void testConfigure() {
        verify(mTabGroupModelFilter).addTabGroupObserver(any());
        verify(mTabModel).addObserver(any());
    }

    @Test
    public void testDestroy() {
        Destroyable destroyable = mAction.getPropertyModel().get(DESTROYABLE);
        destroyable.destroy();
        verify(mTabGroupModelFilter).removeTabGroupObserver(any());
        verify(mTabModel).removeObserver(any());
    }

    @Test
    public void testCreateAction() {
        TabListEditorAction action =
                TabListEditorAddToGroupAction.createAction(
                        mActivity, mTabGroupCreationDialogManager, MENU_ONLY, TEXT, START);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(1);
        action.configure(mTabGroupModelFilterSupplier, mSelectionDelegate, mActionDelegate, false);

        assertTrue(action instanceof TabListEditorAddToGroupAction);
        assertEquals(
                R.plurals.add_tab_to_group_menu_item,
                action.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));

        @StringRes
        int resId =
                action.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID);
        assertEquals(R.plurals.accessibility_add_tab_to_group_menu_item, resId);
        assertEquals(
                MENU_ONLY, action.getPropertyModel().get(TabListEditorActionProperties.SHOW_MODE));
        assertEquals(
                TEXT, action.getPropertyModel().get(TabListEditorActionProperties.BUTTON_TYPE));
        assertEquals(
                START, action.getPropertyModel().get(TabListEditorActionProperties.ICON_POSITION));
        assertNotNull(action.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    public void testCreateAction_NoTabGroups() {
        TabListEditorAction action =
                TabListEditorAddToGroupAction.createAction(
                        mActivity, mTabGroupCreationDialogManager, MENU_ONLY, TEXT, START);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);
        action.configure(mTabGroupModelFilterSupplier, mSelectionDelegate, mActionDelegate, false);

        assertTrue(action instanceof TabListEditorAddToGroupAction);
        assertEquals(
                R.plurals.add_tab_to_new_group_menu_item,
                action.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));

        @StringRes
        int resId =
                action.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID);
        assertEquals(R.plurals.accessibility_add_tab_to_new_group_menu_item, resId);
        assertEquals(
                MENU_ONLY, action.getPropertyModel().get(TabListEditorActionProperties.SHOW_MODE));
        assertEquals(
                TEXT, action.getPropertyModel().get(TabListEditorActionProperties.BUTTON_TYPE));
        assertEquals(
                START, action.getPropertyModel().get(TabListEditorActionProperties.ICON_POSITION));
        assertNotNull(action.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    public void testOnSelectionStateChange() {
        List<Integer> tabIds = new ArrayList<>(Arrays.asList(1, 2));
        when(mSelectionDelegate.getSelectedItemsAsList()).thenReturn(tabIds);
        mAction.configure(mTabGroupModelFilterSupplier, mSelectionDelegate, mActionDelegate, false);

        assertTrue(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(2, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        tabIds.clear();
        when(mSelectionDelegate.getSelectedItemsAsList()).thenReturn(tabIds);
        mAction.onSelectionStateChange(tabIds);

        assertFalse(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    public void testPerformAction() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(1);

        assertTrue(mAction.performAction(tabs));
        verify(mCoordinator).showBottomSheet(tabs);
        verify(mTabGroupCreationDialogManager, never()).showDialog(anyInt(), any());
    }

    @Test
    public void testPerformAction_NoTabGroups() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);

        assertTrue(mAction.performAction(tabs));
        verify(mTabGroupModelFilter).mergeListOfTabsToGroup(eq(tabs), eq(mTab1), anyBoolean());
        verify(mTabGroupCreationDialogManager)
                .showDialog(eq(mTab1.getRootId()), eq(mTabGroupModelFilter));
        verify(mCoordinator, never()).showBottomSheet(tabs);
    }

    @Test(expected = AssertionError.class)
    public void testPerformAction_NoTabs() {
        mAction.performAction(new ArrayList<>());
    }

    @Test
    public void testShouldHideEditorAfterAction() {
        assertTrue(mAction.shouldHideEditorAfterAction());
    }
}
