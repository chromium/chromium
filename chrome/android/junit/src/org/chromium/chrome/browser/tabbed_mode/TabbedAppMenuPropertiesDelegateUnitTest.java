// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettingsJni;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.preferences.Pref;
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
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.content.browser.ContentFeatureListImpl;
import org.chromium.content.browser.ContentFeatureListImplJni;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
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
    private UserPrefs.Natives mUserPrefsJniMock;
    @Mock
    private PrefService mPrefServiceMock;
    @Mock
    private DataReductionProxySettings.Natives mDataReductionJniMock;
    @Mock
    private ContentFeatureListImpl.Natives mContentFeatureListJniMock;
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
        jniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mProfileJniMock.fromWebContents(any(WebContents.class))).thenReturn(mProfileMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefServiceMock);
        when(mPrefServiceMock.getBoolean(Pref.ENABLE_WEB_FEED_UI)).thenReturn(true);
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

        mTabbedAppMenuPropertiesDelegate = Mockito.spy(
                new TabbedAppMenuPropertiesDelegate(ContextUtils.getApplicationContext(),
                        mActivityTabProvider, mMultiWindowModeStateDispatcher, mTabModelSelector,
                        mToolbarManager, mDecorView, mAppMenuDelegate, mOverviewModeSupplier,
                        mBookmarkBridgeSupplier, mSnackbarManager, mWebFeedBridge));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @EnableFeatures({ChromeFeatureList.ANDROID_MANAGED_BY_MENU_ITEM})
    public void testPageMenuItems_Phone_RegularPage_enterprise_user() {
        setUpMocksForPageMenu();
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

        setUpIncognitoMocks();
    }

    private void setUpIncognitoMocks() {
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isIncognitoEnabled();
    }

    private Menu createTestMenu() {
        PopupMenu tempMenu = new PopupMenu(ContextUtils.getApplicationContext(), mDecorView);
        tempMenu.inflate(mTabbedAppMenuPropertiesDelegate.getAppMenuLayoutId());
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
