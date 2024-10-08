// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.PackageManager;
import android.view.ContextThemeWrapper;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.widget.PopupMenu;

import androidx.annotation.NonNull;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl.MenuGroup;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.device.ShadowDeviceConditions;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtilsJni;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.menu_button.MenuUiState;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.accessibility.PageZoomCoordinator;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.ConnectionType;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Optional;

/** Unit tests for {@link AppMenuPropertiesDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class AppMenuPropertiesDelegateUnitTest {

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private NavigationController mNavigationController;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private View mDecorView;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabModelFilter mTabModelFilter;
    @Mock public WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock public BookmarkModel mBookmarkModel;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock private ManagedBrowserUtils.Natives mManagedBrowserUtilsJniMock;
    @Mock private IncognitoReauthController mIncognitoReauthControllerMock;
    @Mock private IncognitoUtils.Natives mIncognitoUtilsJniMock;
    @Mock private ShoppingService mShoppingService;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private AppBannerManager.Natives mAppBannerManagerJniMock;
    @Mock private ReadAloudController mReadAloudController;
    @Mock private TranslateBridge.Natives mTranslateBridgeJniMock;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private AppMenuPropertiesDelegate.CustomItemViewTypeProvider mCustomItemViewTypeProvider;
    @Mock private NativePage mNativePage;
    private OneshotSupplierImpl<IncognitoReauthController> mIncognitoReauthControllerSupplier =
            new OneshotSupplierImpl<>();
    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private ObservableSupplierImpl<BookmarkModel> mBookmarkModelSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<ReadAloudController> mReadAloudControllerSupplier =
            new ObservableSupplierImpl<>();

    private final TestValues mTestValues = new TestValues();
    private AppMenuPropertiesDelegateImpl mAppMenuPropertiesDelegate;
    private MenuUiState mMenuUiState;
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        setupFeatureDefaults();

        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mShadowPackageManager = Shadows.shadowOf(context.getPackageManager());

        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        mIncognitoReauthControllerSupplier.set(mIncognitoReauthControllerMock);
        mReadAloudControllerSupplier.set(mReadAloudController);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getCurrentTabModelFilter()).thenReturn(mTabModelFilter);
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        PageZoomCoordinator.setShouldShowMenuItemForTesting(false);

        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
        mMenuUiState = new MenuUiState();
        doReturn(mMenuUiState).when(mUpdateMenuItemHelper).getUiState();

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        Mockito.when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        WebappRegistry.refreshSharedPrefsForTesting();

        mJniMocker.mock(ManagedBrowserUtilsJni.TEST_HOOKS, mManagedBrowserUtilsJniMock);
        Mockito.when(mManagedBrowserUtilsJniMock.isBrowserManaged(mProfile)).thenReturn(false);
        Mockito.when(mManagedBrowserUtilsJniMock.getTitle(mProfile)).thenReturn("title");

        mJniMocker.mock(AppBannerManagerJni.TEST_HOOKS, mAppBannerManagerJniMock);
        Mockito.when(mAppBannerManagerJniMock.getInstallableWebAppManifestId(any()))
                .thenReturn(null);

        mJniMocker.mock(TranslateBridgeJni.TEST_HOOKS, mTranslateBridgeJniMock);
        Mockito.when(mTranslateBridgeJniMock.canManuallyTranslate(any(), anyBoolean()))
                .thenReturn(false);

        mJniMocker.mock(IncognitoUtilsJni.TEST_HOOKS, mIncognitoUtilsJniMock);

        mBookmarkModelSupplier.set(mBookmarkModel);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        PowerBookmarkUtils.setPowerBookmarkMetaForTesting(PowerBookmarkMeta.newBuilder().build());
        mAppMenuPropertiesDelegate =
                Mockito.spy(
                        new AppMenuPropertiesDelegateImpl(
                                context,
                                mActivityTabProvider,
                                mMultiWindowModeStateDispatcher,
                                mTabModelSelector,
                                mToolbarManager,
                                mDecorView,
                                mLayoutStateProviderSupplier,
                                mBookmarkModelSupplier,
                                mIncognitoReauthControllerSupplier,
                                mReadAloudControllerSupplier));

        mJniMocker.mock(CommerceFeatureUtilsJni.TEST_HOOKS, mCommerceFeatureUtilsJniMock);
        mJniMocker.mock(ShoppingServiceFactoryJni.TEST_HOOKS, mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());
        BuildConfig.IS_DESKTOP_ANDROID = false;
        ResettersForTesting.register(() -> BuildConfig.IS_DESKTOP_ANDROID = false);
    }

    @After
    public void tearDown() {
        AccessibilityState.setIsScreenReaderEnabledForTesting(false);
    }

    private void setupFeatureDefaults() {
        setShoppingListEligible(false);
        FeatureList.setTestValues(mTestValues);
    }

    private void setShoppingListEligible(boolean enabled) {
        doReturn(enabled).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());
        FeatureList.setTestValues(mTestValues);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShouldShowPageMenu_Phone() {
        setUpMocksForPageMenu();
        assertTrue(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowPageMenu_Tablet() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        when(mTabModel.getCount()).thenReturn(1);
        assertTrue(mAppMenuPropertiesDelegate.shouldShowPageMenu());
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
        assertTrue(mAppMenuPropertiesDelegate.shouldShowIconRow());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowIconRow_Tablet() {
        when(mDecorView.getWidth())
                .thenReturn(
                        (int)
                                (600
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
                .thenReturn(
                        (int)
                                (100
                                        * ContextUtils.getApplicationContext()
                                                .getResources()
                                                .getDisplayMetrics()
                                                .density));
        assertTrue(mAppMenuPropertiesDelegate.shouldShowIconRow());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_Ntp() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isNativePage()).thenReturn(true);
        when(mNativePage.isPdf()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.icon_row_menu_id,
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.divider_line_id,
            R.id.open_history_menu_id,
            R.id.quick_delete_menu_id,
            R.id.quick_delete_divider_line_id,
            R.id.downloads_menu_id,
            R.id.all_bookmarks_menu_id,
            R.id.recent_tabs_menu_id,
            R.id.divider_line_id,
            R.id.preferences_id,
            R.id.help_id
        };
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_Pdf() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1_WITH_PDF_PATH);
        when(mTab.isNativePage()).thenReturn(true);
        when(mNativePage.isPdf()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.icon_row_menu_id,
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.divider_line_id,
            R.id.open_history_menu_id,
            R.id.quick_delete_menu_id,
            R.id.quick_delete_divider_line_id,
            R.id.downloads_menu_id,
            R.id.all_bookmarks_menu_id,
            R.id.recent_tabs_menu_id,
            R.id.divider_line_id,
            R.id.share_row_menu_id,
            R.id.find_in_page_id,
            R.id.divider_line_id,
            R.id.preferences_id,
            R.id.help_id
        };
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage() {
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.icon_row_menu_id,
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.divider_line_id,
            R.id.open_history_menu_id,
            R.id.quick_delete_menu_id,
            R.id.quick_delete_divider_line_id,
            R.id.downloads_menu_id,
            R.id.all_bookmarks_menu_id,
            R.id.recent_tabs_menu_id,
            R.id.divider_line_id,
            R.id.share_row_menu_id,
            R.id.find_in_page_id,
            R.id.translate_id,
            R.id.universal_install,
            R.id.request_desktop_site_row_menu_id,
            R.id.auto_dark_web_contents_row_menu_id,
            R.id.divider_line_id,
            R.id.preferences_id,
            R.id.help_id
        };
        Integer[] expectedTitles = {
            0,
            R.string.menu_new_tab,
            R.string.menu_new_incognito_tab,
            0,
            R.string.menu_history,
            R.string.menu_quick_delete,
            0,
            R.string.menu_downloads,
            R.string.menu_bookmarks,
            R.string.menu_recent_tabs,
            0,
            0,
            R.string.menu_find_in_page,
            R.string.menu_translate,
            R.string.menu_add_to_homescreen,
            0,
            0,
            0,
            R.string.menu_settings,
            R.string.menu_help
        };
        Integer[] expectedActionBarItems = {
            R.id.forward_menu_id,
            R.id.bookmark_this_page_id,
            R.id.offline_page_id,
            R.id.info_menu_id,
            R.id.reload_menu_id
        };
        assertMenuItemsAreEqual(menu, expectedItems);
        assertMenuTitlesAreEqual(menu, expectedTitles);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_WithPwa() {
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.icon_row_menu_id,
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.divider_line_id,
            R.id.open_history_menu_id,
            R.id.quick_delete_menu_id,
            R.id.quick_delete_divider_line_id,
            R.id.downloads_menu_id,
            R.id.all_bookmarks_menu_id,
            R.id.recent_tabs_menu_id,
            R.id.divider_line_id,
            R.id.translate_id,
            R.id.share_row_menu_id,
            R.id.find_in_page_id,
            R.id.universal_install,
            R.id.request_desktop_site_row_menu_id,
            R.id.auto_dark_web_contents_row_menu_id,
            R.id.divider_line_id,
            R.id.preferences_id,
            R.id.help_id
        };
        Integer[] expectedTitles = {
            0,
            R.string.menu_new_tab,
            R.string.menu_new_incognito_tab,
            0,
            R.string.menu_history,
            R.string.menu_quick_delete,
            0,
            R.string.menu_downloads,
            R.string.menu_bookmarks,
            R.string.menu_recent_tabs,
            0,
            0,
            R.string.menu_find_in_page,
            R.string.menu_translate,
            R.string.menu_add_to_homescreen,
            0,
            0,
            0,
            R.string.menu_settings,
            R.string.menu_help
        };
        Integer[] expectedActionBarItems = {
            R.id.forward_menu_id,
            R.id.bookmark_this_page_id,
            R.id.offline_page_id,
            R.id.info_menu_id,
            R.id.reload_menu_id
        };
        assertMenuItemsAreEqual(menu, expectedItems);
        assertMenuTitlesAreEqual(menu, expectedTitles);
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_enterprise_user() {
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());
        doReturn(true).when(mAppMenuPropertiesDelegate).shouldShowManagedByMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.icon_row_menu_id,
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.divider_line_id,
            R.id.open_history_menu_id,
            R.id.quick_delete_menu_id,
            R.id.quick_delete_divider_line_id,
            R.id.downloads_menu_id,
            R.id.all_bookmarks_menu_id,
            R.id.recent_tabs_menu_id,
            R.id.divider_line_id,
            R.id.share_row_menu_id,
            R.id.find_in_page_id,
            R.id.translate_id,
            R.id.universal_install,
            R.id.request_desktop_site_row_menu_id,
            R.id.auto_dark_web_contents_row_menu_id,
            R.id.divider_line_id,
            R.id.preferences_id,
            R.id.help_id,
            R.id.managed_by_divider_line_id,
            R.id.managed_by_menu_id
        };
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_DesktopAndroid() {
        BuildConfig.IS_DESKTOP_ANDROID = true;
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.icon_row_menu_id,
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.divider_line_id,
            R.id.open_history_menu_id,
            R.id.quick_delete_menu_id,
            R.id.quick_delete_divider_line_id,
            R.id.downloads_menu_id,
            R.id.all_bookmarks_menu_id,
            R.id.recent_tabs_menu_id,
            R.id.divider_line_id,
            R.id.share_row_menu_id,
            R.id.find_in_page_id,
            R.id.translate_id,
            R.id.universal_install,
            // Request desktop site is hidden.
            R.id.auto_dark_web_contents_row_menu_id,
            R.id.divider_line_id,
            R.id.preferences_id,
            R.id.help_id
        };
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
        setMenuOptions(
                new MenuOptions()
                        .withAllSet()
                        .setNativePage(false)
                        .setShowMoveToOtherWindow(false)
                        .setShowPaintPreview(false));
        doReturn(true).when(mAppMenuPropertiesDelegate).shouldShowIconBeforeItem();

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.update_menu_id,
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.open_history_menu_id,
            R.id.quick_delete_menu_id,
            R.id.downloads_menu_id,
            R.id.all_bookmarks_menu_id,
            R.id.recent_tabs_menu_id,
            R.id.translate_id,
            R.id.find_in_page_id,
            R.id.universal_install,
            R.id.reader_mode_prefs_id,
            R.id.preferences_id,
            R.id.help_id
        };
        assertMenuItemsHaveIcons(menu, expectedItems);
    }

    private void checkOverviewMenuItemsPhone(int tabSelectionEditorMenuItemId) {
        setUpMocksForOverviewMenu(LayoutType.TAB_SWITCHER);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(
                MenuGroup.OVERVIEW_MODE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.close_all_tabs_menu_id,
            tabSelectionEditorMenuItemId,
            R.id.quick_delete_menu_id,
            R.id.preferences_id
        };
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testOverviewMenuItems_Phone_SelectTabs() {
        checkOverviewMenuItemsPhone(R.id.menu_select_tabs);
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
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.preferences_id,
            R.id.quick_delete_menu_id
        };
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    public void testMenuItems_Accessibility_ImageDescriptions() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        when(mTab.isNativePage()).thenReturn(false);
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class), anyBoolean());
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));

        // Ensure the get image descriptions option is shown as needed
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);

        // Test specific setup
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        AccessibilityState.setIsScreenReaderEnabledForTesting(true);

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.icon_row_menu_id,
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.divider_line_id,
            R.id.open_history_menu_id,
            R.id.quick_delete_menu_id,
            R.id.quick_delete_divider_line_id,
            R.id.downloads_menu_id,
            R.id.all_bookmarks_menu_id,
            R.id.recent_tabs_menu_id,
            R.id.divider_line_id,
            R.id.share_row_menu_id,
            R.id.get_image_descriptions_id,
            R.id.find_in_page_id,
            R.id.universal_install,
            R.id.request_desktop_site_row_menu_id,
            R.id.auto_dark_web_contents_row_menu_id,
            R.id.divider_line_id,
            R.id.preferences_id,
            R.id.help_id
        };

        assertMenuItemsAreEqual(menu, expectedItems);

        // Ensure the text of the menu item is correct
        Assert.assertEquals(
                "Get image descriptions", menu.findItem(R.id.get_image_descriptions_id).getTitle());

        // Enable the feature and ensure text changes
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(true);

        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        Assert.assertEquals(
                "Stop image descriptions",
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
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        MenuItem bookmarkMenuItemShortcut = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, mTab, /* fromCCT= */ false);
        verify(bookmarkMenuItemShortcut).setEnabled(true);
    }

    @Test
    public void updateBookmarkMenuItemShortcut_fromCCT() {
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        MenuItem bookmarkMenuItemShortcut = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, mTab, /* fromCCT= */ true);
        verify(bookmarkMenuItemShortcut).setEnabled(true);
    }

    @Test
    public void updateBookmarkMenuItemShortcut_NullTab() {
        MenuItem bookmarkMenuItemShortcut = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, null, /* fromCCT= */ false);
        verify(bookmarkMenuItemShortcut).setEnabled(false);
    }

    @Test
    public void updateBookmarkMenuItemShortcut_NullBookmarkModel() {
        mBookmarkModelSupplier.set(null);

        MenuItem bookmarkMenuItemShortcut = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, mTab, /* fromCCT= */ false);
        verify(bookmarkMenuItemShortcut).setEnabled(false);
    }

    @Test
    public void enablePriceTrackingItemRow() {
        setShoppingListEligible(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        doReturn(mock(BookmarkId.class)).when(mBookmarkModel).getUserBookmarkIdForTab(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setIsPriceTracked(false).build())
                        .build();
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(any());

        MenuItem startPriceTrackingMenuItem = mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(startPriceTrackingMenuItem).setVisible(true);
        verify(startPriceTrackingMenuItem).setEnabled(true);
        verify(stopPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_NullBookmarkModel() {
        setShoppingListEligible(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        mBookmarkModelSupplier.set(null);

        MenuItem startPriceTrackingMenuItem = mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(startPriceTrackingMenuItem).setVisible(false);
        verify(stopPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_NullBookmarkId() {
        setShoppingListEligible(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        doReturn(null).when(mBookmarkModel).getUserBookmarkIdForTab(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setIsPriceTracked(false).build())
                        .build();
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(any());

        MenuItem startPriceTrackingMenuItem = mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(startPriceTrackingMenuItem).setVisible(true);
        verify(startPriceTrackingMenuItem).setEnabled(true);
        verify(stopPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_PriceTrackingEnabled() {
        setShoppingListEligible(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        BookmarkId bookmarkId = mock(BookmarkId.class);
        List<BookmarkId> allBookmarks = new ArrayList<>();
        allBookmarks.add(bookmarkId);
        doReturn(bookmarkId).when(mBookmarkModel).getUserBookmarkIdForTab(any());
        doReturn(allBookmarks)
                .when(mBookmarkModel)
                .getBookmarksOfType(eq(PowerBookmarkType.SHOPPING));
        Long clusterId = 1L;
        doReturn(
                        new ShoppingService.ProductInfo(
                                "",
                                new GURL(""),
                                Optional.of(clusterId),
                                Optional.empty(),
                                "",
                                0,
                                "",
                                Optional.empty()))
                .when(mShoppingService)
                .getAvailableProductInfoForUrl(any());
        doReturn(true).when(mShoppingService).isSubscribedFromCache(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder()
                                        .setIsPriceTracked(true)
                                        .setProductClusterId(clusterId)
                                        .build())
                        .build();
        PowerBookmarkUtils.setPowerBookmarkMetaForTesting(meta);
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(any());

        MenuItem startPriceTrackingMenuItem = mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(stopPriceTrackingMenuItem).setVisible(true);
        verify(stopPriceTrackingMenuItem).setEnabled(true);
        verify(startPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_PriceTrackingEnabled_NoProductInfo() {
        setShoppingListEligible(true);

        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        BookmarkId bookmarkId = mock(BookmarkId.class);
        doReturn(bookmarkId).when(mBookmarkModel).getUserBookmarkIdForTab(any());
        doReturn(new ArrayList<BookmarkId>())
                .when(mBookmarkModel)
                .getBookmarksOfType(eq(PowerBookmarkType.SHOPPING));

        MenuItem startPriceTrackingMenuItem = mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(stopPriceTrackingMenuItem).setVisible(false);
        verify(startPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void shouldCheckBookmarkStar() {
        doReturn(true).when(mBookmarkModel).hasBookmarkIdForTab(mTab);
        assertTrue(mAppMenuPropertiesDelegate.shouldCheckBookmarkStar(mTab));
    }

    @Test
    public void shouldCheckBookmarkStar_NullBookmarkModel() {
        mBookmarkModelSupplier.set(null);
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
        assertTrue(managedByMenuItem.isVisible());
    }

    @Test
    @SmallTest
    public void testNewIncognitoTabOption_WithReauthInProgress() {
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());

        doReturn(true).when(mIncognitoReauthControllerMock).isReauthPageShowing();
        doReturn(mIncognitoTabModel).when(mTabModelSelector).getCurrentModel();

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        verify(mIncognitoReauthControllerMock, times(1)).isReauthPageShowing();

        MenuItem item = menu.findItem(R.id.new_incognito_tab_menu_id);
        assertFalse(item.isEnabled());
    }

    @Test
    @SmallTest
    public void testNewIncognitoTabOption_FromRegularMode_WithReauthNotInProgress() {
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());

        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        verifyNoMoreInteractions(mIncognitoReauthControllerMock);

        MenuItem item = menu.findItem(R.id.new_incognito_tab_menu_id);
        assertTrue(item.isEnabled());
    }

    private Menu setUpMenuWithIncognitoReauthPage(boolean isShowing) {
        setUpMocksForOverviewMenu(LayoutType.TAB_SWITCHER);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        prepareMocksForGroupTabsOnTabModel(mIncognitoTabModel);
        doReturn(isShowing).when(mIncognitoReauthControllerMock).isReauthPageShowing();

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        verify(mIncognitoReauthControllerMock, times(1)).isReauthPageShowing();
        return menu;
    }

    @Test
    @SmallTest
    public void testSelectTabsOption_IsEnabled_InIncognitoMode_When_IncognitoReauthIsNotShowing() {
        Menu menu = setUpMenuWithIncognitoReauthPage(/* isShowing= */ false);
        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertTrue(item.isEnabled());
    }

    @Test
    @SmallTest
    public void testSelectTabsOption_IsDisabled_InIncognitoMode_When_IncognitoReauthIsShowing() {
        Menu menu = setUpMenuWithIncognitoReauthPage(/* isShowing= */ true);
        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertFalse(item.isEnabled());
    }

    @Test
    @SmallTest
    public void testSelectTabsOption_IsEnabled_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu(LayoutType.TAB_SWITCHER);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        prepareMocksForGroupTabsOnTabModel(mTabModel);

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertTrue(item.isEnabled());
    }

    @Test
    @SmallTest
    public void testSelectTabsOption_IsDisabled_InRegularMode_TabStateNotInitialized() {
        setUpMocksForOverviewMenu(LayoutType.TAB_SWITCHER);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        prepareMocksForGroupTabsOnTabModel(mTabModel);

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertFalse(item.isEnabled());
    }

    @Test
    @SmallTest
    public void testSelectTabsOption_IsEnabledOneTab_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu(LayoutType.TAB_SWITCHER);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModelFilter.getCount()).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(1);
        Tab mockTab1 = mock(Tab.class);
        when(mTabModel.getTabAt(0)).thenReturn(mockTab1);

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertTrue(item.isEnabled());
    }

    @Test
    @SmallTest
    public void testSelectTabsOption_IsDisabled_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu(LayoutType.TAB_SWITCHER);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertFalse(item.isEnabled());
    }

    @Test
    public void testShouldShowNewMenu_alreadyMaxWindows_returnsFalse() {
        assertFalse(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isInstanceSwitcherEnabled= */ true,
                        /* currentWindowInstances= */ 10,
                        /* isTabletSizeScreen= */ true,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ false));
        verify(mAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
    }

    @Test
    public void testShouldShowNewMenu_isAutomotive_returnsFalse() {
        assertFalse(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ true,
                        /* isInstanceSwitcherEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ true,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ false));
        verify(mAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
    }

    @Test
    public void testShouldShowNewMenu_instanceSwitcherDisabled_isAutomotive_returnsFalse() {
        assertFalse(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ true,
                        /* isInstanceSwitcherEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ true,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ true,
                        /* isMultiInstanceRunning= */ false));
        verify(mAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
    }

    @Test
    public void testShouldShowMoveToOtherWindow_returnsTrue() {
        assertTrue(
                doTestShouldShowMoveToOtherWindowMenu(
                        /* totalTabCount= */ 1,
                        /* isInstanceSwitcherEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ true,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ false,
                        /* isMoveToOtherWindowSupported= */ true));
    }

    @Test
    public void testShouldShowMoveToOtherWindow_dispatcherReturnsFalse_returnsFalse() {
        assertFalse(
                doTestShouldShowMoveToOtherWindowMenu(
                        /* totalTabCount= */ 1,
                        /* isInstanceSwitcherEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ true,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ true,
                        /* isMultiInstanceRunning= */ false,
                        /* isMoveToOtherWindowSupported= */ false));
        verify(mAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
    }

    @Test
    public void testShouldShowNewMenu_isTabletSizedScreen_returnsTrue() {
        assertTrue(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isInstanceSwitcherEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ true,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ false));
        verify(mAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
    }

    @Test
    public void testShouldShowNewMenu_chromeRunningInAdjacentWindow_returnsFalse() {
        assertFalse(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isInstanceSwitcherEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ true,
                        /* isInMultiWindowMode= */ true,
                        /* isInMultiDisplayMode= */ true,
                        /* isMultiInstanceRunning= */ false));
        verify(mAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
    }

    @Test
    public void testShouldShowNewMenu_multiWindowMode_returnsTrue() {
        assertTrue(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isInstanceSwitcherEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ true,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ false));
        verify(mAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
    }

    @Test
    public void testShouldShowNewMenu_multiDisplayMode_returnsTrue() {
        assertTrue(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isInstanceSwitcherEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ true,
                        /* isMultiInstanceRunning= */ false));
        verify(mAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
    }

    @Test
    public void testShouldShowNewMenu_multiInstanceRunning_returnsFalse() {
        assertFalse(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isInstanceSwitcherEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ true));
        verify(mAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
        verify(mAppMenuPropertiesDelegate, never()).getInstanceCount();
    }

    @Test
    public void testShouldShowNewMenu_canEnterMultiWindowMode_returnsTrue() {
        assertTrue(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isInstanceSwitcherEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ true,
                        /* canEnterMultiWindowMode= */ true,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ false));
        verify(mAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
        verify(mAppMenuPropertiesDelegate, never()).getInstanceCount();
    }

    @Test
    public void testShouldShowNewMenu_instanceSwitcherDisabled_multiWindowMode_returnsTrue() {
        assertTrue(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isInstanceSwitcherEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ true,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ false));
        verify(mAppMenuPropertiesDelegate, never()).getInstanceCount();
    }

    @Test
    public void testShouldShowNewMenu_instanceSwitcherDisabled_multiDisplayMode_returnsTrue() {
        assertTrue(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isInstanceSwitcherEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
                        /* canEnterMultiWindowMode= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ true,
                        /* isMultiInstanceRunning= */ false));
        verify(mAppMenuPropertiesDelegate, never()).getInstanceCount();
    }

    @Test
    public void testReadAloudMenuItem_readAloudNotEnabled() {
        mReadAloudControllerSupplier.set(null);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        setUpMocksForPageMenu();
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        assertFalse(menu.findItem(R.id.readaloud_menu_id).isVisible());
    }

    @Test
    public void testReadAloudMenuItem_notReadable() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mReadAloudController.isReadable(any())).thenReturn(false);
        setUpMocksForPageMenu();
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        assertFalse(menu.findItem(R.id.readaloud_menu_id).isVisible());
    }

    @Test
    public void testReadAloudMenuItem_readable() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mReadAloudController.isReadable(any())).thenReturn(true);
        setUpMocksForPageMenu();
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);
        assertTrue(menu.findItem(R.id.readaloud_menu_id).isVisible());
    }

    @Test
    public void testReadaloudMenuItem_readableBecomesUnreadable() {
        testReadAloudMenuItemUpdates(/* initiallyReadable= */ true, /* laterReadable= */ false);
    }

    @Test
    public void testReadaloudMenuItem_unreadableBecomesReadable() {
        testReadAloudMenuItemUpdates(/* initiallyReadable= */ false, /* laterReadable= */ true);
    }

    @Test
    public void testReadaloudMenuItem_noChangeInReadability_notReadable() {
        testReadAloudMenuItemUpdates(/* initiallyReadable= */ false, /* laterReadable= */ false);
    }

    @Test
    public void testReadaloudMenuItem_noChangeInReadability_readable() {
        testReadAloudMenuItemUpdates(/* initiallyReadable= */ true, /* laterReadable= */ true);
    }

    private void testReadAloudMenuItemUpdates(boolean initiallyReadable, boolean laterReadable) {
        AccessibilityState.setIsScreenReaderEnabledForTesting(false);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mReadAloudController.isReadable(mTab)).thenReturn(initiallyReadable);
        setUpMocksForPageMenu();
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.getMenuItemsForMenu(
                menu, mCustomItemViewTypeProvider, mAppMenuHandler);
        // When menu is created, the visibility should match readability state at that time
        assertEquals(initiallyReadable, hasReadAloudInMenu());

        when(mCustomItemViewTypeProvider.fromMenuItemId(anyInt()))
                .thenReturn(CustomViewBinder.NOT_HANDLED);
        when(mReadAloudController.isReadable(mTab)).thenReturn(laterReadable);
        // When a new readability result is retrieved, ensure that the menu item visibility matches
        // the current readability state.
        mAppMenuPropertiesDelegate.getReadAloudmenuResetter().run();
        assertEquals(laterReadable, hasReadAloudInMenu());
    }

    private boolean hasReadAloudInMenu() {
        ModelList modelList = mAppMenuPropertiesDelegate.getModelList();
        if (modelList == null) {
            return false;
        }
        Iterator<ListItem> it = modelList.iterator();
        while (it.hasNext()) {
            ListItem li = it.next();
            int id = li.model.get(AppMenuItemProperties.MENU_ITEM_ID);
            if (id == R.id.readaloud_menu_id) {
                return true;
            }
        }
        return false;
    }

    private boolean doTestShouldShowNewMenu(
            boolean isAutomotive,
            boolean isInstanceSwitcherEnabled,
            int currentWindowInstances,
            boolean isTabletSizeScreen,
            boolean canEnterMultiWindowMode,
            boolean isChromeRunningInAdjacentWindow,
            boolean isInMultiWindowMode,
            boolean isInMultiDisplayMode,
            boolean isMultiInstanceRunning) {
        mShadowPackageManager.setSystemFeature(PackageManager.FEATURE_AUTOMOTIVE, isAutomotive);
        doReturn(isInstanceSwitcherEnabled)
                .when(mAppMenuPropertiesDelegate)
                .instanceSwitcherWithMultiInstanceEnabled();
        doReturn(currentWindowInstances).when(mAppMenuPropertiesDelegate).getInstanceCount();
        doReturn(isTabletSizeScreen).when(mAppMenuPropertiesDelegate).isTabletSizeScreen();
        doReturn(canEnterMultiWindowMode)
                .when(mMultiWindowModeStateDispatcher)
                .canEnterMultiWindowMode();
        doReturn(isChromeRunningInAdjacentWindow)
                .when(mMultiWindowModeStateDispatcher)
                .isChromeRunningInAdjacentWindow();
        doReturn(isInMultiWindowMode).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        doReturn(isInMultiDisplayMode).when(mMultiWindowModeStateDispatcher).isInMultiDisplayMode();
        doReturn(isMultiInstanceRunning)
                .when(mMultiWindowModeStateDispatcher)
                .isMultiInstanceRunning();

        return mAppMenuPropertiesDelegate.shouldShowNewWindow();
    }

    private boolean doTestShouldShowMoveToOtherWindowMenu(
            int totalTabCount,
            boolean isInstanceSwitcherEnabled,
            int currentWindowInstances,
            boolean isTabletSizeScreen,
            boolean canEnterMultiWindowMode,
            boolean isChromeRunningInAdjacentWindow,
            boolean isInMultiWindowMode,
            boolean isInMultiDisplayMode,
            boolean isMultiInstanceRunning,
            boolean isMoveToOtherWindowSupported) {
        doReturn(isInstanceSwitcherEnabled)
                .when(mAppMenuPropertiesDelegate)
                .instanceSwitcherWithMultiInstanceEnabled();
        doReturn(currentWindowInstances).when(mAppMenuPropertiesDelegate).getInstanceCount();
        doReturn(isTabletSizeScreen).when(mAppMenuPropertiesDelegate).isTabletSizeScreen();
        doReturn(canEnterMultiWindowMode)
                .when(mMultiWindowModeStateDispatcher)
                .canEnterMultiWindowMode();
        doReturn(isChromeRunningInAdjacentWindow)
                .when(mMultiWindowModeStateDispatcher)
                .isChromeRunningInAdjacentWindow();
        doReturn(isInMultiWindowMode).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        doReturn(isInMultiDisplayMode).when(mMultiWindowModeStateDispatcher).isInMultiDisplayMode();
        doReturn(isMultiInstanceRunning)
                .when(mMultiWindowModeStateDispatcher)
                .isMultiInstanceRunning();
        doReturn(isMoveToOtherWindowSupported)
                .when(mMultiWindowModeStateDispatcher)
                .isMoveToOtherWindowSupported(any());

        return mAppMenuPropertiesDelegate.shouldShowMoveToOtherWindow();
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

    private void setUpMocksForOverviewMenu(@LayoutType int layoutType) {
        when(mLayoutStateProvider.isLayoutVisible(layoutType)).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        setUpIncognitoMocks();
    }

    private void setUpIncognitoMocks() {
        doReturn(true).when(mAppMenuPropertiesDelegate).isIncognitoEnabled();
        doReturn(false).when(mIncognitoReauthControllerMock).isIncognitoReauthPending();
        doReturn(false).when(mIncognitoReauthControllerMock).isReauthPageShowing();
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

        assertThat(
                "Populated menu items were:" + getMenuTitles(menu),
                actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private void assertMenuTitlesAreEqual(Menu menu, Integer... expectedTitles) {
        Context context = ContextUtils.getApplicationContext();
        int expectedIndex = 0;
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).isVisible()) {
                Assert.assertEquals(
                        expectedTitles[expectedIndex] == 0
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

        assertThat(
                "Populated action bar items were:" + getMenuTitles(actionBar),
                actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private void assertMenuItemsHaveIcons(Menu menu, Integer... expectedItems) {
        List<Integer> actualItems = new ArrayList<>();
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).isVisible() && menu.getItem(i).getIcon() != null) {
                actualItems.add(menu.getItem(i).getItemId());
            }
        }

        assertThat(
                "menu items with icons were:" + getMenuTitles(menu),
                actualItems,
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
    }

    private void setMenuOptions(MenuOptions options) {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
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
        doReturn(options.showPaintPreview())
                .when(mAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class), anyBoolean());
        when(mWebsitePreferenceBridgeJniMock.getContentSetting(any(), anyInt(), any(), any()))
                .thenReturn(
                        options.isAutoDarkEnabled()
                                ? ContentSettingValues.DEFAULT
                                : ContentSettingValues.BLOCK);
    }

    private void verifyManagedByMenuItem(boolean chromeManagementPageEnabled) {}

    /**
     * Preparation to mock the "final" method TabModelFilter#getTabsWithNoOtherRelatedTabs which
     * plays a part to enable group tabs.
     */
    private void prepareMocksForGroupTabsOnTabModel(@NonNull TabModel tabmodel) {
        when(mTabModelFilter.getTabModel()).thenReturn(tabmodel);
        when(mTabModelFilter.getCount()).thenReturn(2);
        when(tabmodel.getCount()).thenReturn(2);
        Tab mockTab1 = mock(Tab.class);
        Tab mockTab2 = mock(Tab.class);
        when(tabmodel.getTabAt(0)).thenReturn(mockTab1);
        when(tabmodel.getTabAt(1)).thenReturn(mockTab2);
    }
}
