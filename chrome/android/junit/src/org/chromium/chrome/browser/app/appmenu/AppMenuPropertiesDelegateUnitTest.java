// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
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
import org.robolectric.annotation.LooperMode;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl.MenuGroup;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.device.ShadowDeviceConditions;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkMeta;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkType;
import org.chromium.chrome.browser.power_bookmarks.ShoppingSpecifics;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.read_later.ReadingListUtils;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.menu_button.MenuUiState;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.ConnectionType;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link AppMenuPropertiesDelegateImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
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
    private LayoutStateProvider mLayoutStateProvider;
    @Mock
    private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;
    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;
    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    private TabModelFilter mTabModelFilter;
    @Mock
    public WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock
    public BookmarkBridge mBookmarkBridge;
    @Mock
    private IdentityManager mIdentityManagerMock;
    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock
    private SyncService mSyncServiceMock;
    @Mock
    private ManagedBrowserUtils.Natives mManagedBrowserUtilsJniMock;

    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private ObservableSupplierImpl<BookmarkBridge> mBookmarkBridgeSupplier =
            new ObservableSupplierImpl<>();

    private final TestValues mTestValues = new TestValues();
    private AppMenuPropertiesDelegateImpl mAppMenuPropertiesDelegate;
    private MenuUiState mMenuUiState;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        setupFeatureDefaults();

        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn((mTabModel));
        when(mTabModelSelector.getModel(true)).thenReturn((mIncognitoTabModel));
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getCurrentTabModelFilter()).thenReturn(mTabModelFilter);
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
        mMenuUiState = new MenuUiState();
        doReturn(mMenuUiState).when(mUpdateMenuItemHelper).getUiState();

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        Mockito.when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        FeatureList.setTestCanUseDefaultsForTesting();
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);

        mJniMocker.mock(ManagedBrowserUtilsJni.TEST_HOOKS, mManagedBrowserUtilsJniMock);
        Mockito.when(mManagedBrowserUtilsJniMock.isBrowserManaged(mProfile)).thenReturn(false);
        Mockito.when(mManagedBrowserUtilsJniMock.getAccountManagerName(mProfile)).thenReturn("");

        mBookmarkBridgeSupplier.set(mBookmarkBridge);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        PowerBookmarkUtils.setPowerBookmarkMetaForTesting(PowerBookmarkMeta.newBuilder().build());
        mAppMenuPropertiesDelegate = Mockito.spy(new AppMenuPropertiesDelegateImpl(
                ContextUtils.getApplicationContext(), mActivityTabProvider,
                mMultiWindowModeStateDispatcher, mTabModelSelector, mToolbarManager, mDecorView,
                mLayoutStateProviderSupplier, null, mBookmarkBridgeSupplier));
    }

    private void setupFeatureDefaults() {
        setBookmarkItemRowEnabled(false);
        setReadingListItemRowEnabled(false);
        setShoppingListItemRowEnabled(false);
        FeatureList.setTestValues(mTestValues);
    }

    private void setBookmarkItemRowEnabled(boolean enabled) {
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.BOOKMARKS_REFRESH, enabled);
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.BOOKMARKS_REFRESH, "bookmark_in_app_menu", "true");
    }

    private void setReadingListItemRowEnabled(boolean enabled) {
        ReadingListUtils.setReadingListSupportedForTesting(true);
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.READ_LATER, enabled);
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.READ_LATER, "reading_list_in_app_menu", "true");
        FeatureList.setTestValues(mTestValues);
    }

    private void setShoppingListItemRowEnabled(boolean enabled) {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getIdentityManager(any(Profile.class)))
                .thenReturn(mIdentityManagerMock);
        TestThreadUtils.runOnUiThreadBlocking(() -> SyncService.overrideForTests(mSyncServiceMock));
        when(mIdentityManagerMock.hasPrimaryAccount(anyInt())).thenReturn(enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabledForTesting(
                    enabled);
        });
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.SHOPPING_LIST, enabled);
        when(mPrefService.getBoolean(Pref.WEB_AND_APP_ACTIVITY_ENABLED_FOR_SHOPPING))
                .thenReturn(true);
    }

    @After
    public void tearDown() {
        ThreadUtils.setThreadAssertsDisabledForTesting(false);
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
        ChromeAccessibilityUtil.get().setTouchExplorationEnabledForTesting(false);
        ReadingListUtils.setReadingListSupportedForTesting(null);
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
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        when(mTabModel.getCount()).thenReturn(1);
        Assert.assertTrue(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowOverviewMenu_Tablet() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
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
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        when(mTab.isNativePage()).thenReturn(true);
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.divider_line_id, R.id.open_history_menu_id,
                R.id.downloads_menu_id, R.id.all_bookmarks_menu_id, R.id.recent_tabs_menu_id,
                R.id.divider_line_id, R.id.request_desktop_site_row_menu_id, R.id.divider_line_id,
                R.id.preferences_id, R.id.help_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions()
                               .withShowTranslate()
                               .withShowAddToHomeScreen()
                               .withAutoDarkEnabled());

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.divider_line_id, R.id.open_history_menu_id,
                R.id.downloads_menu_id, R.id.all_bookmarks_menu_id, R.id.recent_tabs_menu_id,
                R.id.divider_line_id, R.id.share_row_menu_id, R.id.find_in_page_id,
                R.id.translate_id, R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.auto_dark_web_contents_row_menu_id, R.id.divider_line_id, R.id.preferences_id,
                R.id.help_id};
        Integer[] expectedTitles = {0, R.string.menu_new_tab, R.string.menu_new_incognito_tab, 0,
                R.string.menu_history, R.string.menu_downloads, R.string.menu_bookmarks,
                R.string.menu_recent_tabs, 0, 0, R.string.menu_find_in_page,
                R.string.menu_translate, R.string.menu_add_to_homescreen, 0, 0, 0,
                R.string.menu_settings, R.string.menu_help};
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
        setMenuOptions(new MenuOptions()
                               .withShowTranslate()
                               .withShowAddToHomeScreen()
                               .withAutoDarkEnabled());
        doReturn(new AppBannerManager.InstallStringPair(R.string.menu_add_to_homescreen_install,
                         R.string.menu_add_to_homescreen_install))
                .when(mAppMenuPropertiesDelegate)
                .getAddToHomeScreenTitle(mTab);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.divider_line_id, R.id.open_history_menu_id,
                R.id.downloads_menu_id, R.id.all_bookmarks_menu_id, R.id.recent_tabs_menu_id,
                R.id.divider_line_id, R.id.translate_id, R.id.share_row_menu_id,
                R.id.find_in_page_id, R.id.add_to_homescreen_id,
                R.id.request_desktop_site_row_menu_id, R.id.auto_dark_web_contents_row_menu_id,
                R.id.divider_line_id, R.id.preferences_id, R.id.help_id};
        Integer[] expectedTitles = {0, R.string.menu_new_tab, R.string.menu_new_incognito_tab, 0,
                R.string.menu_history, R.string.menu_downloads, R.string.menu_bookmarks,
                R.string.menu_recent_tabs, 0, 0, R.string.menu_find_in_page,
                R.string.menu_translate, R.string.menu_add_to_homescreen_install, 0, 0, 0,
                R.string.menu_settings, R.string.menu_help};
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
        setMenuOptions(new MenuOptions()
                               .withShowTranslate()
                               .withShowAddToHomeScreen()
                               .withAutoDarkEnabled());
        doReturn(true).when(mAppMenuPropertiesDelegate).shouldShowManagedByMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.divider_line_id, R.id.open_history_menu_id,
                R.id.downloads_menu_id, R.id.all_bookmarks_menu_id, R.id.recent_tabs_menu_id,
                R.id.divider_line_id, R.id.share_row_menu_id, R.id.find_in_page_id,
                R.id.translate_id, R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.auto_dark_web_contents_row_menu_id, R.id.divider_line_id, R.id.preferences_id,
                R.id.help_id, R.id.managed_by_divider_line_id, R.id.managed_by_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItemsIcons_Phone_RegularPage_iconsAfterMenuItems() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions().withAllSet().setNativePage(false));

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
        setMenuOptions(new MenuOptions()
                               .withAllSet()
                               .setNativePage(false)
                               .setShowMoveToOtherWindow(false)
                               .setShowPaintPreview(false));
        doReturn(true).when(mAppMenuPropertiesDelegate).shouldShowIconBeforeItem();

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.update_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.open_history_menu_id, R.id.downloads_menu_id,
                R.id.all_bookmarks_menu_id, R.id.recent_tabs_menu_id, R.id.translate_id,
                R.id.find_in_page_id, R.id.add_to_homescreen_id, R.id.reader_mode_prefs_id,
                R.id.preferences_id, R.id.help_id};
        assertMenuItemsHaveIcons(menu, expectedItems);
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
                R.id.close_all_tabs_menu_id, R.id.menu_group_tabs, R.id.preferences_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOverviewMenuItems_Tablet_NoTabs() {
        setUpIncognitoMocks();
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
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
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL));
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
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);

        // Test specific setup
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
        ChromeAccessibilityUtil.get().setTouchExplorationEnabledForTesting(true);

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.divider_line_id, R.id.open_history_menu_id,
                R.id.downloads_menu_id, R.id.all_bookmarks_menu_id, R.id.recent_tabs_menu_id,
                R.id.divider_line_id, R.id.share_row_menu_id, R.id.get_image_descriptions_id,
                R.id.find_in_page_id, R.id.add_to_homescreen_id,
                R.id.request_desktop_site_row_menu_id, R.id.auto_dark_web_contents_row_menu_id,
                R.id.divider_line_id, R.id.preferences_id, R.id.help_id};

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
    public void updateBookmarkMenuItemShortcut() {
        doReturn(true).when(mBookmarkBridge).isEditBookmarksEnabled();

        MenuItem bookmarkMenuItemShortcut = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, mTab, /*fromCCT=*/false);
        verify(bookmarkMenuItemShortcut).setEnabled(true);
    }

    @Test
    public void updateBookmarkMenuItemShortcut_fromCCT() {
        doReturn(true).when(mBookmarkBridge).isEditBookmarksEnabled();

        MenuItem bookmarkMenuItemShortcut = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, mTab, /*fromCCT=*/true);
        verify(bookmarkMenuItemShortcut).setEnabled(true);
    }

    @Test
    public void updateBookmarkMenuItemShortcut_NullTab() {
        MenuItem bookmarkMenuItemShortcut = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, null, /*fromCCT=*/false);
        verify(bookmarkMenuItemShortcut).setEnabled(false);
    }

    @Test
    public void updateBookmarkMenuItemShortcut_NullBookmarkBridge() {
        mBookmarkBridgeSupplier.set(null);

        MenuItem bookmarkMenuItemShortcut = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, mTab, /*fromCCT=*/false);
        verify(bookmarkMenuItemShortcut).setEnabled(false);
    }

    @Test
    public void updateBookmarkMenuItemRow() {
        setBookmarkItemRowEnabled(true);
        doReturn(true).when(mBookmarkBridge).isEditBookmarksEnabled();

        MenuItem bookmarkMenuItemAdd = Mockito.mock(MenuItem.class);
        MenuItem bookmarkMenuItemEdit = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemRow(
                bookmarkMenuItemAdd, bookmarkMenuItemEdit, mTab);
        verify(bookmarkMenuItemAdd).setVisible(true);
        verify(bookmarkMenuItemAdd).setEnabled(true);
    }

    @Test
    public void updateBookmarkMenuItemRow_WithShoppingAndReadingList() {
        setBookmarkItemRowEnabled(true);
        setShoppingListItemRowEnabled(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        setReadingListItemRowEnabled(true);
        doReturn(true).when(mBookmarkBridge).isEditBookmarksEnabled();

        MenuItem bookmarkMenuItemAdd = Mockito.mock(MenuItem.class);
        MenuItem bookmarkMenuItemEdit = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemRow(
                bookmarkMenuItemAdd, bookmarkMenuItemEdit, mTab);
        verify(bookmarkMenuItemAdd).setVisible(true);
        verify(bookmarkMenuItemAdd).setEnabled(true);

        MenuItem startPriceTrackingMenuItem = Mockito.mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        // TODO(crbug.com/1264685): Add menu visibility asserts here once the code is checked in.

        // Price-tracking takes priority and the reading list option won't be shown.
        MenuItem readingListMenuItemAdd = Mockito.mock(MenuItem.class);
        MenuItem readingListMenuItemDelete = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateReadingListMenuItemRow(
                readingListMenuItemAdd, readingListMenuItemDelete, mTab);
        verify(readingListMenuItemAdd).setVisible(false);
        verify(readingListMenuItemDelete).setVisible(false);

        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
    }

    @Test
    public void updateBookmarkMenuItemRow_NullTab() {
        setBookmarkItemRowEnabled(true);

        MenuItem bookmarkMenuItemAdd = Mockito.mock(MenuItem.class);
        MenuItem bookmarkMenuItemEdit = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemRow(
                bookmarkMenuItemAdd, bookmarkMenuItemEdit, null);
        verify(bookmarkMenuItemAdd).setVisible(false);
        verify(bookmarkMenuItemEdit).setVisible(false);
    }

    @Test
    public void updateBookmarkMenuItemRow_NullBookmarkBridge() {
        setBookmarkItemRowEnabled(true);
        mBookmarkBridgeSupplier.set(null);

        MenuItem bookmarkMenuItemAdd = Mockito.mock(MenuItem.class);
        MenuItem bookmarkMenuItemEdit = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemRow(
                bookmarkMenuItemAdd, bookmarkMenuItemEdit, null);
        verify(bookmarkMenuItemAdd).setVisible(false);
        verify(bookmarkMenuItemEdit).setVisible(false);
    }

    @Test
    public void updateReadingListMenuItemRow() {
        setReadingListItemRowEnabled(true);
        doReturn(true).when(mBookmarkBridge).isEditBookmarksEnabled();

        MenuItem readingListMenuItemAdd = Mockito.mock(MenuItem.class);
        MenuItem readingListMenuItemDelete = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateReadingListMenuItemRow(
                readingListMenuItemAdd, readingListMenuItemDelete, mTab);
        verify(readingListMenuItemAdd).setVisible(true);
        verify(readingListMenuItemDelete).setEnabled(true);
    }

    @Test
    public void updateReadingListMenuItemRow_NullTab() {
        setReadingListItemRowEnabled(true);

        MenuItem readingListMenuItemAdd = Mockito.mock(MenuItem.class);
        MenuItem readingListMenuItemDelete = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateReadingListMenuItemRow(
                readingListMenuItemAdd, readingListMenuItemDelete, null);
        verify(readingListMenuItemAdd).setVisible(false);
        verify(readingListMenuItemDelete).setVisible(false);
    }

    @Test
    public void updateReadingListMenuItemRow_NullBookmarkBridge() {
        setReadingListItemRowEnabled(true);
        mBookmarkBridgeSupplier.set(null);

        MenuItem readingListMenuItemAdd = Mockito.mock(MenuItem.class);
        MenuItem readingListMenuItemDelete = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateReadingListMenuItemRow(
                readingListMenuItemAdd, readingListMenuItemDelete, null);
        verify(readingListMenuItemAdd).setVisible(false);
        verify(readingListMenuItemDelete).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow() {
        setShoppingListItemRowEnabled(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        doReturn(true).when(mBookmarkBridge).isEditBookmarksEnabled();

        doReturn(Mockito.mock(BookmarkId.class))
                .when(mBookmarkBridge)
                .getUserBookmarkIdForTab(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setType(PowerBookmarkType.SHOPPING)
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setIsPriceTracked(false).build())
                        .build();
        doReturn(meta).when(mBookmarkBridge).getPowerBookmarkMeta(any());

        MenuItem startPriceTrackingMenuItem = Mockito.mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(startPriceTrackingMenuItem).setVisible(true);
        verify(startPriceTrackingMenuItem).setEnabled(true);
        verify(stopPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_NullBookmarkId() {
        setShoppingListItemRowEnabled(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        doReturn(true).when(mBookmarkBridge).isEditBookmarksEnabled();

        doReturn(null).when(mBookmarkBridge).getUserBookmarkIdForTab(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setType(PowerBookmarkType.SHOPPING)
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setIsPriceTracked(false).build())
                        .build();
        doReturn(meta).when(mBookmarkBridge).getPowerBookmarkMeta(any());

        MenuItem startPriceTrackingMenuItem = Mockito.mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(startPriceTrackingMenuItem).setVisible(true);
        verify(startPriceTrackingMenuItem).setEnabled(true);
        verify(stopPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_BadType() {
        setShoppingListItemRowEnabled(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        doReturn(true).when(mBookmarkBridge).isEditBookmarksEnabled();

        doReturn(Mockito.mock(BookmarkId.class))
                .when(mBookmarkBridge)
                .getUserBookmarkIdForTab(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setType(PowerBookmarkType.UNSPECIFIED)
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setIsPriceTracked(false).build())
                        .build();
        doReturn(meta).when(mBookmarkBridge).getPowerBookmarkMeta(any());

        MenuItem startPriceTrackingMenuItem = Mockito.mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(startPriceTrackingMenuItem).setVisible(false);
        verify(stopPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_PriceTrackingEnabled() {
        setShoppingListItemRowEnabled(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        doReturn(true).when(mBookmarkBridge).isEditBookmarksEnabled();

        BookmarkId bookmarkId = Mockito.mock(BookmarkId.class);
        List<BookmarkId> allBookmarks = new ArrayList<>();
        allBookmarks.add(bookmarkId);
        doReturn(bookmarkId).when(mBookmarkBridge).getUserBookmarkIdForTab(any());
        doReturn(allBookmarks)
                .when(mBookmarkBridge)
                .getBookmarksOfType(eq(PowerBookmarkType.SHOPPING));
        Long clusterId = 1L;
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setType(PowerBookmarkType.SHOPPING)
                        .setShoppingSpecifics(ShoppingSpecifics.newBuilder()
                                                      .setIsPriceTracked(true)
                                                      .setProductClusterId(clusterId)
                                                      .build())
                        .build();
        PowerBookmarkUtils.setPowerBookmarkMetaForTesting(meta);
        doReturn(meta).when(mBookmarkBridge).getPowerBookmarkMeta(any());

        MenuItem startPriceTrackingMenuItem = Mockito.mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = Mockito.mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(stopPriceTrackingMenuItem).setVisible(true);
        verify(stopPriceTrackingMenuItem).setEnabled(true);
        verify(startPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void shouldCheckBookmarkStar() {
        doReturn(true).when(mBookmarkBridge).hasBookmarkIdForTab(mTab);
        Assert.assertTrue(mAppMenuPropertiesDelegate.shouldCheckBookmarkStar(mTab));
    }

    @Test
    public void shouldCheckBookmarkStar_NullBookmarkBridge() {
        mBookmarkBridgeSupplier.set(null);
        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldCheckBookmarkStar(mTab));
    }

    @Test
    public void managedByMenuItem_ChromeManagementPage() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions().withShowAddToHomeScreen());
        doReturn(true).when(mAppMenuPropertiesDelegate).shouldShowManagedByMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        MenuItem managedByMenuItem = menu.findItem(R.id.managed_by_menu_id);

        Assert.assertNotNull(managedByMenuItem);
        Assert.assertTrue(managedByMenuItem.isVisible());
    }

    private void setUpMocksForPageMenu() {
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldCheckBookmarkStar(any(Tab.class));
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldEnableDownloadPage(any(Tab.class));
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldShowReaderModePrefs(any(Tab.class));
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowManagedByMenuItem(any(Tab.class));
        doReturn(true).when(mAppMenuPropertiesDelegate).isAutoDarkWebContentsEnabled();
        setUpIncognitoMocks();
    }

    private void setUpMocksForOverviewMenu() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
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

    private String getMenuTitles(Menu menu) {
        StringBuilder items = new StringBuilder();
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).isVisible()) {
                items.append("\n").append(menu.getItem(i).getTitle());
            }
        }
        return items.toString();
    }

    /** Options for tests that control how Menu is being rendered. */
    private static class MenuOptions {
        private boolean mIsNativePage;
        private boolean mShowTranslate;
        private boolean mShowUpdate;
        private boolean mShowMoveToOtherWindow;
        private boolean mShowReaderModePrefs;
        private boolean mShowAddToHomeScreen;
        private boolean mShowPaintPreview;
        private boolean mIsAutoDarkEnabled;

        protected boolean isNativePage() {
            return mIsNativePage;
        }

        protected boolean showTranslate() {
            return mShowTranslate;
        }

        protected boolean showUpdate() {
            return mShowUpdate;
        }

        protected boolean showMoveToOtherWindow() {
            return mShowMoveToOtherWindow;
        }

        protected boolean showReaderModePrefs() {
            return mShowReaderModePrefs;
        }

        protected boolean showAddToHomeScreen() {
            return mShowAddToHomeScreen;
        }

        protected boolean showPaintPreview() {
            return mShowPaintPreview;
        }

        protected boolean isAutoDarkEnabled() {
            return mIsAutoDarkEnabled;
        }

        protected MenuOptions setNativePage(boolean state) {
            mIsNativePage = state;
            return this;
        }

        protected MenuOptions setShowTranslate(boolean state) {
            mShowTranslate = state;
            return this;
        }

        protected MenuOptions setShowUpdate(boolean state) {
            mShowUpdate = state;
            return this;
        }

        protected MenuOptions setShowMoveToOtherWindow(boolean state) {
            mShowMoveToOtherWindow = state;
            return this;
        }

        protected MenuOptions setShowReaderModePrefs(boolean state) {
            mShowReaderModePrefs = state;
            return this;
        }

        protected MenuOptions setShowAddToHomeScreen(boolean state) {
            mShowAddToHomeScreen = state;
            return this;
        }

        protected MenuOptions setShowPaintPreview(boolean state) {
            mShowPaintPreview = state;
            return this;
        }

        protected MenuOptions setAutoDarkEnabled(boolean state) {
            mIsAutoDarkEnabled = state;
            return this;
        }

        protected MenuOptions withNativePage() {
            return setNativePage(true);
        }

        protected MenuOptions withShowTranslate() {
            return setShowTranslate(true);
        }

        protected MenuOptions withShowUpdate() {
            return setShowUpdate(true);
        }

        protected MenuOptions withShowMoveToOtherWindow() {
            return setShowMoveToOtherWindow(true);
        }

        protected MenuOptions withShowReaderModePrefs() {
            return setShowReaderModePrefs(true);
        }

        protected MenuOptions withShowAddToHomeScreen() {
            return setShowAddToHomeScreen(true);
        }

        protected MenuOptions withShowPaintPreview() {
            return setShowPaintPreview(true);
        }

        protected MenuOptions withAutoDarkEnabled() {
            return setAutoDarkEnabled(true);
        }

        protected MenuOptions withAllSet() {
            return this.withNativePage()
                    .withShowTranslate()
                    .withShowUpdate()
                    .withShowReaderModePrefs()
                    .withShowAddToHomeScreen()
                    .withShowPaintPreview()
                    .withAutoDarkEnabled();
        }
    };

    private void setMenuOptions(MenuOptions options) {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL));
        when(mTab.isNativePage()).thenReturn(options.isNativePage());
        doReturn(options.showTranslate())
                .when(mAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));
        doReturn(options.showUpdate()).when(mAppMenuPropertiesDelegate).shouldShowUpdateMenuItem();
        doReturn(options.showMoveToOtherWindow())
                .when(mAppMenuPropertiesDelegate)
                .shouldShowMoveToOtherWindow();
        doReturn(options.showReaderModePrefs())
                .when(mAppMenuPropertiesDelegate)
                .shouldShowReaderModePrefs(any(Tab.class));
        if (options.showAddToHomeScreen()) {
            doReturn(new AppBannerManager.InstallStringPair(
                             R.string.menu_add_to_homescreen, R.string.add))
                    .when(mAppMenuPropertiesDelegate)
                    .getAddToHomeScreenTitle(mTab);
        }
        doReturn(options.showPaintPreview())
                .when(mAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class), anyBoolean());
        when(mWebsitePreferenceBridgeJniMock.getContentSetting(any(), anyInt(), any(), any()))
                .thenReturn(options.isAutoDarkEnabled() ? ContentSettingValues.DEFAULT
                                                        : ContentSettingValues.BLOCK);
    }

    private void verifyManagedByMenuItem(boolean chromeManagementPageEnabled) {}
}
