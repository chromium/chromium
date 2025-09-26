// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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

import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

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
    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    @Mock private TabListEditorAction.ActionDelegate mActionDelegate;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private Profile mProfile;
    @Mock private Drawable mDrawable;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    private TabListEditorAddToGroupAction mAction;
    private Token mTabGroupId;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        mTabGroupId = Token.createRandom();
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilterSupplier.get()).thenReturn(mTabGroupModelFilter);
        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(mProfile)).thenReturn(true);

        mAction =
                new TabListEditorAddToGroupAction(
                        mActivity,
                        mTabGroupCreationDialogManager,
                        MENU_ONLY,
                        TEXT,
                        START,
                        mDrawable,
                        (a, b, c, d, e, f, g, h) -> mCoordinator);
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
        List<TabListEditorItemSelectionId> itemIds =
                new ArrayList<>(
                        Arrays.asList(
                                TabListEditorItemSelectionId.createTabId(1),
                                TabListEditorItemSelectionId.createTabId(2)));
        when(mSelectionDelegate.getSelectedItemsAsList()).thenReturn(itemIds);
        mAction.configure(mTabGroupModelFilterSupplier, mSelectionDelegate, mActionDelegate, false);

        assertTrue(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(2, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        itemIds.clear();
        when(mSelectionDelegate.getSelectedItemsAsList()).thenReturn(itemIds);
        mAction.onSelectionStateChange(itemIds);

        assertFalse(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    public void testOnSelectionStateChange_sharedGroupSelected() {
        List<TabListEditorItemSelectionId> itemIds =
                new ArrayList<>(
                        Arrays.asList(
                                TabListEditorItemSelectionId.createTabId(1),
                                TabListEditorItemSelectionId.createTabId(2)));
        when(mTab1.getTabGroupId()).thenReturn(mTabGroupId);

        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = "collaborationId";

        when(mTabGroupSyncService.getGroup(new LocalTabGroupId(mTabGroupId)))
                .thenReturn(savedTabGroup);
        when(mSelectionDelegate.getSelectedItemsAsList()).thenReturn(itemIds);
        mAction.configure(mTabGroupModelFilterSupplier, mSelectionDelegate, mActionDelegate, false);

        assertFalse(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(2, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        itemIds.clear();
        when(mSelectionDelegate.getSelectedItemsAsList()).thenReturn(itemIds);
        mAction.onSelectionStateChange(itemIds);

        assertFalse(mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    public void testPerformAction() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(1);

        assertTrue(mAction.performAction(tabs, Collections.emptyList()));
        verify(mCoordinator).showBottomSheet(tabs);
        verify(mTabGroupCreationDialogManager, never()).showDialog(any(), any());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testPerformAction_NoTabGroups() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);

        assertTrue(mAction.performAction(tabs, Collections.emptyList()));
        verify(mTabGroupModelFilter).mergeListOfTabsToGroup(eq(tabs), eq(mTab1), anyInt());
        verify(mTabGroupCreationDialogManager)
                .showDialog(eq(mTab1.getTabGroupId()), eq(mTabGroupModelFilter));
        verify(mCoordinator, never()).showBottomSheet(tabs);
    }

    @Test(expected = AssertionError.class)
    public void testPerformAction_NoTabs() {
        mAction.performAction(Collections.emptyList(), Collections.emptyList());
    }

    @Test
    public void testShouldHideEditorAfterAction() {
        assertTrue(mAction.shouldHideEditorAfterAction());
    }
}
