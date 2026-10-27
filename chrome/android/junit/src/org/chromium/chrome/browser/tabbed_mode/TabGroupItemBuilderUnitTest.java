// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabbed_mode.AppMenuUnitTestUtils.findItemById;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemTheme;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelType;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemWithSubmenuProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTabGroupItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTabItemProperties;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabGroupItemBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
@DisableFeatures({TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS})
public class TabGroupItemBuilderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Profile mProfile;
    @Mock private RoundedIconGenerator mRoundedIconGenerator;
    @Mock private FaviconHelper.DefaultFaviconHelper mDefaultFaviconHelper;
    private FaviconHelper mFaviconHelper;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private AppMenuItemTheme mAppMenuItemTheme;
    @Mock private Tab mTab;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabList mTabList;
    @Mock private Tab mClosingTab;
    @Mock private TabList mComprehensiveTabList;
    @Mock private TabModelSelector mSelectorWindow2;
    @Mock private TabModel mModelWindow2;
    @Mock private TabList mComprehensiveModelWindow2;

    private TabGroupItemBuilder mTabGroupItemBuilder;
    @Mock private TabModel mTabModel;
    @Mock private TabModel mIncognitoTabModel;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        mFaviconHelper = new FaviconHelper();

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mIncognitoTabModel.getProfile()).thenReturn(mProfile);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        when(mTabModel.containsTabGroup(any()))
                .thenAnswer(
                        inv -> {
                            TabModel model = (TabModel) inv.getMock();
                            Set<Token> ids = model.getAllTabGroupIds();
                            return ids.contains(inv.getArgument(0));
                        });

        mTabGroupItemBuilder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        SupplierUtils.ofNull());
    }

    private Tab setUpMockTabGroup(TabModel tabModel, boolean isIncognito, boolean hasGroupId) {
        Token token1 = new Token(1L, 1L);
        when(tabModel.getCount()).thenReturn(2);
        when(tabModel.getTabGroupCount()).thenReturn(1);
        when(tabModel.getAllTabGroupIds()).thenReturn(Set.of(token1));
        when(tabModel.getTabGroupTitle(token1)).thenReturn("Group 1");
        when(tabModel.getTabGroupColorWithFallback(token1)).thenReturn(TabGroupColorId.BLUE);

        Tab tab = Mockito.mock(Tab.class);
        when(tab.getId()).thenReturn(101);
        when(tab.getTitle()).thenReturn("Tab 1");
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.getTabGroupId()).thenReturn(hasGroupId ? token1 : null);
        when(tab.isOffTheRecord()).thenReturn(isIncognito);
        when(tab.isInitialized()).thenReturn(true);
        when(tab.isDestroyed()).thenReturn(false);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());

        when(tabModel.getTabsInGroup(token1)).thenReturn(Arrays.asList(tab));

        return tab;
    }

    @Test
    public void testTabGroupsSubmenu_WithGroups() {
        Token token1 = new Token(1L, 1L);
        when(mTabModel.getTabGroupCount()).thenReturn(1);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(token1));
        when(mTabModel.getTabGroupTitle(token1)).thenReturn("Group 1");
        when(mTabModel.getTabGroupColorWithFallback(token1)).thenReturn(TabGroupColorId.BLUE);

        Tab tab1 = Mockito.mock(Tab.class);
        when(tab1.getId()).thenReturn(101);
        when(tab1.getTitle()).thenReturn("Tab 1");
        when(tab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        Tab tab2 = Mockito.mock(Tab.class);
        when(tab2.getId()).thenReturn(102);
        when(tab2.getTitle()).thenReturn("Tab 2");
        when(tab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);

        when(mTabModel.getTabsInGroup(token1)).thenReturn(Arrays.asList(tab1, tab2));

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ListItem tabGroupsParent = mTabGroupItemBuilder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        // The submenu has items: Create new tab group, Add to group, Divider, Header, Group
        assertEquals(5, tabGroupsSubmenuItems.size());

        ListItem createNewGroup = tabGroupsSubmenuItems.get(0);
        assertEquals(
                R.id.create_new_tab_group_menu_id,
                createNewGroup.model.get(AppMenuItemProperties.MENU_ITEM_ID));

        ListItem addToGroup = tabGroupsSubmenuItems.get(1);
        assertEquals(
                R.id.add_to_group_menu_id,
                addToGroup.model.get(AppMenuItemProperties.MENU_ITEM_ID));

        ListItem groupItem = tabGroupsSubmenuItems.get(4);
        assertEquals(
                R.id.tab_group_menu_item_id,
                groupItem.model.get(AppMenuItemProperties.MENU_ITEM_ID));

        List<ListItem> tabsSubmenuItems =
                groupItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        assertEquals(2, tabsSubmenuItems.size());

        ListItem tabItem1 = tabsSubmenuItems.get(0);
        assertEquals(AppMenuHandler.AppMenuItemType.TAB, tabItem1.type);
        assertEquals(AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU, groupItem.type);
        assertEquals(101, tabItem1.model.get(AppMenuTabItemProperties.TAB_ID));
        assertEquals(JUnitTestGURLs.URL_1, tabItem1.model.get(AppMenuTabItemProperties.TAB_URL));

        ListItem tabItem2 = tabsSubmenuItems.get(1);
        assertEquals(AppMenuHandler.AppMenuItemType.TAB, tabItem2.type);
        assertEquals(102, tabItem2.model.get(AppMenuTabItemProperties.TAB_ID));
        assertEquals(JUnitTestGURLs.URL_2, tabItem2.model.get(AppMenuTabItemProperties.TAB_URL));
    }

    @Test
    public void testTabGroupsSubmenu_Favicons_GroupedNonIncognito() {
        setUpMockTabGroup(mTabModel, /* isIncognito= */ false, /* hasGroupId= */ true);
        GURL tabUrl = JUnitTestGURLs.URL_1;

        // Intercept the JNI callback and invoke it synchronously with a mock favicon bitmap.
        Answer<Boolean> faviconCallbackAnswer =
                invocation -> {
                    FaviconHelper.FaviconImageCallback callback = invocation.getArgument(5);
                    callback.onFaviconAvailable(
                            Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888), tabUrl);
                    return true;
                };

        // Should call {@code getForeignFaviconImageForURL()} because it is not incognito.
        doAnswer(faviconCallbackAnswer)
                .when(mFaviconHelperJniMock)
                .getForeignFaviconImageForURL(
                        eq(1L), eq(mProfile), eq(tabUrl), anyInt(), eq(false), any());

        ListItem tabGroupsParent = mTabGroupItemBuilder.buildTabGroupsParentItem(mTab);

        ListItem tabGroupItem =
                findItemById(
                        tabGroupsParent
                                .model
                                .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                                .get(),
                        R.id.tab_group_menu_item_id);
        assertNotNull(tabGroupItem);

        // Get the first tab item in that group.
        LazyOneshotSupplier<Drawable> iconSupplier =
                tabGroupItem
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get()
                        .get(0)
                        .model
                        .get(AppMenuItemProperties.ICON_SUPPLIER);

        Drawable drawable = iconSupplier.get();
        assertNotNull(drawable);
    }

    @Test
    public void testTabGroupsSubmenu_Favicons_GroupedIncognito() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        setUpMockTabGroup(mIncognitoTabModel, /* isIncognito= */ true, /* hasGroupId= */ true);
        GURL tabUrl = JUnitTestGURLs.URL_1;

        when(mTab.isIncognito()).thenReturn(true);

        // Intercept the JNI callback and invoke it synchronously with a mock favicon bitmap.
        Answer<Boolean> faviconCallbackAnswer =
                invocation -> {
                    FaviconHelper.FaviconImageCallback callback = invocation.getArgument(5);
                    callback.onFaviconAvailable(
                            Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888), tabUrl);
                    return true;
                };

        // Should call {@code getLocalFaviconImageForURL()} because it is incognito.
        doAnswer(faviconCallbackAnswer)
                .when(mFaviconHelperJniMock)
                .getLocalFaviconImageForURL(
                        eq(1L), eq(mProfile), eq(tabUrl), anyInt(), eq(false), any());

        ListItem tabGroupsParent = mTabGroupItemBuilder.buildTabGroupsParentItem(mTab);

        ListItem tabGroupItem =
                findItemById(
                        tabGroupsParent
                                .model
                                .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                                .get(),
                        R.id.tab_group_menu_item_id);
        assertNotNull(tabGroupItem);

        // Get the first tab item in that group.
        LazyOneshotSupplier<Drawable> iconSupplier =
                tabGroupItem
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get()
                        .get(0)
                        .model
                        .get(AppMenuItemProperties.ICON_SUPPLIER);

        Drawable drawable = iconSupplier.get();
        assertNotNull(drawable);
    }

    @Test
    public void testBuildTabGroupsParentItem_EmptyGroups() {
        when(mTabModel.getTabGroupCount()).thenReturn(0);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of());

        ListItem tabGroupsParent = mTabGroupItemBuilder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        assertEquals(1, tabGroupsSubmenuItems.size());
        assertEquals(
                R.id.create_new_tab_group_menu_id,
                tabGroupsSubmenuItems.get(0).model.get(AppMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testBuildTabGroupsParentItem_NullCurrentTab_NoGroups() {
        when(mTabModel.getTabGroupCount()).thenReturn(0);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of());

        assertFalse(mTabGroupItemBuilder.shouldShowTabGroupsParentItem(/* currentTab= */ null));
    }

    @Test
    public void testBuildTabGroupsParentItem_NullCurrentTab_WithGroups() {
        Token token1 = new Token(1L, 1L);
        when(mTabModel.getTabGroupCount()).thenReturn(1);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(token1));

        assertTrue(mTabGroupItemBuilder.shouldShowTabGroupsParentItem(/* currentTab= */ null));

        ListItem tabGroupsParent =
                mTabGroupItemBuilder.buildTabGroupsParentItem(/* currentTab= */ null);
        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        assertEquals(4, tabGroupsSubmenuItems.size());
        assertEquals(
                R.id.add_to_group_menu_id,
                tabGroupsSubmenuItems.get(0).model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.tab_group_menu_item_id,
                tabGroupsSubmenuItems.get(3).model.get(AppMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testBuildAddToGroupItem_Strings() {
        Token token1 = new Token(1L, 1L);
        Token token2 = new Token(2L, 2L);

        // Case 1: Tab is in token1, model has token1 and token2 (other group exists).
        when(mTab.getTabGroupId()).thenReturn(token1);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(token1, token2));
        ListItem item1 = mTabGroupItemBuilder.buildAddToGroupItem(mTab, /* showIcon= */ false);
        assertEquals(
                mContext.getString(R.string.menu_move_tab_to_group),
                item1.model.get(AppMenuItemProperties.TITLE));

        // Case 2: Tab is not in group, model has token1 (other group exists).
        when(mTab.getTabGroupId()).thenReturn(null);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(token1));
        ListItem item2 = mTabGroupItemBuilder.buildAddToGroupItem(mTab, /* showIcon= */ false);
        assertEquals(
                mContext.getString(R.string.menu_add_tab_to_group),
                item2.model.get(AppMenuItemProperties.TITLE));
    }

    @Test
    public void testShouldShowAddToGroup() {
        Token token1 = new Token(1L, 1L);
        Token token2 = new Token(2L, 2L);

        // Tab in token1, model only has token1 -> false (no other group).
        when(mTab.getTabGroupId()).thenReturn(token1);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(token1));
        assertFalse(mTabGroupItemBuilder.shouldShowAddToGroup(mTab));

        // Tab in token1, model has token1 and token2 -> true.
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(token1, token2));
        assertTrue(mTabGroupItemBuilder.shouldShowAddToGroup(mTab));

        // Tab not in group, model has no groups -> false.
        when(mTab.getTabGroupId()).thenReturn(null);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of());
        assertFalse(mTabGroupItemBuilder.shouldShowAddToGroup(mTab));

        // Tab not in group, model has token1 -> true.
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(token1));
        assertTrue(mTabGroupItemBuilder.shouldShowAddToGroup(mTab));
    }

    @Test
    public void testBuildSubmenuForAddToGroup() {
        Token token1 = new Token(1L, 1L);
        Token token2 = new Token(2L, 2L);

        when(mTab.getTabGroupId()).thenReturn(token1);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(token1, token2));
        doReturn(true).when(mTabModel).containsTabGroup(token2);
        when(mTabModel.getTabGroupTitle(token2)).thenReturn("Group 2");
        when(mTabModel.getTabGroupColorWithFallback(token2)).thenReturn(TabGroupColorId.BLUE);

        ListItem addToGroupItem =
                mTabGroupItemBuilder.buildAddToGroupItem(mTab, /* showIcon= */ true);
        List<ListItem> submenuItems =
                addToGroupItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        // Should only contain the other group (Group 2).
        assertEquals(1, submenuItems.size());
        ListItem groupItem = submenuItems.get(0);
        assertEquals(
                R.id.add_to_existing_group_menu_item_id,
                groupItem.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals("Group 2", groupItem.model.get(AppMenuItemProperties.TITLE));
        assertEquals(token2, groupItem.model.get(AppMenuTabGroupItemProperties.TAB_GROUP_ID));
        assertNotNull(groupItem.model.get(AppMenuItemProperties.ICON));
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.SUBMENUS_IN_APP_MENU,
        ChromeFeatureList.SUBMENUS_IN_APP_MENU_LFF
    })
    public void testBuildAddToGroupItem_SubmenusDisabled() {
        when(mTab.getTabGroupId()).thenReturn(null);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of());
        when(mTabModel.getTabGroupCount()).thenReturn(0);
        assertTrue(mTabGroupItemBuilder.shouldShowAddToGroup(mTab));

        ListItem item = mTabGroupItemBuilder.buildAddToGroupItem(mTab, /* showIcon= */ false);
        assertEquals(AppMenuHandler.AppMenuItemType.STANDARD_NO_ICON, item.type);
        assertEquals(
                mContext.getString(R.string.menu_add_tab_to_new_group),
                item.model.get(AppMenuItemProperties.TITLE));
    }

    @Test
    public void testBuildSubmenuForAddToGroup_withGroupWindowChecker() {
        Token token1 = Token.createRandom();
        Token token2 = Token.createRandom();

        SavedTabGroup group1 = new SavedTabGroup();
        group1.localId = new LocalTabGroupId(token1);
        group1.title = "Group 1";
        group1.updateTimeMs = 100L;
        group1.color = TabGroupColorId.BLUE;
        group1.savedTabs.add(new SavedTabGroupTab());

        SavedTabGroup group2 = new SavedTabGroup();
        group2.localId = new LocalTabGroupId(token2);
        group2.title = "Group 2";
        group2.updateTimeMs = 200L;
        group2.color = TabGroupColorId.RED;
        group2.savedTabs.add(new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"id1", "id2"});
        when(mTabGroupSyncService.getGroup("id1")).thenReturn(group1);
        when(mTabGroupSyncService.getGroup("id2")).thenReturn(group2);

        Tab tab1 = Mockito.mock(Tab.class);
        when(tab1.getTabGroupId()).thenReturn(token1);
        Tab tab2 = Mockito.mock(Tab.class);
        when(tab2.getTabGroupId()).thenReturn(token2);

        TabList tabList = Mockito.mock(TabList.class);
        List<Tab> tabs = List.of(tab1, tab2);
        when(tabList.iterator()).thenAnswer(invocation -> tabs.iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(tabList);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem addToGroupItem = builder.buildAddToGroupItem(null, /* showIcon= */ true);
        List<ListItem> submenuItems =
                addToGroupItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        assertEquals(2, submenuItems.size());
        assertEquals(
                token2, submenuItems.get(0).model.get(AppMenuTabGroupItemProperties.TAB_GROUP_ID));
        assertEquals("Group 2", submenuItems.get(0).model.get(AppMenuItemProperties.TITLE));
        assertEquals(
                token1, submenuItems.get(1).model.get(AppMenuTabGroupItemProperties.TAB_GROUP_ID));
        assertEquals("Group 1", submenuItems.get(1).model.get(AppMenuItemProperties.TITLE));
    }

    @Test
    public void testBuildTabGroupsParentItem_withGroupWindowChecker() {
        Token token1 = Token.createRandom();
        Token token2 = Token.createRandom();

        SavedTabGroup group1 = new SavedTabGroup();
        group1.localId = new LocalTabGroupId(token1);
        group1.title = "Group 1";
        group1.updateTimeMs = 100L;
        group1.color = TabGroupColorId.BLUE;
        group1.savedTabs.add(new SavedTabGroupTab());

        SavedTabGroup group2 = new SavedTabGroup();
        group2.localId = new LocalTabGroupId(token2);
        group2.title = "Group 2";
        group2.updateTimeMs = 200L;
        group2.color = TabGroupColorId.RED;
        group2.savedTabs.add(new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"id1", "id2"});
        when(mTabGroupSyncService.getGroup("id1")).thenReturn(group1);
        when(mTabGroupSyncService.getGroup("id2")).thenReturn(group2);

        Tab tab1 = Mockito.mock(Tab.class);
        when(tab1.getTabGroupId()).thenReturn(token1);
        Tab tab2 = Mockito.mock(Tab.class);
        when(tab2.getTabGroupId()).thenReturn(token2);

        TabList tabList = Mockito.mock(TabList.class);
        List<Tab> tabs = List.of(tab1, tab2);
        when(tabList.iterator()).thenAnswer(invocation -> tabs.iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(tabList);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        // Items: Create new tab group, Add to group, Divider, Header, Group 2, Group 1
        assertEquals(6, tabGroupsSubmenuItems.size());
        assertEquals(
                R.id.create_new_tab_group_menu_id,
                tabGroupsSubmenuItems.get(0).model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.add_to_group_menu_id,
                tabGroupsSubmenuItems.get(1).model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.divider_line_id,
                tabGroupsSubmenuItems.get(2).model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.tab_groups_header_menu_id,
                tabGroupsSubmenuItems.get(3).model.get(AppMenuItemProperties.MENU_ITEM_ID));

        ListItem groupItem2 = tabGroupsSubmenuItems.get(4);
        assertEquals("Group 2", groupItem2.model.get(AppMenuItemProperties.TITLE));
        assertEquals(
                R.id.tab_group_menu_item_id,
                groupItem2.model.get(AppMenuItemProperties.MENU_ITEM_ID));

        ListItem groupItem1 = tabGroupsSubmenuItems.get(5);
        assertEquals("Group 1", groupItem1.model.get(AppMenuItemProperties.TITLE));
        assertEquals(
                R.id.tab_group_menu_item_id,
                groupItem1.model.get(AppMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testBuildTabGroupsParentItem_withGroupWindowChecker_crossWindow() {
        Token token1 = Token.createRandom();
        Token token2 = Token.createRandom();

        SavedTabGroup group1 = new SavedTabGroup();
        group1.localId = new LocalTabGroupId(token1);
        group1.title = "Group 1";
        group1.updateTimeMs = 100L;
        group1.color = TabGroupColorId.BLUE;
        group1.savedTabs.add(new SavedTabGroupTab());

        SavedTabGroup group2 = new SavedTabGroup();
        group2.localId = new LocalTabGroupId(token2);
        group2.title = "Group 2";
        group2.updateTimeMs = 200L;
        group2.color = TabGroupColorId.RED;
        group2.savedTabs.add(new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"id1", "id2"});
        when(mTabGroupSyncService.getGroup("id1")).thenReturn(group1);
        when(mTabGroupSyncService.getGroup("id2")).thenReturn(group2);

        Tab tab1 = Mockito.mock(Tab.class);
        when(tab1.getTabGroupId()).thenReturn(token1);
        when(tab1.getId()).thenReturn(101);
        when(tab1.getTitle()).thenReturn("Tab 1");
        when(tab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        Tab tab2 = Mockito.mock(Tab.class);
        when(tab2.getTabGroupId()).thenReturn(token2);
        when(tab2.getId()).thenReturn(102);
        when(tab2.getTitle()).thenReturn("Tab 2");
        when(tab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);

        // Current model only has tab1 in group1.
        TabList tabList = Mockito.mock(TabList.class);
        List<Tab> tabs = List.of(tab1);
        when(tabList.iterator()).thenAnswer(invocation -> tabs.iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(tabList);
        when(mTabModel.getTabsInGroup(token1)).thenReturn(List.of(tab1));
        when(mTabModel.getTabsInGroup(token2)).thenReturn(List.of());

        // TabWindowManager has group2 in window 2.
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(token2), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getGroupedTabsByWindow(2, token2, false)).thenReturn(List.of(tab2));
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mSelectorWindow2);
        when(mSelectorWindow2.getModel(false)).thenReturn(mModelWindow2);
        when(mModelWindow2.getTabModelType()).thenReturn(TabModelType.STANDARD);
        when(mModelWindow2.containsTabGroup(token2)).thenReturn(true);
        when(mModelWindow2.getTabsInGroup(token2)).thenReturn(List.of(tab2));

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        assertEquals(6, tabGroupsSubmenuItems.size());
        ListItem groupItem2 = tabGroupsSubmenuItems.get(4);
        assertEquals("Group 2", groupItem2.model.get(AppMenuItemProperties.TITLE));

        List<ListItem> tabsSubmenuItems =
                groupItem2.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        assertEquals(1, tabsSubmenuItems.size());
        assertEquals(102, tabsSubmenuItems.get(0).model.get(AppMenuTabItemProperties.TAB_ID));
        assertEquals(
                JUnitTestGURLs.URL_2,
                tabsSubmenuItems.get(0).model.get(AppMenuTabItemProperties.TAB_URL));
    }

    @Test
    public void testBuildTabGroupsParentItem_Incognito_FallbackToTabModel() {
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        Token token1 = new Token(1L, 1L);
        when(mIncognitoTabModel.getTabGroupCount()).thenReturn(1);
        when(mIncognitoTabModel.getAllTabGroupIds()).thenReturn(Set.of(token1));
        when(mIncognitoTabModel.containsTabGroup(token1)).thenReturn(true);
        when(mIncognitoTabModel.getTabGroupTitle(token1)).thenReturn("Incognito Group");
        when(mIncognitoTabModel.getTabGroupColorWithFallback(token1))
                .thenReturn(TabGroupColorId.GREY);

        Tab tab1 = Mockito.mock(Tab.class);
        when(tab1.getId()).thenReturn(101);
        when(tab1.getTitle()).thenReturn("Incognito Tab");
        when(tab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mIncognitoTabModel.getTabsInGroup(token1)).thenReturn(List.of(tab1));

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(true);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        assertEquals(5, tabGroupsSubmenuItems.size());
        ListItem groupItem = tabGroupsSubmenuItems.get(4);
        assertEquals("Incognito Group", groupItem.model.get(AppMenuItemProperties.TITLE));
    }

    @Test
    public void testBuildTabGroupsParentItem_GroupWindowChecker_EmptyGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        // Only create new tab group item is shown.
        assertEquals(1, tabGroupsSubmenuItems.size());
        assertEquals(
                R.id.create_new_tab_group_menu_id,
                tabGroupsSubmenuItems.get(0).model.get(AppMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testBuildTabGroupsParentItem_withGroupWindowChecker_remoteGroup() {
        SavedTabGroup remoteGroup = new SavedTabGroup();
        remoteGroup.syncId = "remote_id";
        remoteGroup.localId = null;
        remoteGroup.title = "Remote Group";
        remoteGroup.color = TabGroupColorId.BLUE;
        remoteGroup.updateTimeMs = 100L;

        SavedTabGroupTab remoteTab = new SavedTabGroupTab();
        remoteTab.syncId = "remote_tab_id";
        remoteTab.title = "Remote Tab";
        remoteTab.url = JUnitTestGURLs.URL_1;
        remoteGroup.savedTabs = List.of(remoteTab);

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"remote_id"});
        when(mTabGroupSyncService.getGroup("remote_id")).thenReturn(remoteGroup);

        List<Tab> emptyTabs = List.of();
        when(mTabList.iterator()).thenAnswer(invocation -> emptyTabs.iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mTabList);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        ListItem groupItem = findItemById(tabGroupsSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        assertEquals("Remote Group", groupItem.model.get(AppMenuItemProperties.TITLE));

        List<ListItem> tabsSubmenuItems =
                groupItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        assertEquals(1, tabsSubmenuItems.size());
        assertEquals("Remote Tab", tabsSubmenuItems.get(0).model.get(AppMenuItemProperties.TITLE));
        assertEquals(
                Tab.INVALID_TAB_ID,
                tabsSubmenuItems.get(0).model.get(AppMenuTabItemProperties.TAB_ID));
        assertEquals(
                JUnitTestGURLs.URL_1,
                tabsSubmenuItems.get(0).model.get(AppMenuTabItemProperties.TAB_URL));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testBuildAddToGroupItem_remoteGroup_syncGroupIdSet() {
        SavedTabGroup remoteGroup = new SavedTabGroup();
        remoteGroup.syncId = "remote_id";
        remoteGroup.localId = null;
        remoteGroup.title = "Remote Group";
        remoteGroup.color = TabGroupColorId.BLUE;
        remoteGroup.updateTimeMs = 100L;

        SavedTabGroupTab remoteTab = new SavedTabGroupTab();
        remoteTab.syncId = "remote_tab_id";
        remoteTab.title = "Remote Tab";
        remoteTab.url = JUnitTestGURLs.URL_1;
        remoteGroup.savedTabs = List.of(remoteTab);

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"remote_id"});
        when(mTabGroupSyncService.getGroup("remote_id")).thenReturn(remoteGroup);

        List<Tab> emptyTabs = List.of();
        when(mTabList.iterator()).thenAnswer(invocation -> emptyTabs.iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mTabList);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem addToGroupItem = builder.buildAddToGroupItem(mTab, /* showIcon= */ true);
        assertNotNull(addToGroupItem);

        List<ListItem> submenuItems =
                addToGroupItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        assertEquals(1, submenuItems.size());

        ListItem groupItem = submenuItems.get(0);
        assertEquals(
                R.id.add_to_existing_group_menu_item_id,
                groupItem.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals("Remote Group", groupItem.model.get(AppMenuItemProperties.TITLE));
        assertEquals("remote_id", groupItem.model.get(AppMenuTabGroupItemProperties.SYNC_GROUP_ID));
        assertNull(groupItem.model.get(AppMenuTabGroupItemProperties.TAB_GROUP_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testBuildTabGroupsParentItem_remoteGroup_disabled() {
        SavedTabGroup remoteGroup = new SavedTabGroup();
        remoteGroup.syncId = "remote_id";
        remoteGroup.localId = null;
        remoteGroup.title = "Remote Group";
        remoteGroup.color = TabGroupColorId.BLUE;
        remoteGroup.updateTimeMs = 100L;

        SavedTabGroupTab remoteTab = new SavedTabGroupTab();
        remoteTab.syncId = "remote_tab_id";
        remoteTab.title = "Remote Tab";
        remoteTab.url = JUnitTestGURLs.URL_1;
        remoteGroup.savedTabs = List.of(remoteTab);

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"remote_id"});
        when(mTabGroupSyncService.getGroup("remote_id")).thenReturn(remoteGroup);

        List<Tab> emptyTabs = List.of();
        when(mTabList.iterator()).thenAnswer(invocation -> emptyTabs.iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mTabList);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        ListItem groupItem = findItemById(tabGroupsSubmenuItems, R.id.tab_group_menu_item_id);
        assertNull(groupItem);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testBuildAddToGroupItem_remoteGroup_disabled() {
        Token token1 = Token.createRandom();

        SavedTabGroup localGroup = new SavedTabGroup();
        localGroup.syncId = "local_id";
        localGroup.localId = new LocalTabGroupId(token1);
        localGroup.title = "Local Group";
        localGroup.color = TabGroupColorId.BLUE;
        localGroup.updateTimeMs = 100L;
        localGroup.savedTabs = List.of(new SavedTabGroupTab());

        SavedTabGroup remoteGroup = new SavedTabGroup();
        remoteGroup.syncId = "remote_id";
        remoteGroup.localId = null;
        remoteGroup.title = "Remote Group";
        remoteGroup.color = TabGroupColorId.RED;
        remoteGroup.updateTimeMs = 200L;
        remoteGroup.savedTabs = List.of(new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {"local_id", "remote_id"});
        when(mTabGroupSyncService.getGroup("local_id")).thenReturn(localGroup);
        when(mTabGroupSyncService.getGroup("remote_id")).thenReturn(remoteGroup);

        Tab tab1 = Mockito.mock(Tab.class);
        when(tab1.getTabGroupId()).thenReturn(token1);

        TabList tabList = Mockito.mock(TabList.class);
        List<Tab> tabs = List.of(tab1);
        when(tabList.iterator()).thenAnswer(invocation -> tabs.iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(tabList);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem addToGroupItem = builder.buildAddToGroupItem(null, /* showIcon= */ true);
        assertNotNull(addToGroupItem);

        List<ListItem> submenuItems =
                addToGroupItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        assertEquals(1, submenuItems.size());

        ListItem groupItem = submenuItems.get(0);
        assertEquals(
                R.id.add_to_existing_group_menu_item_id,
                groupItem.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals("Local Group", groupItem.model.get(AppMenuItemProperties.TITLE));
        assertEquals(token1, groupItem.model.get(AppMenuTabGroupItemProperties.TAB_GROUP_ID));
        assertNull(groupItem.model.get(AppMenuTabGroupItemProperties.SYNC_GROUP_ID));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testBuildTabGroupsParentItem_withGroupWindowChecker_closingGroup() {
        Token token1 = Token.createRandom();

        SavedTabGroup closingGroup = new SavedTabGroup();
        closingGroup.syncId = "closing_id";
        closingGroup.localId = new LocalTabGroupId(token1);
        closingGroup.title = "Saved Title";
        closingGroup.color = TabGroupColorId.BLUE;
        closingGroup.updateTimeMs = 100L;
        closingGroup.savedTabs = List.of(new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"closing_id"});
        when(mTabGroupSyncService.getGroup("closing_id")).thenReturn(closingGroup);
        when(mTabModel.getTabGroupTitle(token1)).thenReturn("Live Closing Group");
        when(mTabModel.getTabGroupColorWithFallback(token1)).thenReturn(TabGroupColorId.RED);

        when(mClosingTab.getTabGroupId()).thenReturn(token1);
        when(mClosingTab.getId()).thenReturn(101);
        when(mClosingTab.getTitle()).thenReturn("Closing Tab");
        when(mClosingTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mClosingTab.isClosing()).thenReturn(true);

        // Non-closing model returns empty list for closing group.
        when(mTabModel.getTabsInGroup(token1)).thenReturn(List.of());

        List<Tab> tabs = List.of(mClosingTab);
        when(mComprehensiveTabList.getCount()).thenReturn(1);
        when(mComprehensiveTabList.getTabAt(0)).thenReturn(mClosingTab);
        when(mComprehensiveTabList.iterator()).thenAnswer(invocation -> tabs.iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveTabList);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        ListItem groupItem = findItemById(tabGroupsSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        assertEquals("Live Closing Group", groupItem.model.get(AppMenuItemProperties.TITLE));

        List<ListItem> tabsSubmenuItems =
                groupItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        assertEquals(1, tabsSubmenuItems.size());
        assertEquals("Closing Tab", tabsSubmenuItems.get(0).model.get(AppMenuItemProperties.TITLE));
        assertEquals(101, tabsSubmenuItems.get(0).model.get(AppMenuTabItemProperties.TAB_ID));
        assertEquals(
                JUnitTestGURLs.URL_1,
                tabsSubmenuItems.get(0).model.get(AppMenuTabItemProperties.TAB_URL));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testBuildTabGroupsParentItem_closingGroupInAnotherWindow() {
        Token token1 = Token.createRandom();

        SavedTabGroup closingGroup = new SavedTabGroup();
        closingGroup.syncId = "closing_cross_window_id";
        closingGroup.localId = new LocalTabGroupId(token1);
        closingGroup.title = "Saved Title";
        closingGroup.color = TabGroupColorId.BLUE;
        closingGroup.updateTimeMs = 100L;
        closingGroup.savedTabs = List.of(new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {"closing_cross_window_id"});
        when(mTabGroupSyncService.getGroup("closing_cross_window_id")).thenReturn(closingGroup);

        when(mTabModel.getTabsInGroup(token1)).thenReturn(List.of());
        when(mComprehensiveTabList.iterator()).thenAnswer(invocation -> List.<Tab>of().iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveTabList);
        when(mTabModel.isIncognito()).thenReturn(false);

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(token1), anyBoolean())).thenReturn(2);

        when(mModelWindow2.getTabModelType()).thenReturn(TabModelType.STANDARD);
        when(mModelWindow2.isIncognito()).thenReturn(false);
        when(mSelectorWindow2.getModel(false)).thenReturn(mModelWindow2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mSelectorWindow2);

        when(mClosingTab.getTabGroupId()).thenReturn(token1);
        when(mClosingTab.getId()).thenReturn(202);
        when(mClosingTab.getTitle()).thenReturn("Cross Window Closing Tab");
        when(mClosingTab.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mClosingTab.isClosing()).thenReturn(true);

        when(mComprehensiveModelWindow2.iterator())
                .thenAnswer(invocation -> List.of(mClosingTab).iterator());
        when(mModelWindow2.getComprehensiveModel()).thenReturn(mComprehensiveModelWindow2);
        when(mModelWindow2.getTabsInGroup(token1)).thenReturn(List.of());
        when(mModelWindow2.getTabGroupTitle(token1)).thenReturn("Live Window 2 Closing Group");
        when(mModelWindow2.getTabGroupColorWithFallback(token1)).thenReturn(TabGroupColorId.BLUE);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        ListItem groupItem = findItemById(tabGroupsSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        assertEquals(
                "Live Window 2 Closing Group", groupItem.model.get(AppMenuItemProperties.TITLE));

        List<ListItem> tabsSubmenuItems =
                groupItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        assertEquals(1, tabsSubmenuItems.size());
        assertEquals(
                "Cross Window Closing Tab",
                tabsSubmenuItems.get(0).model.get(AppMenuItemProperties.TITLE));
        assertEquals(202, tabsSubmenuItems.get(0).model.get(AppMenuTabItemProperties.TAB_ID));
        assertEquals(
                JUnitTestGURLs.URL_2,
                tabsSubmenuItems.get(0).model.get(AppMenuTabItemProperties.TAB_URL));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    @DisableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations")
    public void testBuildAddToGroupItem_remoteOperationsDisabled_excludesHiddenAndDetachedGroups() {
        Token token1 = Token.createRandom();
        SavedTabGroup hiddenGroup = new SavedTabGroup();
        hiddenGroup.syncId = "hidden_id";
        hiddenGroup.localId = new LocalTabGroupId(token1);
        hiddenGroup.title = "Hidden Group";
        hiddenGroup.color = TabGroupColorId.BLUE;
        hiddenGroup.savedTabs = List.of(new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"hidden_id"});
        when(mTabGroupSyncService.getGroup("hidden_id")).thenReturn(hiddenGroup);
        when(mTabModel.getTabsInGroup(token1)).thenReturn(List.of());
        when(mComprehensiveTabList.iterator())
                .thenAnswer(invocation -> Collections.emptyIterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveTabList);

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(token1), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mSelectorWindow2);
        when(mSelectorWindow2.getModel(false)).thenReturn(mModelWindow2);
        when(mModelWindow2.containsTabGroup(token1)).thenReturn(false);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        assertFalse(builder.shouldShowAddToGroup(mTab));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    @DisableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations")
    public void
            testBuildTabGroupsParentItem_remoteOperationsDisabled_excludesHiddenAndDetachedGroups() {
        Token token1 = Token.createRandom();
        SavedTabGroup hiddenGroup = new SavedTabGroup();
        hiddenGroup.syncId = "hidden_id";
        hiddenGroup.localId = new LocalTabGroupId(token1);
        hiddenGroup.title = "Hidden Group";
        hiddenGroup.color = TabGroupColorId.BLUE;
        hiddenGroup.savedTabs = List.of(new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"hidden_id"});
        when(mTabGroupSyncService.getGroup("hidden_id")).thenReturn(hiddenGroup);
        when(mTabModel.getTabsInGroup(token1)).thenReturn(List.of());
        when(mComprehensiveTabList.iterator())
                .thenAnswer(invocation -> Collections.emptyIterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveTabList);

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(token1), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mSelectorWindow2);
        when(mSelectorWindow2.getModel(false)).thenReturn(mModelWindow2);
        when(mModelWindow2.containsTabGroup(token1)).thenReturn(false);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem groupItem = findItemById(tabGroupsSubmenuItems, R.id.tab_group_menu_item_id);
        assertNull(groupItem);
        ListItem headerItem = findItemById(tabGroupsSubmenuItems, R.id.tab_groups_header_menu_id);
        assertNull(headerItem);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    @DisableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations")
    public void testBuildAddToGroupItem_remoteOperationsDisabled_includesHeadlessWindowGroup() {
        Token token1 = Token.createRandom();
        SavedTabGroup headlessGroup = new SavedTabGroup();
        headlessGroup.syncId = "headless_id";
        headlessGroup.localId = new LocalTabGroupId(token1);
        headlessGroup.title = "Headless Group";
        headlessGroup.color = TabGroupColorId.BLUE;
        headlessGroup.savedTabs = List.of(new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"headless_id"});
        when(mTabGroupSyncService.getGroup("headless_id")).thenReturn(headlessGroup);
        when(mTabModel.getTabsInGroup(token1)).thenReturn(List.of());
        when(mComprehensiveTabList.iterator())
                .thenAnswer(invocation -> Collections.emptyIterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveTabList);

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(token1), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mSelectorWindow2);
        when(mSelectorWindow2.getModel(false)).thenReturn(mModelWindow2);
        when(mModelWindow2.getTabModelType()).thenReturn(TabModelType.HEADLESS);
        when(mModelWindow2.containsTabGroup(token1)).thenReturn(true);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        assertTrue(builder.shouldShowAddToGroup(mTab));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    @DisableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations")
    public void
            testBuildTabGroupsParentItem_remoteOperationsDisabled_includesHeadlessWindowGroup() {
        Token token1 = Token.createRandom();
        SavedTabGroup headlessGroup = new SavedTabGroup();
        headlessGroup.syncId = "headless_id";
        headlessGroup.localId = new LocalTabGroupId(token1);
        headlessGroup.title = "Headless Group";
        headlessGroup.color = TabGroupColorId.BLUE;
        headlessGroup.savedTabs = List.of(new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"headless_id"});
        when(mTabGroupSyncService.getGroup("headless_id")).thenReturn(headlessGroup);
        when(mTabModel.getTabsInGroup(token1)).thenReturn(List.of());
        when(mComprehensiveTabList.iterator())
                .thenAnswer(invocation -> Collections.emptyIterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveTabList);

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(token1), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mSelectorWindow2);
        when(mSelectorWindow2.getModel(false)).thenReturn(mModelWindow2);
        when(mModelWindow2.getTabModelType()).thenReturn(TabModelType.HEADLESS);
        when(mModelWindow2.containsTabGroup(token1)).thenReturn(true);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper,
                        () -> mTabGroupSyncService);

        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem groupItem = findItemById(tabGroupsSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        assertEquals("Headless Group", groupItem.model.get(AppMenuItemProperties.TITLE));
        ListItem headerItem = findItemById(tabGroupsSubmenuItems, R.id.tab_groups_header_menu_id);
        assertNotNull(headerItem);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_ACTIONS_SUBMENUS)
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testBuildSubmenuForSpecificGroup_actionsDisabled_allGroupStates() {
        Token tokenCurrent = new Token(1L, 1L);
        Token tokenAnother = new Token(2L, 2L);
        Token tokenClosing = new Token(3L, 3L);
        setupGroupsAcrossWindowStates(tokenCurrent, tokenAnother, tokenClosing);

        TabGroupItemBuilder builder = createBuilder();
        ListItem parentItem = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(parentItem);
        List<ListItem> parentSubmenuItems = getGroupSubmenuItems(parentItem);

        assertSingleTabSubmenuItem(findItemByTitle(parentSubmenuItems, "Current Group"));
        assertSingleTabSubmenuItem(findItemByTitle(parentSubmenuItems, "Another Group"));
        assertSingleTabSubmenuItem(findItemByTitle(parentSubmenuItems, "Hidden Group"));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testBuildTabGroupsParentItem_crossWindowDisabled_onlyCurrentGroupIncluded() {
        Token tokenCurrent = new Token(1L, 1L);
        Token tokenAnother = new Token(2L, 2L);
        Token tokenClosing = new Token(3L, 3L);
        setupGroupsAcrossWindowStates(tokenCurrent, tokenAnother, tokenClosing);

        TabGroupItemBuilder builder = createBuilder();
        ListItem parentItem = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(parentItem);

        List<ListItem> parentSubmenuItems = getGroupSubmenuItems(parentItem);

        // Submenu: Create new tab group, Add to group, Divider, Header, Group (only 1 group!)
        assertEquals(5, parentSubmenuItems.size());
        ListItem groupItem = findItemByTitle(parentSubmenuItems, "Current Group");
        assertNotNull(groupItem);
        assertEquals(
                R.id.tab_group_menu_item_id,
                groupItem.model.get(AppMenuItemProperties.MENU_ITEM_ID));

        List<ListItem> groupSubmenuItems = getGroupSubmenuItems(groupItem);
        assertEquals(1, groupSubmenuItems.size());
        assertEquals(
                "Current Tab", groupSubmenuItems.get(0).model.get(AppMenuItemProperties.TITLE));
        assertEquals(101, groupSubmenuItems.get(0).model.get(AppMenuTabItemProperties.TAB_ID));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testBuildAddToGroupItem_crossWindowDisabled_onlyCurrentGroupIncluded() {
        Token tokenCurrent = new Token(1L, 1L);
        Token tokenAnother = new Token(2L, 2L);
        Token tokenClosing = new Token(3L, 3L);
        setupGroupsAcrossWindowStates(tokenCurrent, tokenAnother, tokenClosing);

        TabGroupItemBuilder builder = createBuilder();
        assertTrue(builder.shouldShowAddToGroup(mTab));
        ListItem addToGroupItem = builder.buildAddToGroupItem(mTab, /* showIcon= */ true);
        assertNotNull(addToGroupItem);
        List<ListItem> addToGroupSubmenuItems = getGroupSubmenuItems(addToGroupItem);
        assertEquals(1, addToGroupSubmenuItems.size());
        assertEquals(
                R.id.add_to_existing_group_menu_item_id,
                addToGroupSubmenuItems.get(0).model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Current Group",
                addToGroupSubmenuItems.get(0).model.get(AppMenuItemProperties.TITLE));
        assertEquals(
                tokenCurrent,
                addToGroupSubmenuItems
                        .get(0)
                        .model
                        .get(AppMenuTabGroupItemProperties.TAB_GROUP_ID));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/false"
    })
    public void
            testBuildSubmenuForSpecificGroup_headlessGroup_remoteOperationsDisabled_returnsEmpty() {
        Token token1 = Token.createRandom();
        SavedTabGroup headlessGroup =
                createLocalSavedTabGroup(
                        "headless_id",
                        token1,
                        "Headless Group",
                        TabGroupColorId.BLUE,
                        100L,
                        List.of(new SavedTabGroupTab()));

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"headless_id"});
        when(mTabGroupSyncService.getGroup("headless_id")).thenReturn(headlessGroup);
        when(mTabModel.getTabsInGroup(token1)).thenReturn(List.of());
        when(mComprehensiveTabList.iterator())
                .thenAnswer(invocation -> Collections.emptyIterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveTabList);

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(token1), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mSelectorWindow2);
        when(mSelectorWindow2.getModel(false)).thenReturn(mModelWindow2);
        when(mModelWindow2.getTabModelType()).thenReturn(TabModelType.HEADLESS);
        when(mModelWindow2.containsTabGroup(token1)).thenReturn(true);
        when(mModelWindow2.getTabsInGroup(token1)).thenReturn(List.of());

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder = createBuilder();
        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems = getGroupSubmenuItems(tabGroupsParent);
        ListItem groupItem = findItemById(tabGroupsSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        assertEquals("Headless Group", groupItem.model.get(AppMenuItemProperties.TITLE));

        List<ListItem> groupSubmenuItems = getGroupSubmenuItems(groupItem);
        assertTrue(groupSubmenuItems.isEmpty());
    }

    @Test
    public void testBuildSubmenuForSpecificGroup_incognito_buildsTabsFromModel() {
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        Token token1 = new Token(1L, 1L);
        when(mIncognitoTabModel.getTabGroupCount()).thenReturn(1);
        when(mIncognitoTabModel.getAllTabGroupIds()).thenReturn(Set.of(token1));
        when(mIncognitoTabModel.containsTabGroup(token1)).thenReturn(true);
        when(mIncognitoTabModel.getTabGroupTitle(token1)).thenReturn("Incognito Group");
        when(mIncognitoTabModel.getTabGroupColorWithFallback(token1))
                .thenReturn(TabGroupColorId.GREY);

        Tab tab1 =
                createMockTab(
                        101,
                        "Incognito Tab",
                        JUnitTestGURLs.URL_1,
                        token1,
                        /* isOffTheRecord= */ true);
        when(mIncognitoTabModel.getTabsInGroup(token1)).thenReturn(List.of(tab1));

        // Even if TabGroupSyncService has sync groups, incognito must ignore it and use local
        // model.
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"synced_group"});

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(true);

        TabGroupItemBuilder builder = createBuilder();
        ListItem tabGroupsParent = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(tabGroupsParent);

        List<ListItem> parentSubmenuItems = getGroupSubmenuItems(tabGroupsParent);
        ListItem groupItem = findItemById(parentSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        assertEquals("Incognito Group", groupItem.model.get(AppMenuItemProperties.TITLE));

        List<ListItem> groupSubmenuItems = getGroupSubmenuItems(groupItem);
        assertEquals(1, groupSubmenuItems.size());
        ListItem tabItem = groupSubmenuItems.get(0);
        assertEquals(AppMenuHandler.AppMenuItemType.TAB, tabItem.type);
        assertEquals(
                R.id.tab_group_tab_menu_item,
                tabItem.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals("Incognito Tab", tabItem.model.get(AppMenuItemProperties.TITLE));
        assertEquals(101, tabItem.model.get(AppMenuTabItemProperties.TAB_ID));
        assertEquals(JUnitTestGURLs.URL_1, tabItem.model.get(AppMenuTabItemProperties.TAB_URL));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testBuildSubmenuForRemoteGroup_nullTabFields_usesFallbacks() {
        SavedTabGroupTab tabWithNulls = createSavedTab("tab_nulls", null, null, 42);
        SavedTabGroup group =
                createRemoteSavedTabGroup(
                        "remote_sync_null_fields",
                        "Remote Group Null Fields",
                        TabGroupColorId.BLUE,
                        300L,
                        List.of(tabWithNulls));

        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {"remote_sync_null_fields"});
        when(mTabGroupSyncService.getGroup("remote_sync_null_fields")).thenReturn(group);

        Answer<Boolean> emptyFaviconAnswer =
                invocation -> {
                    FaviconHelper.FaviconImageCallback callback = invocation.getArgument(5);
                    callback.onFaviconAvailable(
                            Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888), GURL.emptyGURL());
                    return true;
                };
        doAnswer(emptyFaviconAnswer)
                .when(mFaviconHelperJniMock)
                .getLocalFaviconImageForURL(
                        eq(1L), eq(mProfile), eq(GURL.emptyGURL()), anyInt(), eq(false), any());

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder = createBuilder();
        ListItem parentItem = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(parentItem);
        List<ListItem> parentSubmenuItems = getGroupSubmenuItems(parentItem);

        ListItem groupItem = findItemByTitle(parentSubmenuItems, "Remote Group Null Fields");
        assertNotNull(groupItem);
        List<ListItem> submenuItems = getGroupSubmenuItems(groupItem);
        assertEquals(1, submenuItems.size());
        ListItem tabItem = submenuItems.get(0);
        assertEquals("", tabItem.model.get(AppMenuItemProperties.TITLE));
        assertEquals(GURL.emptyGURL(), tabItem.model.get(AppMenuTabItemProperties.TAB_URL));
        assertEquals(42, tabItem.model.get(AppMenuTabItemProperties.TAB_ID));
        LazyOneshotSupplier<Drawable> iconSupplier =
                tabItem.model.get(AppMenuItemProperties.ICON_SUPPLIER);
        assertNotNull(iconSupplier);
        assertNotNull(iconSupplier.get());
        Mockito.verify(mFaviconHelperJniMock)
                .getLocalFaviconImageForURL(
                        eq(1L), eq(mProfile), eq(GURL.emptyGURL()), anyInt(), eq(false), any());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testBuildSubmenuForRemoteGroup_nullGroupInService_returnsEmpty() {
        SavedTabGroup group =
                createRemoteSavedTabGroup(
                        "remote_sync_null_group",
                        "Remote Group Null In Service",
                        TabGroupColorId.GREEN,
                        100L,
                        List.of(new SavedTabGroupTab()));

        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {"remote_sync_null_group"});
        when(mTabGroupSyncService.getGroup("remote_sync_null_group")).thenReturn(group);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        TabGroupItemBuilder builder = createBuilder();
        ListItem parentItem = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(parentItem);
        List<ListItem> parentSubmenuItems = getGroupSubmenuItems(parentItem);

        ListItem groupItem = findItemByTitle(parentSubmenuItems, "Remote Group Null In Service");
        assertNotNull(groupItem);
        when(mTabGroupSyncService.getGroup("remote_sync_null_group")).thenReturn(null);
        List<ListItem> submenuItems = getGroupSubmenuItems(groupItem);
        assertTrue(submenuItems.isEmpty());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_ACTIONS_SUBMENUS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testBuildSubmenuForSpecificGroup_withActions() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Token token = new Token(1L, 1L);
        setUpMockTabGroup(mTabModel, /* isIncognito= */ false, /* hasGroupId= */ true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        SavedTabGroup group =
                createLocalSavedTabGroup(
                        "sync_1",
                        token,
                        "Group 1",
                        TabGroupColorId.BLUE,
                        100L,
                        List.of(new SavedTabGroupTab()));

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync_1"});
        when(mTabGroupSyncService.getGroup("sync_1")).thenReturn(group);

        TabGroupItemBuilder builder = createBuilder();
        ListItem parentItem = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(parentItem);
        List<ListItem> parentSubmenuItems = getGroupSubmenuItems(parentItem);
        ListItem groupItem = findItemById(parentSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        List<ListItem> items = getGroupSubmenuItems(groupItem);

        ListItem openItem = findItemById(items, R.id.tab_group_open_group_menu_id);
        assertNotNull(openItem);
        assertFalse(openItem.model.get(AppMenuItemProperties.ENABLED));
        assertEquals(token, openItem.model.get(AppMenuTabGroupItemProperties.TAB_GROUP_ID));
        assertEquals("sync_1", openItem.model.get(AppMenuTabGroupItemProperties.SYNC_GROUP_ID));

        ListItem openInNewWindowItem =
                findItemById(items, R.id.tab_group_open_in_new_window_menu_id);
        assertNotNull(openInNewWindowItem);
        assertTrue(openInNewWindowItem.model.get(AppMenuItemProperties.ENABLED));
        assertEquals(
                token, openInNewWindowItem.model.get(AppMenuTabGroupItemProperties.TAB_GROUP_ID));
        assertEquals(
                "sync_1",
                openInNewWindowItem.model.get(AppMenuTabGroupItemProperties.SYNC_GROUP_ID));

        ListItem deleteItem = findItemById(items, R.id.tab_group_delete_group_menu_id);
        assertNotNull(deleteItem);
        assertTrue(deleteItem.model.get(AppMenuItemProperties.ENABLED));
        assertEquals(token, deleteItem.model.get(AppMenuTabGroupItemProperties.TAB_GROUP_ID));
        assertEquals("sync_1", deleteItem.model.get(AppMenuTabGroupItemProperties.SYNC_GROUP_ID));

        assertNotNull(findItemById(items, R.id.divider_line_id));
        assertNotNull(findItemById(items, R.id.tabs_in_group_header_menu_id));
        assertNotNull(findItemById(items, R.id.tab_group_tab_menu_item));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_ACTIONS_SUBMENUS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void
            testBuildSubmenuForSpecificGroup_openInNewWindowOmittedWhenMultiInstanceNotSupported() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        Token token = new Token(1L, 1L);
        setUpMockTabGroup(mTabModel, /* isIncognito= */ false, /* hasGroupId= */ true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        SavedTabGroup group =
                createLocalSavedTabGroup(
                        "sync_1",
                        token,
                        "Group 1",
                        TabGroupColorId.BLUE,
                        100L,
                        List.of(new SavedTabGroupTab()));

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync_1"});
        when(mTabGroupSyncService.getGroup("sync_1")).thenReturn(group);

        TabGroupItemBuilder builder = createBuilder();
        ListItem parentItem = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(parentItem);
        ListItem groupItem =
                findItemById(getGroupSubmenuItems(parentItem), R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        List<ListItem> items = getGroupSubmenuItems(groupItem);

        assertNull(findItemById(items, R.id.tab_group_open_in_new_window_menu_id));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_ACTIONS_SUBMENUS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testBuildSubmenuForSpecificGroup_openInNewWindowDisabledWhenGroupContainsAllTabs() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Token token = new Token(1L, 1L);
        setUpMockTabGroup(mTabModel, /* isIncognito= */ false, /* hasGroupId= */ true);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        SavedTabGroup group =
                createLocalSavedTabGroup(
                        "sync_1",
                        token,
                        "Group 1",
                        TabGroupColorId.BLUE,
                        100L,
                        List.of(new SavedTabGroupTab()));

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync_1"});
        when(mTabGroupSyncService.getGroup("sync_1")).thenReturn(group);

        TabGroupItemBuilder builder = createBuilder();
        ListItem parentItem = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(parentItem);
        List<ListItem> parentSubmenuItems = getGroupSubmenuItems(parentItem);
        ListItem groupItem = findItemById(parentSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        List<ListItem> items = getGroupSubmenuItems(groupItem);

        ListItem openInNewWindowItem =
                findItemById(items, R.id.tab_group_open_in_new_window_menu_id);
        assertNotNull(openInNewWindowItem);
        assertFalse(openInNewWindowItem.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_ACTIONS_SUBMENUS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testBuildSubmenuForSpecificGroup_collaborationGroup_deleteOmitted() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Token token = new Token(1L, 1L);
        setUpMockTabGroup(mTabModel, /* isIncognito= */ false, /* hasGroupId= */ true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        SavedTabGroup group =
                createLocalSavedTabGroup(
                        "sync_1",
                        token,
                        "Group 1",
                        TabGroupColorId.BLUE,
                        100L,
                        List.of(new SavedTabGroupTab()));
        group.collaborationId = "collab_1";

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync_1"});
        when(mTabGroupSyncService.getGroup("sync_1")).thenReturn(group);

        TabGroupItemBuilder builder = createBuilder();
        ListItem parentItem = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(parentItem);
        List<ListItem> parentSubmenuItems = getGroupSubmenuItems(parentItem);
        ListItem groupItem = findItemById(parentSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        List<ListItem> items = getGroupSubmenuItems(groupItem);

        assertNull(findItemById(items, R.id.tab_group_delete_group_menu_id));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_ACTIONS_SUBMENUS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testBuildSubmenuForSpecificGroup_openGroupEnabled_hidden() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        SavedTabGroup hiddenGroup =
                createRemoteSavedTabGroup(
                        "sync_hidden",
                        "Hidden Group",
                        TabGroupColorId.RED,
                        200L,
                        List.of(
                                createSavedTab(
                                        "sync_hidden_tab",
                                        "Hidden Tab",
                                        JUnitTestGURLs.URL_2,
                                        null)));

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync_hidden"});
        when(mTabGroupSyncService.getGroup("sync_hidden")).thenReturn(hiddenGroup);

        TabGroupItemBuilder builder = createBuilder();
        ListItem parentItem = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(parentItem);
        List<ListItem> parentSubmenuItems = getGroupSubmenuItems(parentItem);
        ListItem groupItem = findItemById(parentSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        List<ListItem> items = getGroupSubmenuItems(groupItem);

        ListItem openItem = findItemById(items, R.id.tab_group_open_group_menu_id);
        assertNotNull(openItem);
        assertTrue(openItem.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_ACTIONS_SUBMENUS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testBuildSubmenuForSpecificGroup_openGroupEnabled_inAnother() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Token tokenWindow2 = new Token(2L, 2L);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        Tab tabWindow2 =
                createMockTab(201, "Another Tab", JUnitTestGURLs.URL_2, tokenWindow2, false);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.findWindowIdForTabGroup(tokenWindow2)).thenReturn(2);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(tokenWindow2), anyBoolean()))
                .thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mSelectorWindow2);
        when(mSelectorWindow2.getModel(false)).thenReturn(mModelWindow2);
        when(mModelWindow2.isIncognito()).thenReturn(false);
        when(mModelWindow2.containsTabGroup(tokenWindow2)).thenReturn(true);
        when(mModelWindow2.getTabsInGroup(tokenWindow2)).thenReturn(List.of(tabWindow2));

        SavedTabGroup anotherGroup =
                createLocalSavedTabGroup(
                        "sync_another",
                        tokenWindow2,
                        "Another Group",
                        TabGroupColorId.RED,
                        200L,
                        List.of(
                                createSavedTab(
                                        "sync_another_tab",
                                        "Another Tab",
                                        JUnitTestGURLs.URL_2,
                                        201)));

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"sync_another"});
        when(mTabGroupSyncService.getGroup("sync_another")).thenReturn(anotherGroup);

        TabGroupItemBuilder builder = createBuilder();
        ListItem parentItem = builder.buildTabGroupsParentItem(mTab);
        assertNotNull(parentItem);
        List<ListItem> parentSubmenuItems = getGroupSubmenuItems(parentItem);
        ListItem groupItem = findItemById(parentSubmenuItems, R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        List<ListItem> items = getGroupSubmenuItems(groupItem);

        ListItem openItem = findItemById(items, R.id.tab_group_open_group_menu_id);
        assertNotNull(openItem);
        assertTrue(openItem.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_ACTIONS_SUBMENUS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS
    })
    public void testBuildSubmenuForSpecificGroup_incognito() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        setUpMockTabGroup(mIncognitoTabModel, /* isIncognito= */ true, /* hasGroupId= */ true);
        when(mTab.isIncognito()).thenReturn(true);

        ListItem tabGroupsParent = mTabGroupItemBuilder.buildTabGroupsParentItem(mTab);
        ListItem groupItem =
                findItemById(getGroupSubmenuItems(tabGroupsParent), R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        List<ListItem> items = getGroupSubmenuItems(groupItem);

        assertNotNull(findItemById(items, R.id.tab_group_open_group_menu_id));
        assertNotNull(findItemById(items, R.id.tab_group_open_in_new_window_menu_id));
        assertNull(findItemById(items, R.id.tab_group_delete_group_menu_id));
        assertNotNull(findItemById(items, R.id.divider_line_id));
        assertNotNull(findItemById(items, R.id.tabs_in_group_header_menu_id));
        assertNotNull(findItemById(items, R.id.tab_group_tab_menu_item));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_ACTIONS_SUBMENUS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS
    })
    public void testBuildSubmenuForSpecificGroup_nullSyncService() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        setUpMockTabGroup(mTabModel, /* isIncognito= */ false, /* hasGroupId= */ true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ListItem tabGroupsParent = mTabGroupItemBuilder.buildTabGroupsParentItem(mTab);
        ListItem groupItem =
                findItemById(getGroupSubmenuItems(tabGroupsParent), R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        List<ListItem> items = getGroupSubmenuItems(groupItem);

        assertNotNull(findItemById(items, R.id.tab_group_open_group_menu_id));
        assertNotNull(findItemById(items, R.id.tab_group_open_in_new_window_menu_id));
        assertNull(findItemById(items, R.id.tab_group_delete_group_menu_id));
        assertNotNull(findItemById(items, R.id.divider_line_id));
        assertNotNull(findItemById(items, R.id.tabs_in_group_header_menu_id));
        assertNotNull(findItemById(items, R.id.tab_group_tab_menu_item));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_ACTIONS_SUBMENUS)
    public void testBuildSubmenuForSpecificGroup_featureDisabled() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        setUpMockTabGroup(mTabModel, /* isIncognito= */ false, /* hasGroupId= */ true);

        ListItem tabGroupsParent = mTabGroupItemBuilder.buildTabGroupsParentItem(mTab);
        ListItem groupItem =
                findItemById(getGroupSubmenuItems(tabGroupsParent), R.id.tab_group_menu_item_id);
        assertNotNull(groupItem);
        List<ListItem> items = getGroupSubmenuItems(groupItem);

        assertNull(findItemById(items, R.id.tab_group_open_group_menu_id));
        assertNull(findItemById(items, R.id.tab_group_open_in_new_window_menu_id));
        assertNull(findItemById(items, R.id.tab_group_delete_group_menu_id));
        assertNull(findItemById(items, R.id.tabs_in_group_header_menu_id));
        assertNull(findItemById(items, R.id.divider_line_id));
        assertNotNull(findItemById(items, R.id.tab_group_tab_menu_item));
    }

    private TabGroupItemBuilder createBuilder() {
        return new TabGroupItemBuilder(
                mContext,
                mAppMenuItemTheme,
                mTabModelSelector,
                /* isMenuIconAtStart= */ false,
                /* shouldShowIconBeforeItem= */ true,
                mRoundedIconGenerator,
                mDefaultFaviconHelper,
                () -> mFaviconHelper,
                () -> mTabGroupSyncService);
    }

    private Tab createMockTab(
            int id, String title, GURL url, Token groupId, boolean isOffTheRecord) {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(tab.getTitle()).thenReturn(title);
        when(tab.getUrl()).thenReturn(url);
        when(tab.getTabGroupId()).thenReturn(groupId);
        when(tab.isOffTheRecord()).thenReturn(isOffTheRecord);
        when(tab.isInitialized()).thenReturn(true);
        when(tab.isDestroyed()).thenReturn(false);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        return tab;
    }

    private SavedTabGroup createLocalSavedTabGroup(
            String syncId,
            Token localToken,
            String title,
            @TabGroupColorId int color,
            long updateTimeMs,
            List<SavedTabGroupTab> savedTabs) {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = syncId;
        group.localId = new LocalTabGroupId(localToken);
        group.title = title;
        group.color = color;
        group.updateTimeMs = updateTimeMs;
        group.savedTabs = savedTabs;
        return group;
    }

    private SavedTabGroup createRemoteSavedTabGroup(
            String syncId,
            String title,
            @TabGroupColorId int color,
            long updateTimeMs,
            List<SavedTabGroupTab> savedTabs) {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = syncId;
        group.localId = null;
        group.title = title;
        group.color = color;
        group.updateTimeMs = updateTimeMs;
        group.savedTabs = savedTabs;
        return group;
    }

    private SavedTabGroupTab createSavedTab(
            String syncId, String title, GURL url, Integer localId) {
        SavedTabGroupTab tab = new SavedTabGroupTab();
        tab.syncId = syncId;
        tab.title = title;
        tab.url = url;
        tab.localId = localId;
        return tab;
    }

    private ListItem findItemByTitle(List<ListItem> items, String title) {
        for (ListItem item : items) {
            if (title.equals(item.model.get(AppMenuItemProperties.TITLE))) {
                return item;
            }
        }
        return null;
    }

    private List<ListItem> getGroupSubmenuItems(ListItem item) {
        return item.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
    }

    private void assertSingleTabSubmenuItem(ListItem groupItem) {
        assertNotNull(groupItem);
        List<ListItem> items = getGroupSubmenuItems(groupItem);
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(AppMenuHandler.AppMenuItemType.TAB, item.type);
        assertEquals(
                R.id.tab_group_tab_menu_item, item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertNull(findItemById(items, R.id.divider_line_id));
    }

    private void setupGroupsAcrossWindowStates(
            Token tokenCurrent, Token tokenAnother, Token tokenClosing) {
        // 1. IN_CURRENT group
        Tab tabCurrent =
                createMockTab(101, "Current Tab", JUnitTestGURLs.URL_1, tokenCurrent, false);
        when(mTabModel.getTabGroupCount()).thenReturn(1);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(tokenCurrent));
        when(mTabModel.getTabGroupTitle(tokenCurrent)).thenReturn("Current Group");
        when(mTabModel.getTabGroupColorWithFallback(tokenCurrent)).thenReturn(TabGroupColorId.BLUE);
        when(mTabModel.getTabsInGroup(tokenCurrent)).thenReturn(List.of(tabCurrent));

        SavedTabGroup currentGroup =
                createLocalSavedTabGroup(
                        "sync_current",
                        tokenCurrent,
                        "Current Group",
                        TabGroupColorId.BLUE,
                        400L,
                        List.of(
                                createSavedTab(
                                        "sync_current_tab",
                                        "Current Tab",
                                        JUnitTestGURLs.URL_1,
                                        101)));

        // 2. IN_ANOTHER group
        Tab tabWindow2 =
                createMockTab(201, "Another Tab", JUnitTestGURLs.URL_2, tokenAnother, false);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.findWindowIdForTabGroup(tokenAnother)).thenReturn(2);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(tokenAnother), anyBoolean()))
                .thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mSelectorWindow2);
        when(mSelectorWindow2.getModel(false)).thenReturn(mModelWindow2);
        when(mModelWindow2.isIncognito()).thenReturn(false);
        when(mModelWindow2.containsTabGroup(tokenAnother)).thenReturn(true);
        when(mModelWindow2.getTabsInGroup(tokenAnother)).thenReturn(List.of(tabWindow2));

        SavedTabGroup anotherGroup =
                createLocalSavedTabGroup(
                        "sync_another",
                        tokenAnother,
                        "Another Group",
                        TabGroupColorId.RED,
                        300L,
                        List.of(
                                createSavedTab(
                                        "sync_another_tab",
                                        "Another Tab",
                                        JUnitTestGURLs.URL_2,
                                        201)));

        // 3. HIDDEN group
        SavedTabGroup hiddenGroup =
                createRemoteSavedTabGroup(
                        "sync_hidden",
                        "Hidden Group",
                        TabGroupColorId.GREEN,
                        200L,
                        List.of(
                                createSavedTab(
                                        "sync_hidden_tab",
                                        "Hidden Tab",
                                        JUnitTestGURLs.URL_3,
                                        null)));

        // 4. CLOSING group
        when(mClosingTab.getId()).thenReturn(102);
        when(mClosingTab.getTitle()).thenReturn("Closing Tab");
        when(mClosingTab.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mClosingTab.getTabGroupId()).thenReturn(tokenClosing);
        when(mClosingTab.isClosing()).thenReturn(true);
        when(mTabModel.getTabsInGroup(tokenClosing)).thenReturn(List.of());

        List<Tab> comprehensiveTabs = List.of(tabCurrent, mClosingTab);
        when(mComprehensiveTabList.getCount()).thenReturn(2);
        when(mComprehensiveTabList.iterator())
                .thenAnswer(invocation -> comprehensiveTabs.iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveTabList);

        SavedTabGroup closingGroup =
                createLocalSavedTabGroup(
                        "sync_closing",
                        tokenClosing,
                        "Closing Group",
                        TabGroupColorId.YELLOW,
                        100L,
                        List.of(
                                createSavedTab(
                                        "sync_closing_tab",
                                        "Closing Tab",
                                        JUnitTestGURLs.URL_2,
                                        102)));

        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(
                        new String[] {
                            "sync_current", "sync_another", "sync_hidden", "sync_closing"
                        });
        when(mTabGroupSyncService.getGroup("sync_closing")).thenReturn(closingGroup);
        when(mTabGroupSyncService.getGroup("sync_current")).thenReturn(currentGroup);
        when(mTabGroupSyncService.getGroup("sync_another")).thenReturn(anotherGroup);
        when(mTabGroupSyncService.getGroup("sync_hidden")).thenReturn(hiddenGroup);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
    }
}
