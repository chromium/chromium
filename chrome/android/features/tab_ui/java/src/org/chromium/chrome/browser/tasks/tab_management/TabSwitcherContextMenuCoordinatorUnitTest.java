// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;

import android.app.Activity;
import android.content.res.Resources;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.util.List;

/** Unit tests for {@link TabSwitcherContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
public class TabSwitcherContextMenuCoordinatorUnitTest {
    private static @TabId final int TAB_ID = 1;
    private static final int MENU_WIDTH = 300;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabBookmarker mTabBookmarker;
    @Mock private Supplier<TabModel> mTabModelSupplier;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupListBottomSheetCoordinator mTabGroupListBottomSheetCoordinator;
    @Mock private TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private TabListEditorManager mTabListEditorManager;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private Tab mTab;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private Profile mProfile;
    @Mock private Resources mResources;

    private TabSwitcherContextMenuCoordinator mCoordinator;
    private ModelList mMenuItemList;
    private Activity mActivity;
    private GURL mUrl;
    private Token mTabGroupId;

    @Before
    public void setUp() {
        mTabGroupId = Token.createRandom();

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(1);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModelSupplier.get()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
        when(mTab.getTabGroupId()).thenReturn(mTabGroupId);
        when(mResources.getDimensionPixelSize(R.dimen.tab_strip_group_context_menu_max_width))
                .thenReturn(MENU_WIDTH);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mCoordinator =
                new TabSwitcherContextMenuCoordinator(
                        mActivity,
                        mTabBookmarker,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mTabGroupSyncService,
                        mCollaborationService,
                        mTabListEditorManager);
        mMenuItemList = new ModelList();
        when(mTabModel.getTabById(anyInt())).thenReturn(mTab);
        when(mTab.getId()).thenReturn(TAB_ID);
    }

    @Test
    public void testShowMenu() {
        RectProvider rectProvider = new RectProvider();
        mCoordinator = spy(mCoordinator);
        doNothing()
                .when(mCoordinator)
                .createAndShowMenu(
                        any(), any(), anyBoolean(), anyBoolean(), anyInt(), anyInt(), any());
        mCoordinator.showMenu(rectProvider, TAB_ID);
        verify(mCoordinator)
                .createAndShowMenu(
                        eq(rectProvider),
                        eq(TAB_ID),
                        eq(true),
                        eq(false),
                        eq(R.style.TabSwitcherContextMenuAnimation),
                        eq(HorizontalOrientation.LAYOUT_DIRECTION),
                        eq(mActivity));
    }

    @Test
    public void testGetMenuItemClickedCallback_shareTab() {
        TabSwitcherContextMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabSwitcherContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarker,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mTabListEditorManager);

        callback.onClick(R.id.share_tab, TAB_ID, null);
        verify(mShareDelegate).share(mTab, false, TAB_STRIP_CONTEXT_MENU);
    }

    @Test
    public void testGetMenuItemClickedCallback_addToTabGroup() {
        TabSwitcherContextMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabSwitcherContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarker,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mTabListEditorManager);

        callback.onClick(R.id.add_to_tab_group, TAB_ID, null);
        verify(mTabGroupListBottomSheetCoordinator).showBottomSheet(List.of(mTab));
    }

    @Test
    public void testGetMenuItemClickedCallback_addToNewTabGroup() {
        TabSwitcherContextMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabSwitcherContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarker,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mTabListEditorManager);

        callback.onClick(R.id.add_to_new_tab_group, TAB_ID, null);
        verify(mTabGroupCreationDialogManager).showDialog(mTabGroupId, mTabGroupModelFilter);
    }

    @Test
    public void testGetMenuItemClickedCallback_addToBookmarks() {
        TabSwitcherContextMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabSwitcherContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarker,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mTabListEditorManager);

        callback.onClick(R.id.add_to_bookmarks, TAB_ID, null);
        verify(mTabBookmarker).addOrEditBookmark(mTab);
    }

    @Test
    public void testGetMenuItemClickedCallback_selectTabs() {
        TabSwitcherContextMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabSwitcherContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarker,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mTabListEditorManager);

        callback.onClick(R.id.select_tabs, TAB_ID, null);
        verify(mTabListEditorManager).showTabListEditor();
    }

    @Test
    public void testGetMenuItemClickedCallback_closeTab() {
        TabSwitcherContextMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabSwitcherContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarker,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mTabListEditorManager);
        callback.onClick(R.id.close_tab, TAB_ID, null);
        verify(mTabRemover).closeTabs(any(), eq(true));
    }

    @Test
    public void testGetMenuItemClickedCallback_invalidTabId() {
        TabSwitcherContextMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabSwitcherContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarker,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mTabListEditorManager);

        callback.onClick(R.id.share_tab, Tab.INVALID_TAB_ID, null);
        verify(mShareDelegate, never()).share(any(), anyBoolean(), anyInt());
    }

    @Test
    public void testGetMenuItemClickedCallback_tabNotFound() {
        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        TabSwitcherContextMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabSwitcherContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarker,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mTabListEditorManager);

        callback.onClick(R.id.share_tab, TAB_ID, null);
        verify(mShareDelegate, never()).share(any(), anyBoolean(), anyInt());
    }

    @Test
    public void testBuildMenuActionItems_withGroups() {
        mUrl = new GURL("localhost://");
        when(mTab.getUrl()).thenReturn(mUrl);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(5, mMenuItemList.size());
        assertEquals(
                R.string.add_tab_to_group,
                mMenuItemList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.add_to_bookmarks,
                mMenuItemList.get(1).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.share, mMenuItemList.get(2).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.menu_select_tabs,
                mMenuItemList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.close_tab,
                mMenuItemList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
    }

    @Test
    public void testBuildMenuActionItems_noGroups() {
        mUrl = new GURL("localhost://");
        when(mTab.getUrl()).thenReturn(mUrl);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(5, mMenuItemList.size());
        assertEquals(
                R.string.menu_add_to_new_group,
                mMenuItemList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.add_to_bookmarks,
                mMenuItemList.get(1).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.share, mMenuItemList.get(2).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.menu_select_tabs,
                mMenuItemList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.close_tab,
                mMenuItemList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
    }

    @Test
    public void testBuildMenuActionItems_sharingDisabled() {
        mUrl = new GURL("chrome://");
        when(mTab.getUrl()).thenReturn(mUrl);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(4, mMenuItemList.size());
        assertEquals(
                R.string.add_tab_to_group,
                mMenuItemList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.add_to_bookmarks,
                mMenuItemList.get(1).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.menu_select_tabs,
                mMenuItemList.get(2).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.close_tab,
                mMenuItemList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
    }

    @Test
    public void testBuildMenuActionItems_tabNotFound() {
        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);
        assertEquals(0, mMenuItemList.size());
    }

    @Test
    public void testGetMenuWidth() {
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_switcher_context_menu_max_width),
                // Provide an arbitrary value for anchorViewWidthPx for the test.
                mCoordinator.getMenuWidth(/* anchorViewWidthPx= */ 0));
    }

    @Test
    public void testGetCollaborationIdOrNull_tabNotFound() {
        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        assertNull(mCoordinator.getCollaborationIdOrNull(TAB_ID));
    }
}
