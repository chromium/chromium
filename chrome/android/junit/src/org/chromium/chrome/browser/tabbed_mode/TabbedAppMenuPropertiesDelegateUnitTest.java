// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.view.Menu;
import android.view.View;
import android.widget.PopupMenu;

import org.hamcrest.Matchers;
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
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
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
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.accessibility.PageZoomCoordinator;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link TabbedAppMenuPropertiesDelegate}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.WEB_FEED, ChromeFeatureList.READ_LATER,
        ChromeFeatureList.BOOKMARKS_REFRESH})
@Features.DisableFeatures({ChromeFeatureList.SHOPPING_LIST, ChromeFeatureList.WEB_APK_UNIQUE_ID})
public class TabbedAppMenuPropertiesDelegateUnitTest {
    // Costants defining flags that determines multi-window menu items visibility.
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
    private static final boolean PARTNER_YES = true; // partner homepage enabled
    private static final boolean PARTNER_NO = false;
    private static final boolean NEW_YES = true; // show 'new window'
    private static final boolean NEW_NO = false;
    private static final boolean MOVE_YES = true; // show 'move to other window'
    private static final boolean MOVE_NO = false;
    private static final Boolean X____ = null; // do not care

    @Rule
    public JniMocker jniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
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
    private ToolbarManager mToolbarManager;
    @Mock
    private View mDecorView;
    @Mock
    private LayoutStateProvider mLayoutStateProvider;
    @Mock
    private ManagedBrowserUtils.Natives mManagedBrowserUtilsJniMock;
    @Mock
    private Profile mProfile;
    @Mock
    private AppMenuDelegate mAppMenuDelegate;
    @Mock
    Profile.Natives mProfileJniMock;
    @Mock
    Profile mProfileMock;
    @Mock
    private WebFeedSnackbarController.FeedLauncher mFeedLauncher;
    @Mock
    private ModalDialogManager mDialogManager;
    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private OfflinePageUtils.Internal mOfflinePageUtils;
    @Mock
    private SigninManager mSigninManager;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private IdentityServicesProvider mIdentityService;
    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    private TabModelFilter mTabModelFilter;
    @Mock
    public WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock
    private IncognitoReauthController mIncognitoReauthControllerMock;
    @Mock
    private ShoppingService mShoppingService;

    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private OneshotSupplierImpl<IncognitoReauthController> mIncognitoReauthControllerSupplier =
            new OneshotSupplierImpl<>();
    private ObservableSupplierImpl<BookmarkModel> mBookmarkModelSupplier =
            new ObservableSupplierImpl<>();

    private TabbedAppMenuPropertiesDelegate mTabbedAppMenuPropertiesDelegate;

    // Boolean flags to test multi-window menu visibility for various combinations.
    private boolean mIsMultiTab;
    private boolean mIsMultiInstance;
    private boolean mIsMultiWindow;
    private boolean mIsPartnerHomepageEnabled;
    private boolean mIsTabletScreen;
    private boolean mIsMultiWindowApiSupported;
    private boolean mIsNewWindowMenuFeatureEnabled;

    // Used to ensure all the combinations are tested.
    private boolean[] mFlagCombinations = new boolean[1 << 6];

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        mIncognitoReauthControllerSupplier.set(mIncognitoReauthControllerMock);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn((mTabModel));
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getCurrentTabModelFilter()).thenReturn(mTabModelFilter);
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        jniMocker.mock(ProfileJni.TEST_HOOKS, mProfileJniMock);
        when(mProfileJniMock.fromWebContents(any(WebContents.class))).thenReturn(mProfileMock);
        jniMocker.mock(ManagedBrowserUtilsJni.TEST_HOOKS, mManagedBrowserUtilsJniMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        jniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        OfflinePageUtils.setInstanceForTesting(mOfflinePageUtils);
        when(mIdentityService.getSigninManager(any(Profile.class))).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        FeatureList.setTestCanUseDefaultsForTesting();
        PageZoomCoordinator.setShouldShowMenuItemForTesting(false);

        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        PowerBookmarkUtils.setPowerBookmarkMetaForTesting(PowerBookmarkMeta.newBuilder().build());
        mTabbedAppMenuPropertiesDelegate = Mockito.spy(
                new TabbedAppMenuPropertiesDelegate(ContextUtils.getApplicationContext(),
                        mActivityTabProvider, mMultiWindowModeStateDispatcher, mTabModelSelector,
                        mToolbarManager, mDecorView, mAppMenuDelegate, mLayoutStateProviderSupplier,
                        null, mBookmarkModelSupplier, mFeedLauncher, mDialogManager,
                        mSnackbarManager, mIncognitoReauthControllerSupplier));
        SharedPreferencesManager.getInstance().removeKeysWithPrefix(
                ChromePreferenceKeys.MULTI_INSTANCE_URL);
        SharedPreferencesManager.getInstance().removeKeysWithPrefix(
                ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT);

        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage_enterprise_user() {
        setUpMocksForPageMenu();

        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.divider_line_id, R.id.open_history_menu_id,
                R.id.downloads_menu_id, R.id.all_bookmarks_menu_id, R.id.recent_tabs_menu_id,
                R.id.divider_line_id, R.id.translate_id, R.id.share_row_menu_id,
                R.id.find_in_page_id, R.id.add_to_homescreen_id,
                R.id.request_desktop_site_row_menu_id, R.id.auto_dark_web_contents_row_menu_id,
                R.id.divider_line_id, R.id.preferences_id, R.id.help_id,
                R.id.managed_by_divider_line_id, R.id.managed_by_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    public void testPageMenuItems_multiWindowMenu_featureDisabled() {
        setUpMocksForPageMenu();

        mIsNewWindowMenuFeatureEnabled = false;

        // In general multiple tab/window/instances, show 'Move to other window'
        testWindowMenu(TAB_M, WIN_M, INST_M, X____, X____, X____, NEW_NO, MOVE_YES);

        // In multi-window, show it if partner homepage is disabled.
        // Even though partner homepage is enabled, multiple tabs allows it as well.
        testWindowMenu(X____, WIN_M, X____, X____, X____, PARTNER_NO, NEW_NO, MOVE_YES);
        testWindowMenu(TAB_M, WIN_M, INST_S, X____, X____, PARTNER_YES, NEW_NO, MOVE_YES);

        // Hide it for single window, or single tab + partner homepage set.
        testWindowMenu(X____, WIN_S, INST_S, X____, X____, X____, NEW_NO, MOVE_NO);
        testWindowMenu(TAB_S, WIN_M, X____, X____, X____, PARTNER_YES, NEW_NO, MOVE_NO);

        assertTestedAllCombinations();
    }

    @Test
    public void testPageMenuItems_multiWindowMenu_featureEnabled() {
        setUpMocksForPageMenu();

        mIsNewWindowMenuFeatureEnabled = true;

        //
        // Single window
        //
        // No API support (i.e. cannot enter multi-window through menu), do not show 'New Window'
        testWindowMenu(X____, WIN_S, X____, X____, API_NO, X____, NEW_NO, X____);

        // No 'New Window' on phone.
        testWindowMenu(X____, WIN_S, INST_S, PHONE, X____, X____, NEW_NO, MOVE_NO);

        // Show 'New Window' only on tablet and API is supported.
        testWindowMenu(X____, WIN_S, INST_S, TABLET, API_YES, X____, NEW_YES, MOVE_NO);

        //
        // Multi-window
        //
        // In general multiple tab/window/instances, show 'Move to other window'
        testWindowMenu(TAB_M, WIN_M, INST_M, X____, X____, X____, NEW_NO, MOVE_YES);

        // Single instance -> Show 'New window'
        testWindowMenu(X____, WIN_M, INST_S, X____, X____, X____, NEW_YES, MOVE_NO);

        // Single tab can be moved when partner homepage is disabled.
        testWindowMenu(TAB_S, WIN_M, INST_M, X____, X____, PARTNER_NO, NEW_NO, MOVE_YES);

        // Single tab cannot be moved when partner homepage is enabled.
        testWindowMenu(TAB_S, WIN_M, INST_M, X____, X____, PARTNER_YES, NEW_NO, MOVE_NO);

        assertTestedAllCombinations();
    }

    @Test
    public void testPageMenuItems_instanceSwitcher_newWindow() {
        setUpMocksForPageMenu();
        mIsNewWindowMenuFeatureEnabled = true;
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).instanceSwitcherEnabled();

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
        mIsNewWindowMenuFeatureEnabled = true;
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).instanceSwitcherEnabled();

        createInstance(0, "https://url0");

        Menu menu = createMenuForMultiWindow();
        assertEquals(1, mMultiWindowModeStateDispatcher.getInstanceCount());
        assertFalse(isMenuVisible(menu, R.id.move_to_other_window_menu_id));

        createInstance(1, "https://url1");

        Menu menu2 = createMenuForMultiWindow();
        assertEquals(2, mMultiWindowModeStateDispatcher.getInstanceCount());
        assertTrue(isMenuVisible(menu2, R.id.move_to_other_window_menu_id));
    }

    @Test
    public void testPageMenuItems_instanceSwitcher_manageAllWindow() {
        setUpMocksForPageMenu();
        mIsNewWindowMenuFeatureEnabled = true;
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).instanceSwitcherEnabled();

        createInstance(0, "https://url0");

        Menu menu = createMenuForMultiWindow();
        assertFalse(isMenuVisible(menu, R.id.manage_all_windows_menu_id));

        createInstance(1, "https://url1");

        Menu menu2 = createMenuForMultiWindow();
        assertTrue(isMenuVisible(menu2, R.id.manage_all_windows_menu_id));
    }

    @Test
    public void getFooterResourceId_incognito_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mTab.isIncognito()).thenReturn(true);

        assertNotEquals("Footer Resource ID should not be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_offlinePage_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mOfflinePageUtils.isOfflinePage(mTab)).thenReturn(true);

        assertNotEquals("Footer Resource ID should not be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_nonHttpUrl_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mTab.getOriginalUrl()).thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));

        assertNotEquals("Footer Resource ID should not be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_signedOutUser_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);

        assertNotEquals("Footer Resource ID should not be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    @Test
    public void getFooterResourceId_httpsUrl_returnsWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();

        assertEquals("Footer Resource ID should be web_feed_main_menu_item.",
                R.layout.web_feed_main_menu_item,
                mTabbedAppMenuPropertiesDelegate.getFooterResourceId());
    }

    private void setUpMocksForWebFeedFooter() {
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getOriginalUrl()).thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL));
        when(mOfflinePageUtils.isOfflinePage(mTab)).thenReturn(false);
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
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
        doReturn(false).when(mTabbedAppMenuPropertiesDelegate).isPartnerHomepageEnabled();
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isTabletSizeScreen();
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isNewWindowMenuFeatureEnabled();
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isAutoDarkWebContentsEnabled();

        setUpIncognitoMocks();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL));
        when(mTab.isNativePage()).thenReturn(false);
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class), anyBoolean());
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));
        doReturn(new AppBannerManager.InstallStringPair(
                         R.string.menu_add_to_homescreen, R.string.add))
                .when(mTabbedAppMenuPropertiesDelegate)
                .getAddToHomeScreenTitle(mTab);
        when(mManagedBrowserUtilsJniMock.isBrowserManaged(any())).thenReturn(true);
    }

    private void setUpIncognitoMocks() {
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isIncognitoEnabled();
        doReturn(false).when(mIncognitoReauthControllerMock).isIncognitoReauthPending();
    }

    private Menu createTestMenu() {
        // mMultiWindowModeStateDispatcher.isOpenInOtherWindowSupported() is determined by
        // isInMultiWindowMode() and isInMultiDisplayMode(). Set that condition here.
        boolean openInOtherWindow = mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                || mMultiWindowModeStateDispatcher.isInMultiDisplayMode();
        doReturn(openInOtherWindow)
                .when(mMultiWindowModeStateDispatcher)
                .isOpenInOtherWindowSupported();

        PopupMenu tempMenu = new PopupMenu(ContextUtils.getApplicationContext(), mDecorView);
        tempMenu.inflate(R.menu.main_menu);
        return tempMenu.getMenu();
    }

    private void setMultiWindowMenuFlags(int i) {
        mIsMultiTab = (i & 1) == 1;
        mIsMultiInstance = ((i >> 1) & 1) == 1;
        mIsMultiWindow = ((i >> 2) & 1) == 1;
        mIsPartnerHomepageEnabled = ((i >> 3) & 1) == 1;
        mIsTabletScreen = ((i >> 4) & 1) == 1;
        mIsMultiWindowApiSupported = ((i >> 5) & 1) == 1;
    }

    private Menu createMenuForMultiWindow() {
        doReturn(mIsMultiTab ? 2 : 1).when(mTabModelSelector).getTotalTabCount();
        doReturn(mIsMultiWindow).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        doReturn(mIsMultiWindowApiSupported)
                .when(mMultiWindowModeStateDispatcher)
                .canEnterMultiWindowMode();
        doReturn(mIsMultiInstance).when(mMultiWindowModeStateDispatcher).isMultiInstanceRunning();
        doReturn(mIsPartnerHomepageEnabled)
                .when(mTabbedAppMenuPropertiesDelegate)
                .isPartnerHomepageEnabled();
        doReturn(mIsTabletScreen).when(mTabbedAppMenuPropertiesDelegate).isTabletSizeScreen();
        doReturn(mIsNewWindowMenuFeatureEnabled)
                .when(mTabbedAppMenuPropertiesDelegate)
                .isNewWindowMenuFeatureEnabled();
        doReturn(MultiWindowUtils.getInstanceCount())
                .when(mMultiWindowModeStateDispatcher)
                .getInstanceCount();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        return menu;
    }

    private void testWindowMenu(Boolean multiTab, Boolean multiWindow, Boolean multiInstance,
            Boolean tablet, Boolean apiSupported, Boolean partnerEnabled, Boolean showNewWindow,
            Boolean showMoveWindow) {
        for (int i = 0; i < (1 << 6); ++i) {
            boolean bitMultiTab = (i & 1) == 1;
            boolean bitMultiWindow = ((i >> 1) & 1) == 1;
            boolean bitMultiInstance = ((i >> 2) & 1) == 1;
            boolean bitTabletScreen = ((i >> 3) & 1) == 1;
            boolean bitApiSupported = ((i >> 4) & 1) == 1;
            boolean bitPartnerEnabled = ((i >> 5) & 1) == 1;

            if ((multiTab == null || bitMultiTab == multiTab)
                    && (multiWindow == null || bitMultiWindow == multiWindow)
                    && (multiInstance == null || bitMultiInstance == multiInstance)
                    && (tablet == null || bitTabletScreen == tablet)
                    && (apiSupported == null || bitApiSupported == apiSupported)
                    && (partnerEnabled == null || bitPartnerEnabled == partnerEnabled)) {
                mIsMultiTab = bitMultiTab;
                mIsMultiWindow = bitMultiWindow;
                mIsMultiInstance = bitMultiInstance;
                mIsTabletScreen = bitTabletScreen;
                mIsMultiWindowApiSupported = bitApiSupported;
                mIsPartnerHomepageEnabled = bitPartnerEnabled;

                // Ignore invalid combination.
                if (!bitMultiWindow && bitMultiInstance) continue;

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
        for (int i = 0; i < (1 << 6); ++i) {
            boolean bitMultiTab = (i & 1) == 1;
            boolean bitMultiWindow = ((i >> 1) & 1) == 1;
            boolean bitMultiInstance = ((i >> 2) & 1) == 1;
            boolean bitTabletScreen = ((i >> 3) & 1) == 1;
            boolean bitApiSupported = ((i >> 4) & 1) == 1;
            boolean bitPartnerEnabled = ((i >> 5) & 1) == 1;

            // Ignore invalid combination.
            if (!bitMultiWindow && bitMultiInstance) continue;

            assertTrue("Not tested: "
                            + getFlags(bitMultiTab, bitMultiWindow, bitMultiInstance,
                                    bitTabletScreen, bitApiSupported, bitPartnerEnabled),
                    mFlagCombinations[i]);
        }
    }

    private String getFlags(boolean multiTab, boolean multiWindow, boolean multiInstance,
            boolean tablet, boolean apiSupported, boolean partnerEnabled) {
        return "(" + (multiTab ? "TAB_M" : "TAB_S") + ", " + (multiWindow ? "WIN_M" : "WIN_S")
                + ", " + (multiInstance ? "INST_M" : "INST_S") + ", "
                + (tablet ? "TABLET" : "PHONE") + ", " + (apiSupported ? "API_YES" : "API_NO")
                + ", " + (partnerEnabled ? "PARTNER_YES" : "PARTNER_NO") + ")";
    }

    private String getFlags() {
        return getFlags(mIsMultiTab, mIsMultiWindow, mIsMultiInstance, mIsTabletScreen,
                mIsMultiWindowApiSupported, mIsPartnerHomepageEnabled);
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
        Assert.assertTrue("Item should must be contained in the menu: " + itemId,
                isMenuVisible(menu, itemId));
    }

    private void assertMenuDoesNotContain(Menu menu, int itemId) {
        Assert.assertFalse("Item should must not be contained in the menu: " + itemId,
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
        SharedPreferencesManager.getInstance().writeString(urlKey, url);
        String tabCountKey =
                ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT.createKey(String.valueOf(index));
        SharedPreferencesManager.getInstance().writeInt(tabCountKey, 1);
        String accessTimeKey = ChromePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME.createKey(
                String.valueOf(index));
        SharedPreferencesManager.getInstance().writeLong(accessTimeKey, System.currentTimeMillis());
    }
}
