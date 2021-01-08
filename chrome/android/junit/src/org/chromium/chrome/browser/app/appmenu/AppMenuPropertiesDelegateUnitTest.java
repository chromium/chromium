// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.widget.PopupMenu;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl.AppMenuSimilarSelectionType;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl.MenuGroup;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.device.ShadowDeviceConditions;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content.browser.ContentFeatureListImpl;
import org.chromium.content.browser.ContentFeatureListImplJni;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.ConnectionType;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Unit tests for {@link AppMenuPropertiesDelegateImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AppMenuPropertiesDelegateUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private ActivityTabProvider mActivityTabProvider;
    @Mock
    private Tab mTab;
    @Mock
    private WebContents mWebContents;
    @Mock
    private NavigationController mNavigationController;
    @Mock
    private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mTabModel;
    @Mock
    private TabModel mIncognitoTabModel;
    @Mock
    private ToolbarManager mToolbarManager;
    @Mock
    private View mDecorView;
    @Mock
    private OverviewModeBehavior mOverviewModeBehavior;
    @Mock
    private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock
    private ContentFeatureListImpl.Natives mContentFeatureListJniMock;
    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;
    @Mock
    private Profile.Natives mProfileJniMock;
    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;
    @Mock
    private ModalDialogManager mModalDialogManager;

    private OneshotSupplierImpl<OverviewModeBehavior> mOverviewModeSupplier =
            new OneshotSupplierImpl<>();
    private ObservableSupplierImpl<BookmarkBridge> mBookmarkBridgeSupplier =
            new ObservableSupplierImpl<>();

    private AppMenuPropertiesDelegateImpl mAppMenuPropertiesDelegate;

    private UpdateMenuItemHelper.MenuUiState mMenuUiState;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mOverviewModeSupplier.set(mOverviewModeBehavior);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn((mTabModel));
        when(mTabModelSelector.getModel(true)).thenReturn((mIncognitoTabModel));
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
        mMenuUiState = new UpdateMenuItemHelper.MenuUiState();
        doReturn(mMenuUiState).when(mUpdateMenuItemHelper).getUiState();

        mJniMocker.mock(ContentFeatureListImplJni.TEST_HOOKS, mContentFeatureListJniMock);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        mJniMocker.mock(ProfileJni.TEST_HOOKS, mProfileJniMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        Mockito.when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        Mockito.when(mProfileJniMock.fromWebContents(any())).thenReturn(mProfile);
        FeatureList.setTestCanUseDefaultsForTesting();

        mAppMenuPropertiesDelegate = Mockito.spy(new AppMenuPropertiesDelegateImpl(
                ContextUtils.getApplicationContext(), mActivityTabProvider,
                mMultiWindowModeStateDispatcher, mTabModelSelector, mToolbarManager, mDecorView,
                mOverviewModeSupplier, mBookmarkBridgeSupplier, mModalDialogManager));
    }

    @After
    public void tearDown() {
        ThreadUtils.setThreadAssertsDisabledForTesting(false);
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
        ChromeAccessibilityUtil.get().setTouchExplorationEnabledForTesting(false);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShouldShowPageMenu_Phone() {
        setUpMocksForPageMenu();
        Assert.assertTrue(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowPageMenu_Tablet() {
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(false);
        when(mTabModel.getCount()).thenReturn(1);
        Assert.assertTrue(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowOverviewMenu_Tablet() {
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(true);
        when(mTabModel.getCount()).thenReturn(1);
        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(
                MenuGroup.OVERVIEW_MODE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShouldShowIconRow_Phone() {
        Assert.assertTrue(mAppMenuPropertiesDelegate.shouldShowIconRow());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowIconRow_Tablet() {
        when(mDecorView.getWidth())
                .thenReturn((int) (600
                        * ContextUtils.getApplicationContext()
                                  .getResources()
                                  .getDisplayMetrics()
                                  .density));
        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldShowIconRow());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowIconRow_TabletNarrow() {
        when(mDecorView.getWidth())
                .thenReturn((int) (100
                        * ContextUtils.getApplicationContext()
                                  .getResources()
                                  .getDisplayMetrics()
                                  .density));
        Assert.assertTrue(mAppMenuPropertiesDelegate.shouldShowIconRow());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_Ntp() {
        setUpMocksForPageMenu();
        when(mTab.getUrlString()).thenReturn(UrlConstants.NTP_URL);
        when(mTab.isNativePage()).thenReturn(true);
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.all_bookmarks_menu_id,
                R.id.recent_tabs_menu_id, R.id.open_history_menu_id, R.id.downloads_menu_id,
                R.id.request_desktop_site_row_menu_id, R.id.preferences_id, R.id.help_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage() {
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, false /*showUpdate*/,
                false /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, false /*showPaintPreview*/);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.all_bookmarks_menu_id,
                R.id.recent_tabs_menu_id, R.id.open_history_menu_id, R.id.downloads_menu_id,
                R.id.translate_id, R.id.share_row_menu_id, R.id.find_in_page_id,
                R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.preferences_id, R.id.help_id};
        Integer[] expectedTitles = {0, R.string.menu_new_tab, R.string.menu_new_incognito_tab,
                R.string.menu_bookmarks, R.string.menu_recent_tabs, R.string.menu_history,
                R.string.menu_downloads, R.string.menu_translate, 0, R.string.menu_find_in_page,
                R.string.menu_add_to_homescreen, 0, R.string.menu_settings, R.string.menu_help};
        Integer[] expectedActionBarItems = {R.id.forward_menu_id, R.id.bookmark_this_page_id,
                R.id.offline_page_id, R.id.info_menu_id, R.id.reload_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
        assertMenuTitlesAreEqual(menu, expectedTitles);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_WithPwa() {
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, false /*showUpdate*/,
                false /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, false /*showPaintPreview*/);
        doReturn(new AppBannerManager.InstallStringPair(R.string.menu_add_to_homescreen_install,
                         R.string.menu_add_to_homescreen_install))
                .when(mAppMenuPropertiesDelegate)
                .getAddToHomeScreenTitle(mTab);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.all_bookmarks_menu_id,
                R.id.recent_tabs_menu_id, R.id.open_history_menu_id, R.id.downloads_menu_id,
                R.id.translate_id, R.id.share_row_menu_id, R.id.find_in_page_id,
                R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.preferences_id, R.id.help_id};
        Integer[] expectedTitles = {0, R.string.menu_new_tab, R.string.menu_new_incognito_tab,
                R.string.menu_bookmarks, R.string.menu_recent_tabs, R.string.menu_history,
                R.string.menu_downloads, R.string.menu_translate, 0, R.string.menu_find_in_page,
                R.string.menu_add_to_homescreen_install, 0, R.string.menu_settings,
                R.string.menu_help};
        Integer[] expectedActionBarItems = {R.id.forward_menu_id, R.id.bookmark_this_page_id,
                R.id.offline_page_id, R.id.info_menu_id, R.id.reload_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
        assertMenuTitlesAreEqual(menu, expectedTitles);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_enterprise_user() {
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, false /*showUpdate*/,
                false /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, false /*showPaintPreview*/);
        doReturn(true).when(mAppMenuPropertiesDelegate).shouldShowManagedByMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.all_bookmarks_menu_id,
                R.id.recent_tabs_menu_id, R.id.open_history_menu_id, R.id.downloads_menu_id,
                R.id.translate_id, R.id.share_row_menu_id, R.id.find_in_page_id,
                R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.preferences_id, R.id.help_id, R.id.managed_by_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItemsIcons_Phone_RegularPage_iconsAfterMenuItems() {
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, true /*showUpdate*/,
                true /*showMoveToOtherWindow*/, true /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, true /*showPaintPreview*/);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.update_menu_id, R.id.reader_mode_prefs_id};
        assertMenuItemsHaveIcons(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItemsIcons_Phone_RegularPage_iconsBeforeMenuItems() {
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, true /*showUpdate*/,
                false /*showMoveToOtherWindow*/, true /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, false /*showPaintPreview*/);
        doReturn(true).when(mAppMenuPropertiesDelegate).shouldShowIconBeforeItem();

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.update_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.all_bookmarks_menu_id,
                R.id.recent_tabs_menu_id, R.id.open_history_menu_id, R.id.downloads_menu_id,
                R.id.translate_id, R.id.find_in_page_id, R.id.add_to_homescreen_id,
                R.id.reader_mode_prefs_id, R.id.preferences_id, R.id.help_id};
        assertMenuItemsHaveIcons(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_regroup() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP, true);
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, true /*showUpdate*/,
                true /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, true /*showPaintPreview*/);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.update_menu_id,
                R.id.move_to_other_window_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.divider_line_id, R.id.open_history_menu_id,
                R.id.downloads_row_menu_id, R.id.all_bookmarks_row_menu_id,
                R.id.recent_tabs_menu_id, R.id.divider_line_id, R.id.share_row_menu_id,
                R.id.paint_preview_show_id, R.id.find_in_page_id, R.id.translate_id,
                R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.divider_line_id, R.id.preferences_id, R.id.help_id};
        Integer[] expectedActionBarItems = {R.id.forward_menu_id, R.id.bookmark_this_page_id,
                R.id.offline_page_id, R.id.info_menu_id, R.id.reload_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_backward_button_action_bar() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP, true);
        AppMenuPropertiesDelegateImpl.ACTION_BAR_VARIATION.setForTesting("backward_button");
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, false /*showUpdate*/,
                false /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, false /*showPaintPreview*/);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.divider_line_id, R.id.open_history_menu_id,
                R.id.downloads_row_menu_id, R.id.all_bookmarks_row_menu_id,
                R.id.recent_tabs_menu_id, R.id.divider_line_id, R.id.share_row_menu_id,
                R.id.find_in_page_id, R.id.translate_id, R.id.add_to_homescreen_id,
                R.id.request_desktop_site_row_menu_id, R.id.divider_line_id, R.id.preferences_id,
                R.id.info_id, R.id.help_id};
        Integer[] expectedActionBarItems = {R.id.backward_menu_id, R.id.forward_menu_id,
                R.id.offline_page_id, R.id.bookmark_this_page_id, R.id.reload_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_share_button_action_bar() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP, true);
        AppMenuPropertiesDelegateImpl.ACTION_BAR_VARIATION.setForTesting("share_button");
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, false /*showUpdate*/,
                false /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, false /*showPaintPreview*/);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.divider_line_id, R.id.open_history_menu_id,
                R.id.downloads_row_menu_id, R.id.all_bookmarks_row_menu_id,
                R.id.recent_tabs_menu_id, R.id.divider_line_id, R.id.find_in_page_id,
                R.id.translate_id, R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.divider_line_id, R.id.preferences_id, R.id.info_id, R.id.help_id};
        Integer[] expectedActionBarItems = {R.id.forward_menu_id, R.id.bookmark_this_page_id,
                R.id.offline_page_id, R.id.share_menu_button_id, R.id.reload_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_threebutton_actionbar() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP, true);
        AppMenuPropertiesDelegateImpl.ACTION_BAR_VARIATION.setForTesting("");
        CachedFeatureFlags.setForTesting(
                ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_THREE_BUTTON_ACTIONBAR, true);
        AppMenuPropertiesDelegateImpl.THREE_BUTTON_ACTION_BAR_VARIATION.setForTesting(
                "action_chip_view");
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, true /*showUpdate*/,
                true /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, true /*showPaintPreview*/);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.update_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.move_to_other_window_menu_id,
                R.id.divider_line_id, R.id.open_history_menu_id, R.id.downloads_row_menu_id,
                R.id.all_bookmarks_row_menu_id, R.id.recent_tabs_menu_id, R.id.divider_line_id,
                R.id.share_row_menu_id, R.id.paint_preview_show_id, R.id.find_in_page_id,
                R.id.translate_id, R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.divider_line_id, R.id.preferences_id, R.id.help_id};
        Integer[] expectedActionBarItems = {
                R.id.forward_menu_id, R.id.info_menu_id, R.id.reload_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_threebutton_actionbar_backward_actionbar() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP, true);
        CachedFeatureFlags.setForTesting(
                ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_THREE_BUTTON_ACTIONBAR, true);
        AppMenuPropertiesDelegateImpl.ACTION_BAR_VARIATION.setForTesting("backward_button");
        AppMenuPropertiesDelegateImpl.THREE_BUTTON_ACTION_BAR_VARIATION.setForTesting(
                "action_chip_view");
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, true /*showUpdate*/,
                true /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, true /*showPaintPreview*/);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.update_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.move_to_other_window_menu_id,
                R.id.divider_line_id, R.id.open_history_menu_id, R.id.downloads_row_menu_id,
                R.id.all_bookmarks_row_menu_id, R.id.recent_tabs_menu_id, R.id.divider_line_id,
                R.id.share_row_menu_id, R.id.paint_preview_show_id, R.id.find_in_page_id,
                R.id.translate_id, R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.divider_line_id, R.id.preferences_id, R.id.info_id, R.id.help_id};
        Integer[] expectedActionBarItems = {
                R.id.backward_menu_id, R.id.forward_menu_id, R.id.reload_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_threebutton_actionbar_share_actionbar() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP, true);
        CachedFeatureFlags.setForTesting(
                ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_THREE_BUTTON_ACTIONBAR, true);
        AppMenuPropertiesDelegateImpl.ACTION_BAR_VARIATION.setForTesting("share_button");
        AppMenuPropertiesDelegateImpl.THREE_BUTTON_ACTION_BAR_VARIATION.setForTesting(
                "destination_chip_view");
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, true /*showUpdate*/,
                true /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, true /*showPaintPreview*/);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.update_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.move_to_other_window_menu_id,
                R.id.divider_line_id, R.id.open_history_menu_id, R.id.downloads_row_menu_id,
                R.id.all_bookmarks_row_menu_id, R.id.recent_tabs_menu_id, R.id.divider_line_id,
                R.id.paint_preview_show_id, R.id.find_in_page_id, R.id.translate_id,
                R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.divider_line_id, R.id.preferences_id, R.id.info_id, R.id.help_id};
        Integer[] expectedActionBarItems = {
                R.id.forward_menu_id, R.id.share_menu_button_id, R.id.reload_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_threebutton_actionbar_add_to_menuitem() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP, true);
        CachedFeatureFlags.setForTesting(
                ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_THREE_BUTTON_ACTIONBAR, true);
        CachedFeatureFlags.setForTesting(ChromeFeatureList.READ_LATER, true);
        AppMenuPropertiesDelegateImpl.ACTION_BAR_VARIATION.setForTesting("share_button");
        AppMenuPropertiesDelegateImpl.THREE_BUTTON_ACTION_BAR_VARIATION.setForTesting(
                "add_to_option");
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, true /*showUpdate*/,
                true /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                true /*showAddToHomeScreen*/, true /*showPaintPreview*/);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.update_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.move_to_other_window_menu_id,
                R.id.divider_line_id, R.id.open_history_menu_id, R.id.downloads_row_menu_id,
                R.id.all_bookmarks_row_menu_id, R.id.recent_tabs_menu_id,
                R.id.add_to_divider_line_id, R.id.add_to_menu_id, R.id.divider_line_id,
                R.id.paint_preview_show_id, R.id.find_in_page_id, R.id.translate_id,
                R.id.request_desktop_site_row_menu_id, R.id.divider_line_id, R.id.preferences_id,
                R.id.info_id, R.id.help_id};
        Integer[] expectedActionBarItems = {
                R.id.forward_menu_id, R.id.share_menu_button_id, R.id.reload_menu_id};
        Integer[] expectedAddToItems = {R.id.add_to_bookmarks_menu_id,
                R.id.add_to_reading_list_menu_id, R.id.add_to_downloads_menu_id,
                R.id.add_to_homescreen_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
        assertAddToItemsAreEqual(menu, expectedAddToItems);
        assertAddToItemsEnableState(
                menu, new HashSet<>(Arrays.asList(R.id.add_to_reading_list_menu_id)), null);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void
    testPageMenuItems_Phone_RegularPage_threebutton_actionbar_add_to_menuitem_intall_app() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP, true);
        CachedFeatureFlags.setForTesting(
                ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_THREE_BUTTON_ACTIONBAR, true);
        AppMenuPropertiesDelegateImpl.ACTION_BAR_VARIATION.setForTesting("share_button");
        AppMenuPropertiesDelegateImpl.THREE_BUTTON_ACTION_BAR_VARIATION.setForTesting(
                "add_to_option");
        setUpMocksForPageMenu();
        setMenuOptions(false /*isNativePage*/, true /*showTranslate*/, true /*showUpdate*/,
                true /*showMoveToOtherWindow*/, false /*showReaderModePrefs*/,
                false /*showAddToHomeScreen*/, true /*showPaintPreview*/);
        doReturn(new AppBannerManager.InstallStringPair(
                         R.string.menu_add_to_homescreen_install, R.string.app_banner_install))
                .when(mAppMenuPropertiesDelegate)
                .getAddToHomeScreenTitle(mTab);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.update_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.move_to_other_window_menu_id,
                R.id.divider_line_id, R.id.open_history_menu_id, R.id.downloads_row_menu_id,
                R.id.all_bookmarks_row_menu_id, R.id.recent_tabs_menu_id,
                R.id.add_to_divider_line_id, R.id.add_to_menu_id, R.id.install_app_id,
                R.id.divider_line_id, R.id.paint_preview_show_id, R.id.find_in_page_id,
                R.id.translate_id, R.id.request_desktop_site_row_menu_id, R.id.divider_line_id,
                R.id.preferences_id, R.id.info_id, R.id.help_id};
        Integer[] expectedActionBarItems = {
                R.id.forward_menu_id, R.id.share_menu_button_id, R.id.reload_menu_id};
        Integer[] expectedAddToItems = {
                R.id.add_to_bookmarks_menu_id, R.id.add_to_downloads_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
        assertAddToItemsAreEqual(menu, expectedAddToItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testOverviewMenuItems_Phone() {
        setUpMocksForOverviewMenu();
        when(mIncognitoTabModel.getCount()).thenReturn(0);

        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(
                MenuGroup.OVERVIEW_MODE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.new_tab_menu_id, R.id.new_incognito_tab_menu_id,
                R.id.close_all_tabs_menu_id, R.id.preferences_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOverviewMenuItems_Tablet_NoTabs() {
        setUpIncognitoMocks();
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(false);
        when(mTabModel.getCount()).thenReturn(0);

        Assert.assertEquals(
                MenuGroup.TABLET_EMPTY_MODE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldShowPageMenu());

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
                R.id.new_tab_menu_id, R.id.new_incognito_tab_menu_id, R.id.preferences_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    public void testMenuItems_Accessibility_ImageDescriptions() {
        setUpMocksForPageMenu();
        when(mTab.getUrlString()).thenReturn("https://google.com");
        when(mTab.isNativePage()).thenReturn(false);
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class), anyBoolean());
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));
        doReturn(new AppBannerManager.InstallStringPair(
                         R.string.menu_add_to_homescreen, R.string.add))
                .when(mAppMenuPropertiesDelegate)
                .getAddToHomeScreenTitle(mTab);

        // Ensure the get image descriptions option is shown as needed
        when(mContentFeatureListJniMock.isEnabled(
                     ContentFeatureList.EXPERIMENTAL_ACCESSIBILITY_LABELS))
                .thenReturn(true);
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);

        // Test specific setup
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
        ChromeAccessibilityUtil.get().setTouchExplorationEnabledForTesting(true);

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.all_bookmarks_menu_id,
                R.id.recent_tabs_menu_id, R.id.open_history_menu_id, R.id.downloads_menu_id,
                R.id.share_row_menu_id, R.id.get_image_descriptions_id, R.id.find_in_page_id,
                R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.preferences_id, R.id.help_id};

        assertMenuItemsAreEqual(menu, expectedItems);

        // Ensure the text of the menu item is correct
        Assert.assertEquals(
                "Get image descriptions", menu.findItem(R.id.get_image_descriptions_id).getTitle());

        // Enable the feature and ensure text changes
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(true);

        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        Assert.assertEquals("Stop image descriptions",
                menu.findItem(R.id.get_image_descriptions_id).getTitle());

        // Setup no wifi condition, and "only on wifi" user option.
        DeviceConditions noWifi =
                new DeviceConditions(false, 75, ConnectionType.CONNECTION_2G, false, false, true);
        ShadowDeviceConditions.setCurrentConditions(noWifi);
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI))
                .thenReturn(true);

        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        Assert.assertEquals(
                "Get image descriptions", menu.findItem(R.id.get_image_descriptions_id).getTitle());
    }

    @Test
    public void testMenuItems_AppMenuSimilarSelectionChecker() {
        Assert.assertEquals("No match for bookmark page then all bookmarks",
                AppMenuSimilarSelectionType.BOOKMARK_PAGE_THEN_ALL_BOOKMARKS,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.bookmark_this_page_id, R.id.all_bookmarks_menu_id));
        Assert.assertEquals("No match for bookmark page then all bookmarks",
                AppMenuSimilarSelectionType.BOOKMARK_PAGE_THEN_ALL_BOOKMARKS,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.bookmark_this_page_chip_id, R.id.all_bookmarks_menu_id));
        Assert.assertEquals("No match for bookmark page then all bookmarks",
                AppMenuSimilarSelectionType.BOOKMARK_PAGE_THEN_ALL_BOOKMARKS,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.add_to_bookmarks_menu_id, R.id.all_bookmarks_menu_id));
        Assert.assertTrue("Should return true for bookmark page then all bookmarks",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.bookmark_this_page_id, R.id.all_bookmarks_menu_id));
        Assert.assertTrue("Should return true for bookmark page then all bookmarks",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.bookmark_this_page_chip_id, R.id.all_bookmarks_menu_id));
        Assert.assertTrue("Should return true for bookmark page then all bookmarks",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.add_to_bookmarks_menu_id, R.id.all_bookmarks_menu_id));

        Assert.assertEquals("No match for all bookmarks then bookmark page",
                AppMenuSimilarSelectionType.ALL_BOOKMARKS_THEN_BOOKMARK_PAGE,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.all_bookmarks_menu_id, R.id.bookmark_this_page_id));
        Assert.assertEquals("No match for all bookmarks then bookmark page",
                AppMenuSimilarSelectionType.ALL_BOOKMARKS_THEN_BOOKMARK_PAGE,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.all_bookmarks_menu_id, R.id.bookmark_this_page_chip_id));
        Assert.assertEquals("No match for all bookmarks then bookmark page",
                AppMenuSimilarSelectionType.ALL_BOOKMARKS_THEN_BOOKMARK_PAGE,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.all_bookmarks_menu_id, R.id.add_to_bookmarks_menu_id));
        Assert.assertTrue("Should return true for all bookmarks then bookmark page",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.all_bookmarks_menu_id, R.id.bookmark_this_page_id));
        Assert.assertTrue("Should return true for all bookmarks then bookmark page",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.all_bookmarks_menu_id, R.id.bookmark_this_page_chip_id));
        Assert.assertTrue("Should return true for all bookmarks then bookmark page",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.all_bookmarks_menu_id, R.id.add_to_bookmarks_menu_id));

        Assert.assertEquals("No match for download page then all downloads",
                AppMenuSimilarSelectionType.DOWNLOAD_PAGE_THEN_ALL_DOWNLOADS,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.offline_page_id, R.id.downloads_menu_id));
        Assert.assertEquals("No match for download page then all downloads",
                AppMenuSimilarSelectionType.DOWNLOAD_PAGE_THEN_ALL_DOWNLOADS,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.offline_page_chip_id, R.id.downloads_menu_id));
        Assert.assertEquals("No match for download page then all downloads",
                AppMenuSimilarSelectionType.DOWNLOAD_PAGE_THEN_ALL_DOWNLOADS,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.add_to_downloads_menu_id, R.id.downloads_menu_id));
        Assert.assertTrue("Should return true for download page then all downloads",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.offline_page_id, R.id.downloads_menu_id));
        Assert.assertTrue("Should return true for download page then all downloads",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.offline_page_chip_id, R.id.downloads_menu_id));
        Assert.assertTrue("Should return true for download page then all downloads",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.add_to_downloads_menu_id, R.id.downloads_menu_id));

        Assert.assertEquals("No match for all downloads then download page",
                AppMenuSimilarSelectionType.ALL_DOWNLOADS_THEN_DOWNLOAD_PAGE,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.downloads_menu_id, R.id.offline_page_id));
        Assert.assertEquals("No match for all downloads then download page",
                AppMenuSimilarSelectionType.ALL_DOWNLOADS_THEN_DOWNLOAD_PAGE,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.downloads_menu_id, R.id.offline_page_chip_id));
        Assert.assertEquals("No match for all downloads then download page",
                AppMenuSimilarSelectionType.ALL_DOWNLOADS_THEN_DOWNLOAD_PAGE,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.downloads_menu_id, R.id.add_to_downloads_menu_id));
        Assert.assertTrue("Should return true for all downloads then download page",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.downloads_menu_id, R.id.offline_page_id));
        Assert.assertTrue("Should return true for all downloads then download page",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.downloads_menu_id, R.id.offline_page_chip_id));
        Assert.assertTrue("Should return true for all downloads then download page",
                mAppMenuPropertiesDelegate.recordAppMenuSimilarSelectionIfNeeded(
                        R.id.downloads_menu_id, R.id.add_to_downloads_menu_id));

        Assert.assertEquals("Should no match for all downloads then all bookmarks",
                AppMenuSimilarSelectionType.NO_MATCH,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.downloads_menu_id, R.id.all_bookmarks_menu_id));
        Assert.assertEquals("Should no match for all bookmarks then share",
                AppMenuSimilarSelectionType.NO_MATCH,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.all_bookmarks_menu_id, R.id.share_menu_button_id));
        Assert.assertEquals("Should no match for new tab then find in page",
                AppMenuSimilarSelectionType.NO_MATCH,
                mAppMenuPropertiesDelegate.findSimilarSelectionPattern(
                        R.id.new_tab_menu_id, R.id.find_in_page_id));
    }

    private void setUpMocksForPageMenu() {
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(false);
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldCheckBookmarkStar(any(Tab.class));
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldEnableDownloadPage(any(Tab.class));
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldShowReaderModePrefs(any(Tab.class));
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowManagedByMenuItem(any(Tab.class));
        setUpIncognitoMocks();
    }

    private void setUpMocksForOverviewMenu() {
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        setUpIncognitoMocks();
    }

    private void setUpIncognitoMocks() {
        doReturn(true).when(mAppMenuPropertiesDelegate).isIncognitoEnabled();
    }

    private Menu createTestMenu() {
        PopupMenu tempMenu = new PopupMenu(ContextUtils.getApplicationContext(), mDecorView);
        tempMenu.inflate(mAppMenuPropertiesDelegate.getAppMenuLayoutId());
        Menu menu = tempMenu.getMenu();
        return menu;
    }

    private void assertMenuItemsAreEqual(Menu menu, Integer... expectedItems) {
        List<Integer> actualItems = new ArrayList<>();
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).isVisible()) {
                actualItems.add(menu.getItem(i).getItemId());
            }
        }

        Assert.assertThat("Populated menu items were:" + getMenuTitles(menu), actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private void assertMenuTitlesAreEqual(Menu menu, Integer... expectedTitles) {
        Context context = ContextUtils.getApplicationContext();
        int expectedIndex = 0;
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).isVisible()) {
                Assert.assertEquals(expectedTitles[expectedIndex] == 0
                                ? null
                                : context.getString(expectedTitles[expectedIndex]),
                        menu.getItem(i).getTitle());
                expectedIndex++;
            }
        }
    }

    private void assertActionBarItemsAreEqual(Menu menu, Integer... expectedItems) {
        SubMenu actionBar = menu.findItem(R.id.icon_row_menu_id).getSubMenu();
        List<Integer> actualItems = new ArrayList<>();
        for (int i = 0; i < actionBar.size(); i++) {
            if (actionBar.getItem(i).isVisible()) {
                actualItems.add(actionBar.getItem(i).getItemId());
            }
        }

        Assert.assertThat("Populated action bar items were:" + getMenuTitles(actionBar),
                actualItems, Matchers.containsInAnyOrder(expectedItems));
    }

    private void assertMenuItemsHaveIcons(Menu menu, Integer... expectedItems) {
        List<Integer> actualItems = new ArrayList<>();
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).isVisible() && menu.getItem(i).getIcon() != null) {
                actualItems.add(menu.getItem(i).getItemId());
            }
        }

        Assert.assertThat("menu items with icons were:" + getMenuTitles(menu), actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private void assertAddToItemsAreEqual(Menu menu, Integer... expectedItems) {
        SubMenu addToSubMenu = menu.findItem(R.id.add_to_menu_id).getSubMenu();
        List<Integer> actualItems = new ArrayList<>();
        for (int i = 0; i < addToSubMenu.size(); i++) {
            if (addToSubMenu.getItem(i).isVisible()) {
                actualItems.add(addToSubMenu.getItem(i).getItemId());
            }
        }

        Assert.assertThat("Populated add to items were:" + getMenuTitles(addToSubMenu), actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private void assertAddToItemsEnableState(
            Menu menu, Set<Integer> enabledItems, Set<Integer> disabledItems) {
        SubMenu addToSubMenu = menu.findItem(R.id.add_to_menu_id).getSubMenu();
        for (int i = 0; i < addToSubMenu.size(); i++) {
            MenuItem item = addToSubMenu.getItem(i);
            if (enabledItems != null && enabledItems.contains(item.getItemId())) {
                Assert.assertTrue(item.isEnabled());
            }
            if (disabledItems != null && disabledItems.contains(item.getItemId())) {
                Assert.assertFalse(item.isEnabled());
            }
        }
    }

    private String getMenuTitles(Menu menu) {
        StringBuilder items = new StringBuilder();
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).isVisible()) {
                items.append("\n").append(menu.getItem(i).getTitle());
            }
        }
        return items.toString();
    }

    private void setMenuOptions(boolean isNativePage, boolean showTranslate, boolean showUpdate,
            boolean showMoveToOtherWindow, boolean showReaderModePrefs, boolean showAddToHomeScreen,
            boolean showPaintPreview) {
        when(mTab.getUrlString()).thenReturn("https://google.com");
        when(mTab.isNativePage()).thenReturn(isNativePage);
        doReturn(showTranslate)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));
        doReturn(showUpdate).when(mAppMenuPropertiesDelegate).shouldShowUpdateMenuItem();
        doReturn(showMoveToOtherWindow)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowMoveToOtherWindow();
        doReturn(showReaderModePrefs)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowReaderModePrefs(any(Tab.class));
        if (showAddToHomeScreen) {
            doReturn(new AppBannerManager.InstallStringPair(
                             R.string.menu_add_to_homescreen, R.string.add))
                    .when(mAppMenuPropertiesDelegate)
                    .getAddToHomeScreenTitle(mTab);
        }
        doReturn(showPaintPreview)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class), anyBoolean());
    }
}
