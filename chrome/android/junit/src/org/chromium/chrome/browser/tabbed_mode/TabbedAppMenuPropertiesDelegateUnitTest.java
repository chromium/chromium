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
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.Menu;
import android.view.View;
import android.widget.PopupMenu;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.accessibility.PageZoomCoordinator;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link TabbedAppMenuPropertiesDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
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
    private static final Boolean X____ = null; // do not care

    @Rule public JniMocker jniMocker = new JniMocker();
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private NavigationController mNavigationController;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
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
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabModelFilter mTabModelFilter;
    @Mock public WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock private IncognitoReauthController mIncognitoReauthControllerMock;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingService mShoppingService;
    @Mock private AppBannerManager.Natives mAppBannerManagerJniMock;
    @Mock private ReadAloudController mReadAloudController;
    @Mock private PrefService mPrefService;
    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;

    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private OneshotSupplierImpl<IncognitoReauthController> mIncognitoReauthControllerSupplier =
            new OneshotSupplierImpl<>();
    private ObservableSupplierImpl<BookmarkModel> mBookmarkModelSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<ReadAloudController> mReadAloudControllerSupplier =
            new ObservableSupplierImpl<>();

    private TabbedAppMenuPropertiesDelegate mTabbedAppMenuPropertiesDelegate;

    // Boolean flags to test multi-window menu visibility for various combinations.
    private boolean mIsMultiInstance;
    private boolean mIsMultiWindow;
    private boolean mIsTabletScreen;
    private boolean mIsMultiWindowApiSupported;
    private boolean mIsMoveToOtherWindowSupported;

    // Used to ensure all the combinations are tested.
    private boolean[] mFlagCombinations = new boolean[1 << 5];

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        mIncognitoReauthControllerSupplier.set(mIncognitoReauthControllerMock);
        mReadAloudControllerSupplier.set(mReadAloudController);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getCurrentTabModelFilter()).thenReturn(mTabModelFilter);
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        jniMocker.mock(ManagedBrowserUtilsJni.TEST_HOOKS, mManagedBrowserUtilsJniMock);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        jniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        OfflinePageUtils.setInstanceForTesting(mOfflinePageUtils);
        when(mIdentityService.getSigninManager(any(Profile.class))).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        PageZoomCoordinator.setShouldShowMenuItemForTesting(false);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        jniMocker.mock(AppBannerManagerJni.TEST_HOOKS, mAppBannerManagerJniMock);
        Mockito.when(mAppBannerManagerJniMock.getInstallableWebAppManifestId(any()))
                .thenReturn(null);
        jniMocker.mock(WebFeedBridgeJni.TEST_HOOKS, mWebFeedBridgeJniMock);
        when(mWebFeedBridgeJniMock.isWebFeedEnabled()).thenReturn(true);

        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        PowerBookmarkUtils.setPowerBookmarkMetaForTesting(PowerBookmarkMeta.newBuilder().build());
        mTabbedAppMenuPropertiesDelegate =
                Mockito.spy(
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
                                mReadAloudControllerSupplier));
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_URL);
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT);

        jniMocker.mock(CommerceFeatureUtilsJni.TEST_HOOKS, mCommerceFeatureUtilsJniMock);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_enterprise_user() {
        setUpMocksForPageMenu();

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
                                R.id.translate_id,
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

        // Single window
        //
        // No API support (i.e. cannot enter multi-window through menu), do not show 'New Window'
        testWindowMenu(WIN_S, X____, X____, API_NO, X____, NEW_NO, X____);

        // No 'New Window' on phone.
        testWindowMenu(WIN_S, INST_S, PHONE, X____, MOVE_OTHER_NO, NEW_NO, MOVE_NO);

        // Show 'New Window' only on tablet and API is supported.
        testWindowMenu(WIN_S, INST_S, TABLET, API_YES, X____, NEW_YES, MOVE_NO);

        //
        // Multi-window
        //
        // Move to other window supported, show 'Move to other window'
        testWindowMenu(WIN_M, INST_M, X____, X____, MOVE_OTHER_YES, X____, MOVE_YES);

        // Move to other window not supported, hide 'Move to other window'
        testWindowMenu(WIN_M, INST_M, X____, X____, MOVE_OTHER_NO, X____, MOVE_NO);

        // Single instance -> Show 'New window'
        testWindowMenu(WIN_M, INST_S, X____, X____, X____, NEW_YES, MOVE_NO);

        assertTestedAllCombinations();
    }

    @Test
    public void testPageMenuItems_instanceSwitcher_newWindow() {
        setUpMocksForPageMenu();
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
        Menu menu = createMenuForMultiWindow();
        assertTrue(isMenuVisible(menu, R.id.universal_install));
        assertFalse(isMenuVisible(menu, R.id.open_webapk_id));
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
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isTabletSizeScreen();
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isAutoDarkWebContentsEnabled();

        setUpIncognitoMocks();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        when(mTab.isNativePage()).thenReturn(false);
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class), anyBoolean());
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));
        when(mManagedBrowserUtilsJniMock.isBrowserManaged(any())).thenReturn(true);
    }

    private void setUpIncognitoMocks() {
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isIncognitoEnabled();
        doReturn(false).when(mIncognitoReauthControllerMock).isIncognitoReauthPending();
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

    private void assertMenuContains(Menu menu, int itemId) {
        Assert.assertTrue(
                "Item should must be contained in the menu: " + itemId,
                isMenuVisible(menu, itemId));
    }

    private void assertMenuDoesNotContain(Menu menu, int itemId) {
        Assert.assertFalse(
                "Item should must not be contained in the menu: " + itemId,
                isMenuVisible(menu, itemId));
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
}
