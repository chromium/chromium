// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabbed_mode.AppMenuUnitTestUtils.assertMenuItemsAreEqual;
import static org.chromium.chrome.browser.tabbed_mode.AppMenuUnitTestUtils.assertMenuTitlesAreEqual;
import static org.chromium.chrome.browser.tabbed_mode.AppMenuUnitTestUtils.item;

import android.content.Context;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemTheme;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.RecentlyClosedBulkEvent;
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedGroup;
import org.chromium.chrome.browser.ntp.RecentlyClosedTab;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.tabbed_mode.AppMenuUnitTestUtils.MenuItem;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemWithSubmenuProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuRecentEntryItemProperties;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link HistoryItemBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
@DisableFeatures({TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS})
public class HistoryItemBuilderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    @Mock private AppMenuItemTheme mAppMenuItemTheme;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Profile mProfile;
    @Mock private RecentlyClosedEntriesManager mRecentlyClosedEntriesManager;
    @Mock private ForeignSessionHelper mForeignSessionHelperMock;
    @Mock private RoundedIconGenerator mRoundedIconGenerator;
    @Mock private FaviconHelper.DefaultFaviconHelper mDefaultFaviconHelper;
    private FaviconHelper mFaviconHelper;

    private HistoryItemBuilder mHistoryItemBuilder;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        TabModel tabModel = Mockito.mock(TabModel.class);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);
        when(tabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        mHistoryItemBuilder =
                new HistoryItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        mTabModelSelector,
                        () -> mFaviconHelper,
                        () -> mRecentlyClosedEntriesManager,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper);

        mHistoryItemBuilder.setForeignSessionHelperForTesting(mForeignSessionHelperMock);
    }

    @Test
    public void testHistorySubmenu_WithRecentEntries() {
        List<RecentlyClosedEntry> entries = new ArrayList<>();
        RecentlyClosedTab tab1 =
                new RecentlyClosedTab(
                        /* sessionId= */ 1,
                        /* timestamp= */ 0,
                        "Title 1",
                        JUnitTestGURLs.URL_1,
                        /* tabGroupId= */ null);
        RecentlyClosedTab tab2 =
                new RecentlyClosedTab(
                        /* sessionId= */ 2,
                        /* timestamp= */ 0,
                        "Title 2",
                        JUnitTestGURLs.URL_2,
                        /* tabGroupId= */ null);
        entries.add(tab1);
        entries.add(tab2);
        when(mRecentlyClosedEntriesManager.getRecentlyClosedEntries()).thenReturn(entries);

        List<MenuItem> expectedSubmenu =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.open_history_menu_id),
                                item(R.id.recent_tabs_menu_id),
                                item(R.id.divider_line_id),
                                item(R.id.recent_tabs_header_menu_id),
                                item(R.id.recent_entry_tab_menu_item),
                                item(R.id.recent_entry_tab_menu_item)));

        List<ListItem> items =
                mHistoryItemBuilder
                        .buildHistoryParentItem()
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        assertMenuItemsAreEqual(items, expectedSubmenu);
    }

    @Test
    public void testHistorySubmenu_WithRecentlyClosedWindow() {
        List<RecentlyClosedEntry> entries = new ArrayList<>();
        RecentlyClosedWindow closedWindow =
                new RecentlyClosedWindow(
                        /* timestamp= */ 0,
                        /* instanceId= */ 1,
                        JUnitTestGURLs.URL_1.getSpec(),
                        /* title= */ "Custom Window",
                        "Active Tab Title",
                        /* tabCount= */ 3);
        entries.add(closedWindow);
        when(mRecentlyClosedEntriesManager.getRecentlyClosedEntries()).thenReturn(entries);

        RecentlyClosedTab tab1 =
                new RecentlyClosedTab(
                        /* sessionId= */ 10,
                        /* timestamp= */ 0,
                        "Tab 1 Title",
                        JUnitTestGURLs.URL_1,
                        /* tabGroupId= */ null);
        RecentlyClosedTab tab2 =
                new RecentlyClosedTab(
                        /* sessionId= */ 20,
                        /* timestamp= */ 0,
                        "Tab 2 Title",
                        JUnitTestGURLs.URL_2,
                        /* tabGroupId= */ null);
        when(mRecentlyClosedEntriesManager.getTabsForClosedWindow(closedWindow))
                .thenReturn(List.of(tab1, tab2));

        List<MenuItem> expectedSubmenu =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.open_history_menu_id),
                                item(R.id.recent_tabs_menu_id),
                                item(R.id.divider_line_id),
                                item(R.id.recent_tabs_header_menu_id),
                                item(
                                        R.id.recent_entry_menu_item,
                                        item(R.id.recent_entry_window_menu_item),
                                        item(R.id.divider_line_id),
                                        item(R.id.recent_entry_window_tab_menu_item),
                                        item(R.id.recent_entry_window_tab_menu_item))));

        List<ListItem> items =
                mHistoryItemBuilder
                        .buildHistoryParentItem()
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        assertMenuItemsAreEqual(items, expectedSubmenu);

        Context context = ContextUtils.getApplicationContext();
        String tabsText =
                context.getResources()
                        .getQuantityString(R.plurals.recent_tabs_group_closure_without_title, 3, 3);
        String restoreText = context.getString(R.string.menu_recent_entry_restore_window);

        List<MenuItem> expectedTitles =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.string.menu_history),
                                item(R.string.menu_recent_tabs),
                                item(0),
                                item(R.string.recent_tabs),
                                item(
                                        context.getString(
                                                R.string.menu_window_title_with_tab_count,
                                                "Custom Window",
                                                tabsText),
                                        item(restoreText),
                                        item(0),
                                        item("Tab 1 Title"),
                                        item("Tab 2 Title"))));

        assertMenuTitlesAreEqual(items, expectedTitles);

        // Index 4 is the first recently closed entry in the submenu (after the default history
        // actions: History, Recent Tabs, the Divider, and the Recent Tabs header).
        ListItem windowItem = items.get(4);
        List<ListItem> windowSubmenu =
                windowItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem restoreItem = windowSubmenu.get(0);
        assertEquals(
                closedWindow, restoreItem.model.get(AppMenuRecentEntryItemProperties.RECENT_ENTRY));
        assertEquals(AppMenuHandler.AppMenuItemType.RECENT_ENTRY_NO_ICON, restoreItem.type);
    }

    @Test
    public void testHistorySubmenu_WithRecentlyClosedBulkEvent() {
        List<RecentlyClosedEntry> entries = new ArrayList<>();
        RecentlyClosedBulkEvent bulkEvent = new RecentlyClosedBulkEvent(100, 0);
        RecentlyClosedTab tab1 =
                new RecentlyClosedTab(
                        /* sessionId= */ 1,
                        /* timestamp= */ 0,
                        "Title 1",
                        JUnitTestGURLs.URL_1,
                        /* tabGroupId= */ null);
        RecentlyClosedTab tab2 =
                new RecentlyClosedTab(
                        /* sessionId= */ 2,
                        /* timestamp= */ 0,
                        "Title 2",
                        JUnitTestGURLs.URL_2,
                        /* tabGroupId= */ null);
        bulkEvent.getTabs().add(tab1);
        bulkEvent.getTabs().add(tab2);
        entries.add(bulkEvent);
        when(mRecentlyClosedEntriesManager.getRecentlyClosedEntries()).thenReturn(entries);

        List<MenuItem> expectedSubmenu =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.open_history_menu_id),
                                item(R.id.recent_tabs_menu_id),
                                item(R.id.divider_line_id),
                                item(R.id.recent_tabs_header_menu_id),
                                item(R.id.recent_entry_tab_menu_item),
                                item(R.id.recent_entry_tab_menu_item)));

        List<ListItem> items =
                mHistoryItemBuilder
                        .buildHistoryParentItem()
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        assertMenuItemsAreEqual(items, expectedSubmenu);

        List<MenuItem> expectedTitles =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.string.menu_history),
                                item(R.string.menu_recent_tabs),
                                item(0),
                                item(R.string.recent_tabs),
                                item("Title 1"),
                                item("Title 2")));

        assertMenuTitlesAreEqual(items, expectedTitles);

        // Verify that the RECENT_ENTRY property points to the individual tab, not the bulk event.
        ListItem item1 = items.get(4);
        assertEquals(tab1, item1.model.get(AppMenuRecentEntryItemProperties.RECENT_ENTRY));
        ListItem item2 = items.get(5);
        assertEquals(tab2, item2.model.get(AppMenuRecentEntryItemProperties.RECENT_ENTRY));
    }

    @Test
    public void testHistorySubmenu_WithUnnamedRecentlyClosedWindow() {
        List<RecentlyClosedEntry> entries = new ArrayList<>();
        RecentlyClosedWindow closedWindow =
                new RecentlyClosedWindow(
                        /* timestamp= */ 0,
                        /* instanceId= */ 1,
                        JUnitTestGURLs.URL_1.getSpec(),
                        /* title= */ null,
                        "Active Tab Title",
                        /* tabCount= */ 3);
        entries.add(closedWindow);
        when(mRecentlyClosedEntriesManager.getRecentlyClosedEntries()).thenReturn(entries);

        List<MenuItem> expectedSubmenu =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.open_history_menu_id),
                                item(R.id.recent_tabs_menu_id),
                                item(R.id.divider_line_id),
                                item(R.id.recent_tabs_header_menu_id),
                                item(
                                        R.id.recent_entry_menu_item,
                                        item(R.id.recent_entry_window_menu_item))));

        List<ListItem> items =
                mHistoryItemBuilder
                        .buildHistoryParentItem()
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        assertMenuItemsAreEqual(items, expectedSubmenu);

        Context context = ContextUtils.getApplicationContext();
        String tabsText =
                context.getResources()
                        .getQuantityString(R.plurals.recent_tabs_group_closure_without_title, 3, 3);
        String restoreText = context.getString(R.string.menu_recent_entry_restore_window);

        List<MenuItem> expectedTitles =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.string.menu_history),
                                item(R.string.menu_recent_tabs),
                                item(0),
                                item(R.string.recent_tabs),
                                item(tabsText, item(restoreText))));

        assertMenuTitlesAreEqual(items, expectedTitles);

        // Verify the recent entry itself in the model.
        // Index 4 is the first recently closed entry in the submenu (after the default history
        // actions: History, Recent Tabs, the Divider, and the Recent Tabs header).
        ListItem windowItem = items.get(4);
        List<ListItem> windowSubmenu =
                windowItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem restoreItem = windowSubmenu.get(0);
        assertEquals(
                closedWindow, restoreItem.model.get(AppMenuRecentEntryItemProperties.RECENT_ENTRY));
    }

    private void runHistorySubmenuWithRecentlyClosedGroupTest(
            String title, List<MenuItem> expectedTitles) {
        RecentlyClosedGroup closedGroup =
                new RecentlyClosedGroup(
                        /* sessionId= */ 1, /* timestamp= */ 0, title, /* color= */ 0);
        RecentlyClosedTab tab1 =
                new RecentlyClosedTab(
                        /* sessionId= */ 10,
                        /* timestamp= */ 0,
                        "Tab 1 Title",
                        JUnitTestGURLs.URL_1,
                        /* tabGroupId= */ null);
        RecentlyClosedTab tab2 =
                new RecentlyClosedTab(
                        /* sessionId= */ 20,
                        /* timestamp= */ 0,
                        "Tab 2 Title",
                        JUnitTestGURLs.URL_2,
                        /* tabGroupId= */ null);
        closedGroup.getTabs().add(tab1);
        closedGroup.getTabs().add(tab2);

        List<RecentlyClosedEntry> entries = new ArrayList<>();
        entries.add(closedGroup);
        when(mRecentlyClosedEntriesManager.getRecentlyClosedEntries()).thenReturn(entries);

        List<MenuItem> expectedSubmenu =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.open_history_menu_id),
                                item(R.id.recent_tabs_menu_id),
                                item(R.id.divider_line_id),
                                item(R.id.recent_tabs_header_menu_id),
                                item(
                                        R.id.recent_entry_menu_item,
                                        item(R.id.recent_entry_group_menu_item),
                                        item(R.id.divider_line_id),
                                        item(R.id.recent_entry_tab_menu_item),
                                        item(R.id.recent_entry_tab_menu_item))));

        List<ListItem> items =
                mHistoryItemBuilder
                        .buildHistoryParentItem()
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        assertMenuItemsAreEqual(items, expectedSubmenu);

        assertMenuTitlesAreEqual(items, expectedTitles);

        // Verify the recent entry itself in the model.
        // Index 4 is the first recently closed entry in the submenu.
        ListItem groupItem = items.get(4);
        List<ListItem> groupSubmenu =
                groupItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem restoreItem = groupSubmenu.get(0);
        assertEquals(
                closedGroup, restoreItem.model.get(AppMenuRecentEntryItemProperties.RECENT_ENTRY));
    }

    @Test
    public void testHistorySubmenu_WithRecentlyClosedNamedGroup() {
        Context context = ContextUtils.getApplicationContext();
        String tabsText =
                context.getResources()
                        .getQuantityString(R.plurals.recent_tabs_group_closure_without_title, 2, 2);
        String restoreText = context.getString(R.string.menu_recent_entry_restore_group);

        List<MenuItem> expectedTitles =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.string.menu_history),
                                item(R.string.menu_recent_tabs),
                                item(0),
                                item(R.string.recent_tabs),
                                item(
                                        context.getString(
                                                R.string.menu_window_title_with_tab_count,
                                                "Custom Group",
                                                tabsText),
                                        item(restoreText),
                                        item(0),
                                        item("Tab 1 Title"),
                                        item("Tab 2 Title"))));

        runHistorySubmenuWithRecentlyClosedGroupTest("Custom Group", expectedTitles);
    }

    @Test
    public void testHistorySubmenu_WithRecentlyClosedUnnamedGroup() {
        Context context = ContextUtils.getApplicationContext();
        String tabsText =
                context.getResources()
                        .getQuantityString(R.plurals.recent_tabs_group_closure_without_title, 2, 2);
        String restoreText = context.getString(R.string.menu_recent_entry_restore_group);

        List<MenuItem> expectedTitles =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.string.menu_history),
                                item(R.string.menu_recent_tabs),
                                item(0),
                                item(R.string.recent_tabs),
                                item(
                                        tabsText,
                                        item(restoreText),
                                        item(0),
                                        item("Tab 1 Title"),
                                        item("Tab 2 Title"))));

        runHistorySubmenuWithRecentlyClosedGroupTest("", expectedTitles);
    }

    @Test
    public void testHistorySubmenu_WithForeignSessions() {
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(new ForeignSessionTab(JUnitTestGURLs.URL_1, "Tab 1 Title", 0, 0, 10));
        tabs.add(new ForeignSessionTab(JUnitTestGURLs.URL_2, "Tab 2 Title", 0, 0, 20));

        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(new ForeignSessionWindow(0, 1, tabs));

        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(new ForeignSession("tag1", "Laptop", 0, windows, FormFactor.DESKTOP));

        when(mForeignSessionHelperMock.getForeignSessions()).thenReturn(sessions);

        List<MenuItem> expectedSubmenu =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.open_history_menu_id),
                                item(R.id.recent_tabs_menu_id),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.recent_entry_menu_item,
                                        item(R.id.recent_entry_foreign_tab_menu_item),
                                        item(R.id.recent_entry_foreign_tab_menu_item))));

        List<MenuItem> expectedTitles =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.string.menu_history),
                                item(R.string.menu_recent_tabs),
                                item(0),
                                item("Laptop", item("Tab 1 Title"), item("Tab 2 Title"))));

        List<ListItem> items =
                mHistoryItemBuilder
                        .buildHistoryParentItem()
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        assertMenuItemsAreEqual(items, expectedSubmenu);

        assertMenuTitlesAreEqual(items, expectedTitles);

        ListItem laptopSessionItem = items.get(3);
        List<ListItem> laptopSubmenu =
                laptopSessionItem
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        ListItem tab1Item = laptopSubmenu.get(0);
        assertEquals(
                "tag1", tab1Item.model.get(AppMenuRecentEntryItemProperties.FOREIGN_SESSION_TAG));
        assertEquals(
                tabs.get(0),
                tab1Item.model.get(AppMenuRecentEntryItemProperties.FOREIGN_SESSION_TAB));

        ListItem tab2Item = laptopSubmenu.get(1);
        assertEquals(
                "tag1", tab2Item.model.get(AppMenuRecentEntryItemProperties.FOREIGN_SESSION_TAG));
        assertEquals(
                tabs.get(1),
                tab2Item.model.get(AppMenuRecentEntryItemProperties.FOREIGN_SESSION_TAB));
    }
}
