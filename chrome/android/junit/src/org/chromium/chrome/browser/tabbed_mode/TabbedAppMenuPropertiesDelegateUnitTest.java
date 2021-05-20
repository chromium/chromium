// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
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
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettingsJni;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.content.browser.ContentFeatureListImpl;
import org.chromium.content.browser.ContentFeatureListImplJni;
import org.chromium.content_public.browser.ContentFeatureList;
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
@Features.EnableFeatures({ChromeFeatureList.WEB_FEED})
public class TabbedAppMenuPropertiesDelegateUnitTest {
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
    private OverviewModeBehavior mOverviewModeBehavior;
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
    private DataReductionProxySettings.Natives mDataReductionJniMock;
    @Mock
    private ContentFeatureListImpl.Natives mContentFeatureListJniMock;
    @Mock
    private WebFeedSnackbarController.FeedLauncher mFeedLauncher;
    @Mock
    private ModalDialogManager mDialogManager;
    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private WebFeedBridge mWebFeedBridge;
    @Mock
    private OfflinePageUtils.Internal mOfflinePageUtils;
    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    private TabModelFilter mTabModelFilter;

    private OneshotSupplierImpl<OverviewModeBehavior> mOverviewModeSupplier =
            new OneshotSupplierImpl<>();
    private ObservableSupplierImpl<BookmarkBridge> mBookmarkBridgeSupplier =
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

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mOverviewModeSupplier.set(mOverviewModeBehavior);
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
        jniMocker.mock(DataReductionProxySettingsJni.TEST_HOOKS, mDataReductionJniMock);
        when(mDataReductionJniMock.isDataReductionProxyEnabled(anyLong(), any())).thenReturn(false);
        jniMocker.mock(ContentFeatureListImplJni.TEST_HOOKS, mContentFeatureListJniMock);
        when(mContentFeatureListJniMock.isEnabled(
                     ContentFeatureList.EXPERIMENTAL_ACCESSIBILITY_LABELS))
                .thenReturn(false);
        OfflinePageUtils.setInstanceForTesting(mOfflinePageUtils);
        FeatureList.setTestCanUseDefaultsForTesting();

        mTabbedAppMenuPropertiesDelegate = Mockito.spy(new TabbedAppMenuPropertiesDelegate(
                ContextUtils.getApplicationContext(), mActivityTabProvider,
                mMultiWindowModeStateDispatcher, mTabModelSelector, mToolbarManager, mDecorView,
                mAppMenuDelegate, mOverviewModeSupplier, mBookmarkBridgeSupplier, mFeedLauncher,
                mDialogManager, mSnackbarManager, mWebFeedBridge));
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
                R.id.request_desktop_site_row_menu_id, R.id.divider_line_id, R.id.preferences_id,
                R.id.help_id, R.id.managed_by_menu_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    private void setMultiWindowMenuFlags(int i) {
        mIsMultiTab = (i & 1) == 1;
        mIsMultiInstance = ((i >> 1) & 1) == 1;
        mIsMultiWindow = ((i >> 2) & 1) == 1;
        mIsPartnerHomepageEnabled = ((i >> 3) & 1) == 1;
        mIsTabletScreen = ((i >> 4) & 1) == 1;
        mIsMultiWindowApiSupported = ((i >> 5) & 1) == 1;
    }

    private Menu initMocksForMultiWindowMenu(boolean isNewWindowFeatureEnabled) {
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
        doReturn(isNewWindowFeatureEnabled)
                .when(mTabbedAppMenuPropertiesDelegate)
                .isNewWindowMenuFeatureEnabled();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);
        return menu;
    }

    @Test
    public void testPageMenuItems_doNotShowNewWindowForNonTablet() {
        setUpMocksForPageMenu();

        for (int i = 0; i < (1 << 6); ++i) {
            setMultiWindowMenuFlags(i);
            mIsTabletScreen = false;
            Menu menu = initMocksForMultiWindowMenu(true);

            // 'New Window' is never enabled on non-tablet-sized screen.
            assertMenuDoesNotContain(menu, R.id.new_window_menu_id);
        }
    }

    @Test
    public void testPageMenuItems_neverShowBothNewWindowAndMoveToOtherWindow() {
        setUpMocksForPageMenu();

        for (int i = 0; i < (1 << 6); ++i) {
            setMultiWindowMenuFlags(i);
            Menu menu = initMocksForMultiWindowMenu(true);
            assertFalse(isMenuVisible(menu, R.id.new_window_menu_id)
                    && isMenuVisible(menu, R.id.move_to_other_window_menu_id));
        }
    }

    @Test
    public void testPageMenuItems_noMoveToOtherWindowForPartnerHomepageWithSingleTab() {
        setUpMocksForPageMenu();

        for (int i = 0; i < (1 << 6); ++i) {
            setMultiWindowMenuFlags(i);
            mIsPartnerHomepageEnabled = true;
            mIsMultiTab = false;
            Menu menu = initMocksForMultiWindowMenu(true);
            assertFalse(isMenuVisible(menu, R.id.move_to_other_window_menu_id));
        }
    }

    @Test
    public void testPageMenuItems_disabledFeature() {
        // If the feature is disabled, show 'move to other window' as before.
        setUpMocksForPageMenu();
        mIsMultiTab = true;
        mIsMultiWindow = true;
        mIsTabletScreen = true;
        Menu menu = initMocksForMultiWindowMenu(false);
        assertFalse(isMenuVisible(menu, R.id.new_window_menu_id));
        assertTrue(isMenuVisible(menu, R.id.move_to_other_window_menu_id));

        // Hide even 'move to other window' for single tab/enabled partner homepage.
        mIsPartnerHomepageEnabled = true;
        mIsMultiTab = false;
        menu = initMocksForMultiWindowMenu(false);
        assertFalse(isMenuVisible(menu, R.id.new_window_menu_id));
        assertFalse(isMenuVisible(menu, R.id.move_to_other_window_menu_id));
    }

    @Test
    public void testPageMenuItems_showMoveToOtherWindowOnPhone() {
        setUpMocksForPageMenu();
        mIsMultiTab = true;
        mIsMultiWindow = true;
        Menu menu = initMocksForMultiWindowMenu(true);
        assertFalse(isMenuVisible(menu, R.id.new_window_menu_id));
        assertTrue(isMenuVisible(menu, R.id.move_to_other_window_menu_id));
    }

    @Test
    public void testPageMenuItems_singleTab_singleWindow() {
        setUpMocksForPageMenu();

        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        doReturn(true).when(mMultiWindowModeStateDispatcher).canEnterMultiWindowMode();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        // Single tab in single-window mode shows 'New Window' only.
        assertMenuContains(menu, R.id.new_window_menu_id);
        assertMenuDoesNotContain(menu, R.id.move_to_other_window_menu_id);

        doReturn(false).when(mMultiWindowModeStateDispatcher).canEnterMultiWindowMode();
        menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        // Show neither if multi-window mode cannot be entered through menu.
        assertMenuDoesNotContain(menu, R.id.new_window_menu_id);
        assertMenuDoesNotContain(menu, R.id.move_to_other_window_menu_id);
    }

    @Test
    public void testPageMenuItems_singletab_multiwindow_multiinstance() {
        setUpMocksForPageMenu();

        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        doReturn(true).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        doReturn(true).when(mMultiWindowModeStateDispatcher).isMultiInstanceRunning();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        // Single tab in the current window can be moved to the other window, leaving
        // the current instance empty-tabbed.
        assertMenuDoesNotContain(menu, R.id.new_window_menu_id);
        assertMenuContains(menu, R.id.move_to_other_window_menu_id);
    }

    @Test
    public void testPageMenuItems_singletab_multiwindow_partnerhomepage() {
        setUpMocksForPageMenu();

        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        doReturn(true).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isPartnerHomepageEnabled();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        // 'Move to other window' should be hidden when there is a single tab on the current
        // window but partner homepage is set, since that would close the current instance,
        // which apparently is not intended.
        assertMenuContains(menu, R.id.new_window_menu_id);
        assertMenuDoesNotContain(menu, R.id.move_to_other_window_menu_id);

        // Should be invisible when in multi-instance mode as well.
        doReturn(true).when(mMultiWindowModeStateDispatcher).isMultiInstanceRunning();
        menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        assertMenuDoesNotContain(menu, R.id.new_window_menu_id);
        assertMenuDoesNotContain(menu, R.id.move_to_other_window_menu_id);
    }

    @Test
    public void testPageMenuItems_multipletab_singlewindow_singleinstance() {
        setUpMocksForPageMenu();

        doReturn(2).when(mTabModelSelector).getTotalTabCount();
        doReturn(true).when(mMultiWindowModeStateDispatcher).canEnterMultiWindowMode();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        // Multiple tabs in single-window mode with single instance shows 'New Window' only
        // if it's capable of entering multi-window mode.
        assertMenuContains(menu, R.id.new_window_menu_id);
        assertMenuDoesNotContain(menu, R.id.move_to_other_window_menu_id);
    }

    @Test
    public void testPageMenuItems_multipletab_multiwindow_multiinstance() {
        setUpMocksForPageMenu();

        doReturn(2).when(mTabModelSelector).getTotalTabCount();
        doReturn(true).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        doReturn(true).when(mMultiWindowModeStateDispatcher).isMultiInstanceRunning();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        // Multiple tabs in multi-window mode shows 'move to other window' only.
        assertMenuDoesNotContain(menu, R.id.new_window_menu_id);
        assertMenuContains(menu, R.id.move_to_other_window_menu_id);

        // Partner homepage doesn't affect when there are multiple tabs.
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isPartnerHomepageEnabled();
        menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        // Multiple tabs in multi-window mode shows 'move to other window' only.
        assertMenuDoesNotContain(menu, R.id.new_window_menu_id);
        assertMenuContains(menu, R.id.move_to_other_window_menu_id);
    }

    @Test
    public void testPageMenuItems_multiwindow_singleinstance() {
        setUpMocksForPageMenu();

        doReturn(2).when(mTabModelSelector).getTotalTabCount();
        doReturn(true).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        Menu menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        // In multi-window, single instance mode we show 'new window'.
        assertMenuContains(menu, R.id.new_window_menu_id);
        assertMenuDoesNotContain(menu, R.id.move_to_other_window_menu_id);

        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        menu = createTestMenu();
        mTabbedAppMenuPropertiesDelegate.prepareMenu(menu, null);

        // Even with single tab, we show 'new window'.
        assertMenuContains(menu, R.id.new_window_menu_id);
        assertMenuDoesNotContain(menu, R.id.move_to_other_window_menu_id);
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
    }

    private void setUpMocksForPageMenu() {
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(false);
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
        when(mManagedBrowserUtilsJniMock.hasBrowserPoliciesApplied(any())).thenReturn(true);
    }

    private void setUpIncognitoMocks() {
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isIncognitoEnabled();
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
        tempMenu.inflate(mTabbedAppMenuPropertiesDelegate.getAppMenuLayoutId());
        return tempMenu.getMenu();
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
}
