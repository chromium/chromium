// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemTheme;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemWithSubmenuProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTabItemProperties;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
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

    private TabGroupItemBuilder mTabGroupItemBuilder;
    private TabModel mTabModel;
    private TabModel mIncognitoTabModel;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        mFaviconHelper = new FaviconHelper();

        mTabModel = Mockito.mock(TabModel.class);
        mIncognitoTabModel = Mockito.mock(TabModel.class);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mIncognitoTabModel.getProfile()).thenReturn(mProfile);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        mTabGroupItemBuilder =
                new TabGroupItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        () -> mFaviconHelper);
    }

    private Tab setUpMockTabGroup(TabModel tabModel, boolean isIncognito, boolean hasGroupId) {
        Token token1 = new Token(1L, 1L);
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

    private ListItem findItemById(List<ListItem> items, int id) {
        for (ListItem item : items) {
            if (item.model.get(AppMenuItemProperties.MENU_ITEM_ID) == id) {
                return item;
            }
        }
        return null;
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

        ListItem tabItem2 = tabsSubmenuItems.get(1);
        assertEquals(AppMenuHandler.AppMenuItemType.TAB, tabItem2.type);
        assertEquals(102, tabItem2.model.get(AppMenuTabItemProperties.TAB_ID));
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
}
