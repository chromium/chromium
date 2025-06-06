// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
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

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.test.PausedExecutorTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ai.AiAssistantService;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl.MenuGroup;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.device.ShadowDeviceConditions;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtilsJni;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.accessibility.PageZoomCoordinator;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.ConnectionType;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;

/** Unit tests for {@link TabbedAppMenuPropertiesDelegate}. */
// TODO(crbug.com/376238770): Removes ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION from
// @DisableFeatures() and adds "Customize New Tab Page" to all expectedItems list once the feature
// flag is turned on by default.
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY,
    ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION,
    DomDistillerFeatures.READER_MODE_IMPROVEMENTS
})
public class TabbedAppMenuPropertiesDelegateUnitTest {
    // Constants defining flags that determines multi-window menu items visibility.
    private static final boolean TAB_M = true; // multiple tabs
    private static final boolean TAB_S = false;
    private static final boolean WIN_M = true; // in multi-window mode
    private static final boolean WIN_S = false;
    private static final boolean INST_M = true; // in multi-instance mode
    private static final boolean INST_S = false;
    private static final boolean TABLET = true;
    private static final boolean PHONE = false;
    private static final boolean API_YES = true; // multi-window API supported
    private static final boolean API_NO = false;
    private static final boolean MOVE_OTHER_YES =
            true; // multi-window move to other window supported
    private static final boolean MOVE_OTHER_NO = false;

    private static final boolean NEW_YES = true; // show 'new window'
    private static final boolean NEW_NO = false;
    private static final boolean MOVE_YES = true; // show 'move to other window'
    private static final boolean MOVE_NO = false;
    private static final Boolean ANY = null; // do not care

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public PausedExecutorTestRule mExecutorRule = new PausedExecutorTestRule();

    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private NativePage mNativePage;
    @Mock private NavigationController mNavigationController;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private View mDecorView;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private ManagedBrowserUtils.Natives mManagedBrowserUtilsJniMock;
    @Mock private Profile mProfile;
    @Mock private AppMenuDelegate mAppMenuDelegate;
    @Mock private WebFeedSnackbarController.FeedLauncher mFeedLauncher;
    @Mock private ModalDialogManager mDialogManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private OfflinePageUtils.Internal mOfflinePageUtils;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private IdentityServicesProvider mIdentityService;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private IncognitoUtils.Natives mIncognitoUtilsJniMock;
    @Mock public WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock private IncognitoReauthController mIncognitoReauthControllerMock;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingService mShoppingService;
    @Mock private AppBannerManager.Natives mAppBannerManagerJniMock;
    @Mock private ReadAloudController mReadAloudController;
    @Mock private UserPrefs.Natives mUserPrefsNatives;
    @Mock private PrefService mPrefService;
    @Mock private SyncService mSyncService;
    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private TranslateBridge.Natives mTranslateBridgeJniMock;

    private ShadowPackageManager mShadowPackageManager;

    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<IncognitoReauthController>
            mIncognitoReauthControllerSupplier = new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<BookmarkModel> mBookmarkModelSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<ReadAloudController> mReadAloudControllerSupplier =
            new ObservableSupplierImpl<>();

    private TabbedAppMenuPropertiesDelegate mTabbedAppMenuPropertiesDelegate;

    // Boolean flags to test multi-window menu visibility for various combinations.
    private boolean mIsMultiInstance;
    private boolean mIsMultiWindow;
    private boolean mIsTabletScreen;
    private boolean mIsMultiWindowApiSupported;
    private boolean mIsMoveToOtherWindowSupported;

    // Used to ensure all the combinations are tested.
    private final boolean[] mFlagCombinations = new boolean[1 << 5];

    @Before
    public void setUp() {
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
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getCurrentTabGroupModelFilter())
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        ManagedBrowserUtilsJni.setInstanceForTesting(mManagedBrowserUtilsJniMock);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        WebsitePreferenceBridgeJni.setInstanceForTesting(mWebsitePreferenceBridgeJniMock);
        OfflinePageUtils.setInstanceForTesting(mOfflinePageUtils);
        when(mIdentityService.getSigninManager(any(Profile.class))).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        PageZoomCoordinator.setShouldShowMenuItemForTesting(false);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        AppBannerManagerJni.setInstanceForTesting(mAppBannerManagerJniMock);
        Mockito.when(mAppBannerManagerJniMock.getInstallableWebAppManifestId(any()))
                .thenReturn(null);
        WebFeedBridgeJni.setInstanceForTesting(mWebFeedBridgeJniMock);
        when(mWebFeedBridgeJniMock.isWebFeedEnabled()).thenReturn(true);
        UserPrefsJni.setInstanceForTesting(mUserPrefsNatives);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);

        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        SyncServiceFactory.setInstanceForTesting(mSyncService);

        IncognitoUtilsJni.setInstanceForTesting(mIncognitoUtilsJniMock);

        TranslateBridgeJni.setInstanceForTesting(mTranslateBridgeJniMock);
        Mockito.when(mTranslateBridgeJniMock.canManuallyTranslate(any(), anyBoolean()))
                .thenReturn(false);

        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        PowerBookmarkUtils.setPowerBookmarkMetaForTesting(PowerBookmarkMeta.newBuilder().build());
        TabbedAppMenuPropertiesDelegate delegate =
                new TabbedAppMenuPropertiesDelegate(
                        context,
                        mActivityTabProvider,
                        mMultiWindowModeStateDispatcher,
                        mTabModelSelector,
                        mToolbarManager,
                        mDecorView,
                        mAppMenuDelegate,
                        mLayoutStateProviderSupplier,
                        mBookmarkModelSupplier,
                        mFeedLauncher,
                        mDialogManager,
                        mSnackbarManager,
                        mIncognitoReauthControllerSupplier,
                        mReadAloudControllerSupplier);
        mExecutorRule.runAllBackgroundAndUi();
        mTabbedAppMenuPropertiesDelegate = Mockito.spy(delegate);

        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_URL);
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT);

        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
    }

    @After
    public void tearDown() {
        AccessibilityState.setIsKnownScreenReaderEnabledForTesting(false);
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

    private void setUpMocksForOverviewMenu() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        setUpIncognitoMocks();
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
    }

    private void setUpIncognitoMocks() {
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isIncognitoEnabled();
        doReturn(false).when(mIncognitoReauthControllerMock).isIncognitoReauthPending();
        doReturn(false).when(mIncognitoReauthControllerMock).isReauthPageShowing();
    }

    /**
     * Preparation to mock the "final" method TabGroupModelFilter#getTabsWithNoOtherRelatedTabs
     * which plays a part to enable group tabs.
     */
    private void prepareMocksForGroupTabsOnTabModel(@NonNull TabModel tabmodel) {
        when(mTabGroupModelFilter.getTabModel()).thenReturn(tabmodel);
        when(tabmodel.getCount()).thenReturn(2);
        Tab mockTab1 = mock(Tab.class);
        Tab mockTab2 = mock(Tab.class);
        when(tabmodel.getTabAt(0)).thenReturn(mockTab1);
        when(tabmodel.getTabAt(1)).thenReturn(mockTab2);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShouldShowIconRow_Phone() {
        assertTrue(mTabbedAppMenuPropertiesDelegate.shouldShowIconRow());
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
        assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowIconRow());
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
        assertTrue(mTabbedAppMenuPropertiesDelegate.shouldShowIconRow());
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
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

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
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

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
            R.id.open_with_id,
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

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        List<Integer> expectedItems = new ArrayList<>();
        List<Integer> expectedTitles = new ArrayList<>();

        expectedItems.add(R.id.icon_row_menu_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.new_tab_menu_id);
        expectedTitles.add(R.string.menu_new_tab);
        expectedItems.add(R.id.new_incognito_tab_menu_id);
        expectedTitles.add(R.string.menu_new_incognito_tab);
        expectedItems.add(R.id.divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.open_history_menu_id);
        expectedTitles.add(R.string.menu_history);
        expectedItems.add(R.id.quick_delete_menu_id);
        expectedTitles.add(R.string.menu_quick_delete);
        expectedItems.add(R.id.quick_delete_divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.downloads_menu_id);
        expectedTitles.add(R.string.menu_downloads);
        expectedItems.add(R.id.all_bookmarks_menu_id);
        expectedTitles.add(R.string.menu_bookmarks);
        expectedItems.add(R.id.recent_tabs_menu_id);
        expectedTitles.add(R.string.menu_recent_tabs);
        expectedItems.add(R.id.divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.share_row_menu_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.find_in_page_id);
        expectedTitles.add(R.string.menu_find_in_page);
        expectedItems.add(R.id.translate_id);
        expectedTitles.add(R.string.menu_translate);
        expectedItems.add(R.id.universal_install);
        expectedTitles.add(R.string.menu_add_to_homescreen);
        if (!BuildConfig.IS_DESKTOP_ANDROID) {
            expectedItems.add(R.id.request_desktop_site_row_menu_id);
            expectedTitles.add(0);
        }
        expectedItems.add(R.id.auto_dark_web_contents_row_menu_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.preferences_id);
        expectedTitles.add(R.string.menu_settings);
        expectedItems.add(R.id.help_id);
        expectedTitles.add(R.string.menu_help);

        Integer[] expectedActionBarItems = {
            R.id.forward_menu_id,
            R.id.bookmark_this_page_id,
            R.id.offline_page_id,
            R.id.info_menu_id,
            R.id.reload_menu_id
        };
        assertMenuItemsAreEqual(menu, expectedItems.toArray(new Integer[0]));
        assertMenuTitlesAreEqual(menu, expectedTitles.toArray(new Integer[0]));
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

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        List<Integer> expectedItems = new ArrayList<>();
        List<Integer> expectedTitles = new ArrayList<>();

        expectedItems.add(R.id.icon_row_menu_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.new_tab_menu_id);
        expectedTitles.add(R.string.menu_new_tab);
        expectedItems.add(R.id.new_incognito_tab_menu_id);
        expectedTitles.add(R.string.menu_new_incognito_tab);
        expectedItems.add(R.id.divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.open_history_menu_id);
        expectedTitles.add(R.string.menu_history);
        expectedItems.add(R.id.quick_delete_menu_id);
        expectedTitles.add(R.string.menu_quick_delete);
        expectedItems.add(R.id.quick_delete_divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.downloads_menu_id);
        expectedTitles.add(R.string.menu_downloads);
        expectedItems.add(R.id.all_bookmarks_menu_id);
        expectedTitles.add(R.string.menu_bookmarks);
        expectedItems.add(R.id.recent_tabs_menu_id);
        expectedTitles.add(R.string.menu_recent_tabs);
        expectedItems.add(R.id.divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.share_row_menu_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.find_in_page_id);
        expectedTitles.add(R.string.menu_find_in_page);
        expectedItems.add(R.id.translate_id);
        expectedTitles.add(R.string.menu_translate);
        expectedItems.add(R.id.universal_install);
        expectedTitles.add(R.string.menu_add_to_homescreen);
        if (!BuildConfig.IS_DESKTOP_ANDROID) {
            expectedItems.add(R.id.request_desktop_site_row_menu_id);
            expectedTitles.add(0);
        }
        expectedItems.add(R.id.auto_dark_web_contents_row_menu_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.preferences_id);
        expectedTitles.add(R.string.menu_settings);
        expectedItems.add(R.id.help_id);
        expectedTitles.add(R.string.menu_help);

        Integer[] expectedActionBarItems = {
            R.id.forward_menu_id,
            R.id.bookmark_this_page_id,
            R.id.offline_page_id,
            R.id.info_menu_id,
            R.id.reload_menu_id
        };
        assertMenuItemsAreEqual(menu, expectedItems.toArray(new Integer[0]));
        assertMenuTitlesAreEqual(menu, expectedTitles.toArray(new Integer[0]));
        assertActionBarItemsAreEqual(menu, expectedActionBarItems);
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

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

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
        doReturn(false).when(mTabbedAppMenuPropertiesDelegate).shouldShowIconBeforeItem();

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

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
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).shouldShowIconBeforeItem();

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

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

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testOverviewMenuItems_Phone_SelectTabs() {
        setUpMocksForOverviewMenu();
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.close_all_tabs_menu_id,
            R.id.menu_select_tabs,
            R.id.quick_delete_menu_id,
            R.id.preferences_id
        };
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testOverviewMenuItems_Phone_SelectTabs_tabGroupEntryPointsFeatureEnabled() {
        setUpMocksForOverviewMenu();
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.new_tab_group_menu_id,
            R.id.close_all_tabs_menu_id,
            R.id.menu_select_tabs,
            R.id.quick_delete_menu_id,
            R.id.preferences_id
        };
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOverviewMenuItems_Tablet_NoTabs() {
        setUpIncognitoMocks();
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        when(mTabModel.getCount()).thenReturn(0);

        assertEquals(
                MenuGroup.TABLET_EMPTY_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

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
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class), anyBoolean());
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));

        // Ensure the get image descriptions option is shown as needed
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);

        // Test specific setup
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        AccessibilityState.setIsKnownScreenReaderEnabledForTesting(true);

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        ArrayList<Integer> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
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
                                R.id.auto_dark_web_contents_row_menu_id,
                                R.id.divider_line_id,
                                R.id.preferences_id,
                                R.id.help_id));
        if (!BuildConfig.IS_DESKTOP_ANDROID) {
            expectedItems.add(R.id.request_desktop_site_row_menu_id);
        }

        assertMenuItemsAreEqual(menu, expectedItems.toArray(new Integer[0]));

        // Ensure the text of the menu item is correct
        assertEquals(
                "Get image descriptions", menu.findItem(R.id.get_image_descriptions_id).getTitle());

        // Enable the feature and ensure text changes
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(true);

        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        assertEquals(
                "Stop image descriptions",
                menu.findItem(R.id.get_image_descriptions_id).getTitle());

        // Setup no wifi condition, and "only on wifi" user option.
        DeviceConditions noWifi =
                new DeviceConditions(false, 75, ConnectionType.CONNECTION_2G, false, false, true);
        ShadowDeviceConditions.setCurrentConditions(noWifi);
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI))
                .thenReturn(true);

        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        assertEquals(
                "Get image descriptions", menu.findItem(R.id.get_image_descriptions_id).getTitle());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_enterprise_user() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        when(mManagedBrowserUtilsJniMock.isBrowserManaged(any())).thenReturn(true);
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowManagedByMenuItem(any(Tab.class));

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        ArrayList<Integer> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
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
                                R.id.universal_install,
                                R.id.auto_dark_web_contents_row_menu_id,
                                R.id.divider_line_id,
                                R.id.preferences_id,
                                R.id.help_id,
                                R.id.managed_by_divider_line_id,
                                R.id.managed_by_menu_id));

        if (!BuildConfig.IS_DESKTOP_ANDROID) {
            expectedItems.add(R.id.request_desktop_site_row_menu_id);
        }

        assertMenuItemsAreEqual(menu, expectedItems.toArray(new Integer[0]));
    }

    @Test
    public void testPageMenuItems_multiWindowMenu_featureEnabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);

        // Single window
        //
        // No API support (i.e. cannot enter multi-window through menu), do not show 'New Window'
        testWindowMenu(WIN_S, ANY, ANY, API_NO, ANY, NEW_NO, ANY);

        // No 'New Window' on phone.
        testWindowMenu(WIN_S, INST_S, PHONE, ANY, MOVE_OTHER_NO, NEW_NO, MOVE_NO);

        // Show 'New Window' only on tablet and API is supported.
        testWindowMenu(WIN_S, INST_S, TABLET, API_YES, ANY, NEW_YES, MOVE_NO);

        //
        // Multi-window
        //
        // Move to other window supported, show 'Move to other window'
        testWindowMenu(WIN_M, INST_M, ANY, ANY, MOVE_OTHER_YES, ANY, MOVE_YES);

        // Move to other window not supported, hide 'Move to other window'
        testWindowMenu(WIN_M, INST_M, ANY, ANY, MOVE_OTHER_NO, ANY, MOVE_NO);

        // Single instance -> Show 'New window'
        testWindowMenu(WIN_M, INST_S, ANY, ANY, ANY, NEW_YES, MOVE_NO);

        assertTestedAllCombinations();
    }

    @Test
    public void testPageMenuItems_instanceSwitcher_newWindow() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);

        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .instanceSwitcherWithMultiInstanceEnabled();

        createInstance(0, "https://url0");

        // On phone, we do not show 'New Window'.
        mIsTabletScreen = false;
        Menu menu = createMenuForMultiWindow();
        assertFalse(isMenuVisible(menu, R.id.new_window_menu_id));

        // Multi-window mode, with a single instance (no adjacent instance running) makes
        // the menu visible.
        doReturn(false).when(mMultiWindowModeStateDispatcher).isChromeRunningInAdjacentWindow();
        mIsMultiWindow = true;

        menu = createMenuForMultiWindow();
        assertTrue(isMenuVisible(menu, R.id.new_window_menu_id));

        // On tablet, we show 'New Window' by default.
        mIsTabletScreen = true;
        mIsMultiWindow = false;
        menu = createMenuForMultiWindow();
        assertTrue(isMenuVisible(menu, R.id.new_window_menu_id));

        for (int i = 0; i < MultiWindowUtils.getMaxInstances(); ++i) {
            createInstance(i, "https://url" + i);
        }

        Menu menu2 = createMenuForMultiWindow();
        assertFalse(isMenuVisible(menu2, R.id.new_window_menu_id));
    }

    @Test
    public void testPageMenuItems_instanceSwitcher_moveTabToOtherWindow() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);

        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .instanceSwitcherWithMultiInstanceEnabled();
        mIsMoveToOtherWindowSupported = true;

        Menu menu = createMenuForMultiWindow();
        assertTrue(isMenuVisible(menu, R.id.move_to_other_window_menu_id));
    }

    @Test
    public void testPageMenuItems_instanceSwitcher_manageAllWindow() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);

        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .instanceSwitcherWithMultiInstanceEnabled();

        createInstance(0, "https://url0");

        Menu menu = createMenuForMultiWindow();
        assertFalse(isMenuVisible(menu, R.id.manage_all_windows_menu_id));

        createInstance(1, "https://url1");

        Menu menu2 = createMenuForMultiWindow();
        assertTrue(isMenuVisible(menu2, R.id.manage_all_windows_menu_id));
    }

    @Test
    public void testPageMenuItems_universalInstall() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        Menu menu = createMenuForMultiWindow();
        assertTrue(isMenuVisible(menu, R.id.universal_install));
        assertFalse(isMenuVisible(menu, R.id.open_webapk_id));
    }

    @Test
    public void managedByMenuItem_ChromeManagementPage() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions().withShowAddToHomeScreen());
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowManagedByMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        MenuItem managedByMenuItem = menu.findItem(R.id.managed_by_menu_id);

        Assert.assertNotNull(managedByMenuItem);
        assertTrue(managedByMenuItem.isVisible());
    }

    @Test
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
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        verify(mIncognitoReauthControllerMock, times(1)).isReauthPageShowing();

        MenuItem item = menu.findItem(R.id.new_incognito_tab_menu_id);
        assertFalse(item.isEnabled());
    }

    @Test
    public void testNewIncognitoTabOption_FromRegularMode_WithReauthNotInProgress() {
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());

        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        verifyNoMoreInteractions(mIncognitoReauthControllerMock);

        MenuItem item = menu.findItem(R.id.new_incognito_tab_menu_id);
        assertTrue(item.isEnabled());
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_IMPROVEMENTS + ":always_on_entry_point/false")
    public void readerModeEntryPointDisabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        MenuItem item = menu.findItem(R.id.reader_mode_menu_id);
        assertFalse(item.isVisible());
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_IMPROVEMENTS + ":always_on_entry_point/true")
    public void readerModeEntryPointEnabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        MenuItem item = menu.findItem(R.id.reader_mode_menu_id);
        assertTrue(item.isVisible());
    }

    private Menu setUpMenuWithIncognitoReauthPage(boolean isShowing) {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        prepareMocksForGroupTabsOnTabModel(mIncognitoTabModel);
        doReturn(isShowing).when(mIncognitoReauthControllerMock).isReauthPageShowing();

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        verify(mIncognitoReauthControllerMock, times(1)).isReauthPageShowing();
        return menu;
    }

    @Test
    public void testSelectTabsOption_IsEnabled_InIncognitoMode_When_IncognitoReauthIsNotShowing() {
        Menu menu = setUpMenuWithIncognitoReauthPage(/* isShowing= */ false);
        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertTrue(item.isEnabled());
    }

    @Test
    public void testSelectTabsOption_IsDisabled_InIncognitoMode_When_IncognitoReauthIsShowing() {
        Menu menu = setUpMenuWithIncognitoReauthPage(/* isShowing= */ true);
        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertFalse(item.isEnabled());
    }

    @Test
    public void testSelectTabsOption_IsEnabled_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        prepareMocksForGroupTabsOnTabModel(mTabModel);

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertTrue(item.isEnabled());
    }

    @Test
    public void testSelectTabsOption_IsDisabled_InRegularMode_TabStateNotInitialized() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        prepareMocksForGroupTabsOnTabModel(mTabModel);

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertFalse(item.isEnabled());
    }

    @Test
    public void testSelectTabsOption_IsEnabledOneTab_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(1);
        Tab mockTab1 = mock(Tab.class);
        when(mTabModel.getTabAt(0)).thenReturn(mockTab1);

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertTrue(item.isEnabled());
    }

    @Test
    public void testSelectTabsOption_IsDisabled_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MenuItem item = menu.findItem(R.id.menu_select_tabs);
        assertFalse(item.isEnabled());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION})
    public void testCustomizeNewTabPageOption() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(UrlConstants.NTP_URL));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        when(mActivityTabProvider.get()).thenReturn(ntpTab);

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        MenuItem item = menu.findItem(R.id.ntp_customization_id);
        assertTrue(item.isEnabled());
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
                .when(mTabbedAppMenuPropertiesDelegate)
                .instanceSwitcherWithMultiInstanceEnabled();
        doReturn(currentWindowInstances).when(mTabbedAppMenuPropertiesDelegate).getInstanceCount();
        doReturn(isTabletSizeScreen).when(mTabbedAppMenuPropertiesDelegate).isTabletSizeScreen();
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

        return mTabbedAppMenuPropertiesDelegate.shouldShowNewWindow();
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
        verify(mTabbedAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
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
        verify(mTabbedAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
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
        verify(mTabbedAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
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
        verify(mTabbedAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
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
        verify(mTabbedAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
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
        verify(mTabbedAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
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
        verify(mTabbedAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
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
        verify(mTabbedAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
        verify(mTabbedAppMenuPropertiesDelegate, never()).getInstanceCount();
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
        verify(mTabbedAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
        verify(mTabbedAppMenuPropertiesDelegate, never()).getInstanceCount();
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
        verify(mTabbedAppMenuPropertiesDelegate, never()).getInstanceCount();
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
        verify(mTabbedAppMenuPropertiesDelegate, never()).getInstanceCount();
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
                .when(mTabbedAppMenuPropertiesDelegate)
                .instanceSwitcherWithMultiInstanceEnabled();
        doReturn(currentWindowInstances).when(mTabbedAppMenuPropertiesDelegate).getInstanceCount();
        doReturn(isTabletSizeScreen).when(mTabbedAppMenuPropertiesDelegate).isTabletSizeScreen();
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

        return mTabbedAppMenuPropertiesDelegate.shouldShowMoveToOtherWindow();
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
        verify(mTabbedAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
    }

    @Test
    public void testReadAloudMenuItem_readAloudNotEnabled() {
        mReadAloudControllerSupplier.set(null);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        setUpMocksForPageMenu();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        assertFalse(menu.findItem(R.id.readaloud_menu_id).isVisible());
    }

    @Test
    public void testReadAloudMenuItem_notReadable() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mReadAloudController.isReadable(any())).thenReturn(false);
        setUpMocksForPageMenu();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        assertFalse(menu.findItem(R.id.readaloud_menu_id).isVisible());
    }

    @Test
    public void testReadAloudMenuItem_readable() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mReadAloudController.isReadable(any())).thenReturn(true);
        setUpMocksForPageMenu();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        assertTrue(menu.findItem(R.id.readaloud_menu_id).isVisible());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)
    public void testAiWebMenuItem_shouldAppearOnWebPages() {
        var aiAssistantService = mock(AiAssistantService.class);
        AiAssistantService.setInstanceForTesting(aiAssistantService);
        when(aiAssistantService.canShowAiForTab(any(), eq(mTab))).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        setUpMocksForPageMenu();

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        assertTrue(
                "AI Web menu item should be visible",
                menu.findItem(R.id.ai_web_menu_id).isVisible());
        assertFalse(
                "AI PDF menu item should not be visible",
                menu.findItem(R.id.ai_pdf_menu_id).isVisible());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)
    public void testAiPdfMenuItem_shouldAppearOnPdfPages() {
        var aiAssistantService = mock(AiAssistantService.class);
        AiAssistantService.setInstanceForTesting(aiAssistantService);
        when(aiAssistantService.canShowAiForTab(any(), eq(mTab))).thenReturn(true);

        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1_WITH_PDF_PATH);
        var pdfNativePage = mock(PdfPage.class);
        when(mTab.getNativePage()).thenReturn(pdfNativePage);
        when(mTab.isNativePage()).thenReturn(true);

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        assertFalse(
                "AI Web menu item should not be visible",
                menu.findItem(R.id.ai_web_menu_id).isVisible());
        assertTrue(
                "AI PDF menu item should be visible",
                menu.findItem(R.id.ai_pdf_menu_id).isVisible());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)
    public void testAiMenuItems_shouldNotAppearIfDisabled() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        setUpMocksForPageMenu();

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        assertFalse(
                "AI Web menu item should not be visible",
                menu.findItem(R.id.ai_web_menu_id).isVisible());
        assertFalse(
                "AI PDF menu item should not be visible",
                menu.findItem(R.id.ai_pdf_menu_id).isVisible());
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
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mReadAloudController.isReadable(mTab)).thenReturn(initiallyReadable);
        setUpMocksForPageMenu();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.getMenuItemsForMenu(menu, mAppMenuHandler);
        // When menu is created, the visibility should match readability state at that time
        assertEquals(initiallyReadable, hasReadAloudInMenu());

        when(mReadAloudController.isReadable(mTab)).thenReturn(laterReadable);
        // When a new readability result is retrieved, ensure that the menu item visibility matches
        // the current readability state.
        mTabbedAppMenuPropertiesDelegate.getReadAloudmenuResetter().run();
        assertEquals(laterReadable, hasReadAloudInMenu());
    }

    private boolean hasReadAloudInMenu() {
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getModelList();
        if (modelList == null) {
            return false;
        }
        Iterator<MVCListAdapter.ListItem> it = modelList.iterator();
        while (it.hasNext()) {
            MVCListAdapter.ListItem li = it.next();
            int id = li.model.get(AppMenuItemProperties.MENU_ITEM_ID);
            if (id == R.id.readaloud_menu_id) {
                return true;
            }
        }
        return false;
    }

    @Test
    public void getFooterResourceId_incognito_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mTab.isIncognito()).thenReturn(true);

        assertNotEquals(
                "Footer Resource ID should not be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_offlinePage_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mOfflinePageUtils.isOfflinePage(mTab)).thenReturn(true);

        assertNotEquals(
                "Footer Resource ID should not be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_nonHttpUrl_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mTab.getOriginalUrl()).thenReturn(JUnitTestGURLs.NTP_URL);

        assertNotEquals(
                "Footer Resource ID should not be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_signedOutUser_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);

        assertNotEquals(
                "Footer Resource ID should not be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_httpsUrl_returnsWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();

        assertEquals(
                "Footer Resource ID should be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_dseOff_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(false);

        assertNotEquals(
                "Footer Resource ID should not be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_dseOn_returnsWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);

        assertEquals(
                "Footer Resource ID should be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_signedOutUser_dseOn_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);

        assertNotEquals(
                "Footer Resource ID should not be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    private void setUpMocksForWebFeedFooter() {
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getOriginalUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mOfflinePageUtils.isOfflinePage(mTab)).thenReturn(false);
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
    }

    private void setUpMocksForPageMenu() {
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldCheckBookmarkStar(any(Tab.class));
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldEnableDownloadPage(any(Tab.class));
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowReaderModePrefs(any(Tab.class));
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowManagedByMenuItem(any(Tab.class));
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isAutoDarkWebContentsEnabled();

        setUpIncognitoMocks();
    }

    private Menu createTestMenu() {
        // mMultiWindowModeStateDispatcher.isOpenInOtherWindowSupported() is determined by
        // isInMultiWindowMode() and isInMultiDisplayMode(). Set that condition here.
        boolean openInOtherWindow =
                mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                        || mMultiWindowModeStateDispatcher.isInMultiDisplayMode();
        doReturn(openInOtherWindow)
                .when(mMultiWindowModeStateDispatcher)
                .isOpenInOtherWindowSupported();

        PopupMenu tempMenu = new PopupMenu(ContextUtils.getApplicationContext(), mDecorView);
        tempMenu.inflate(R.menu.main_menu);
        return tempMenu.getMenu();
    }

    private Menu createMenuForMultiWindow() {
        doReturn(mIsMultiWindow).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        doReturn(mIsMultiWindowApiSupported)
                .when(mMultiWindowModeStateDispatcher)
                .canEnterMultiWindowMode();
        doReturn(mIsMultiInstance).when(mMultiWindowModeStateDispatcher).isMultiInstanceRunning();
        doReturn(mIsTabletScreen).when(mTabbedAppMenuPropertiesDelegate).isTabletSizeScreen();
        doReturn(MultiWindowUtils.getInstanceCount())
                .when(mMultiWindowModeStateDispatcher)
                .getInstanceCount();
        doReturn(mIsMoveToOtherWindowSupported)
                .when(mMultiWindowModeStateDispatcher)
                .isMoveToOtherWindowSupported(mTabModelSelector);
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        return menu;
    }

    private void testWindowMenu(
            Boolean multiWindow,
            Boolean multiInstance,
            Boolean tablet,
            Boolean apiSupported,
            Boolean moveToOtherWindowSupported,
            Boolean showNewWindow,
            Boolean showMoveWindow) {
        for (int i = 0; i < (1 << 5); ++i) {
            boolean bitMultiWindow = (i & 1) == 1;
            boolean bitMultiInstance = ((i >> 1) & 1) == 1;
            boolean bitTabletScreen = ((i >> 2) & 1) == 1;
            boolean bitApiSupported = ((i >> 3) & 1) == 1;
            boolean bitMoveToOtherWindowSupported = ((i >> 4) & 1) == 1;

            if ((multiWindow == null || bitMultiWindow == multiWindow)
                    && (multiInstance == null || bitMultiInstance == multiInstance)
                    && (tablet == null || bitTabletScreen == tablet)
                    && (apiSupported == null || bitApiSupported == apiSupported)
                    && (moveToOtherWindowSupported == null
                            || bitMoveToOtherWindowSupported == moveToOtherWindowSupported)) {
                mIsMultiWindow = bitMultiWindow;
                mIsMultiInstance = bitMultiInstance;
                mIsTabletScreen = bitTabletScreen;
                mIsMultiWindowApiSupported = bitApiSupported;
                mIsMoveToOtherWindowSupported = bitMoveToOtherWindowSupported;

                // Ignore invalid combination.
                if ((!bitMultiWindow && bitMultiInstance)
                        || (!bitMultiInstance && bitMoveToOtherWindowSupported)) continue;

                mFlagCombinations[i] = true;
                Menu menu = createMenuForMultiWindow();
                if (showNewWindow != null) {
                    if (showNewWindow) {
                        assertTrue(getFlags(), isMenuVisible(menu, R.id.new_window_menu_id));
                    } else {
                        assertFalse(getFlags(), isMenuVisible(menu, R.id.new_window_menu_id));
                    }
                }
                if (showMoveWindow != null) {
                    if (showMoveWindow) {
                        assertTrue(
                                getFlags(), isMenuVisible(menu, R.id.move_to_other_window_menu_id));
                    } else {
                        assertFalse(
                                getFlags(), isMenuVisible(menu, R.id.move_to_other_window_menu_id));
                    }
                }
            }
        }
    }

    private void assertTestedAllCombinations() {
        for (int i = 0; i < (1 << 5); ++i) {
            boolean bitMultiWindow = (i & 1) == 1;
            boolean bitMultiInstance = ((i >> 1) & 1) == 1;
            boolean bitTabletScreen = ((i >> 2) & 1) == 1;
            boolean bitApiSupported = ((i >> 3) & 1) == 1;
            boolean bitMoveToOtherWindowSupported = ((i >> 4) & 1) == 1;

            // Ignore invalid combination.
            if ((!bitMultiWindow && bitMultiInstance)
                    || (!bitMultiInstance && bitMoveToOtherWindowSupported)) continue;

            assertTrue(
                    "Not tested: "
                            + getFlags(
                                    bitMultiWindow,
                                    bitMultiInstance,
                                    bitTabletScreen,
                                    bitApiSupported,
                                    bitMoveToOtherWindowSupported),
                    mFlagCombinations[i]);
        }
    }

    private String getFlags(
            boolean multiWindow,
            boolean multiInstance,
            boolean tablet,
            boolean apiSupported,
            boolean moveToOtherWindowSupported) {
        return "("
                + (multiWindow ? "WIN_M" : "WIN_S")
                + ", "
                + (multiInstance ? "INST_M" : "INST_S")
                + ", "
                + (tablet ? "TABLET" : "PHONE")
                + ", "
                + (apiSupported ? "API_YES" : "API_NO")
                + ", "
                + (moveToOtherWindowSupported ? "MOVE_OTHER_YES" : "MOVE_OTHER_NO")
                + ")";
    }

    private String getFlags() {
        return getFlags(
                mIsMultiWindow,
                mIsMultiInstance,
                mIsTabletScreen,
                mIsMultiWindowApiSupported,
                mIsMoveToOtherWindowSupported);
    }

    private boolean isMenuVisible(Menu menu, int itemId) {
        boolean found = false;
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).isVisible() && menu.getItem(i).getItemId() == itemId) {
                found = true;
                break;
            }
        }
        return found;
    }

    private String getMenuTitles(Menu menu) {
        StringBuilder items = new StringBuilder();
        for (int i = 0; i < menu.size(); i++) {
            MenuItem menuItem = menu.getItem(i);
            if (menuItem.isVisible()) {
                CharSequence title = menuItem.getTitle();
                if (title == null && menuItem.getItemId() == R.id.icon_row_menu_id) {
                    title = "Icon Row";
                }
                if (title == null
                        && (menuItem.getItemId() == R.id.divider_line_id
                                || menuItem.getItemId() == R.id.quick_delete_divider_line_id)) {
                    title = "Divider";
                }
                if (title == null && menuItem.hasSubMenu()) {
                    title = menuItem.getSubMenu().getItem(0).getTitle();
                }
                items.append("\n").append(title).append(":").append(menuItem.getItemId());
            }
        }
        return items.toString();
    }

    private static void createInstance(int index, String url) {
        String urlKey = ChromePreferenceKeys.MULTI_INSTANCE_URL.createKey(String.valueOf(index));
        ChromeSharedPreferences.getInstance().writeString(urlKey, url);
        String tabCountKey =
                ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT.createKey(String.valueOf(index));
        ChromeSharedPreferences.getInstance().writeInt(tabCountKey, 1);
        String accessTimeKey =
                ChromePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME.createKey(
                        String.valueOf(index));
        ChromeSharedPreferences.getInstance().writeLong(accessTimeKey, System.currentTimeMillis());
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
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));
        doReturn(options.showUpdate())
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowUpdateMenuItem();
        doReturn(options.showMoveToOtherWindow())
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowMoveToOtherWindow();
        doReturn(options.showReaderModePrefs())
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowReaderModePrefs(any(Tab.class));
        doReturn(options.showPaintPreview())
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class), anyBoolean());
        when(mWebsitePreferenceBridgeJniMock.getContentSetting(any(), anyInt(), any(), any()))
                .thenReturn(
                        options.isAutoDarkEnabled()
                                ? ContentSettingValues.DEFAULT
                                : ContentSettingValues.BLOCK);
    }
}
