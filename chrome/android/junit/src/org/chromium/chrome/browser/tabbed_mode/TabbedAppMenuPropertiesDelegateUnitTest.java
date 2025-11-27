// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
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
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

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
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ai.AiAssistantService;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl.MenuGroup;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedMainMenuItem;
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
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
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
import org.chromium.chrome.browser.toolbar.menu_button.MenuItemState;
import org.chromium.chrome.browser.toolbar.menu_button.MenuUiState;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemWithSubmenuProperties;
import org.chromium.chrome.browser.ui.extensions.ExtensionsBuildflags;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionUiBackendRule;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.browser_ui.accessibility.PageZoomManager;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;
import org.chromium.net.ConnectionType;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
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
    ChromeFeatureList.FEED_AUDIO_OVERVIEWS,
    DomDistillerFeatures.READER_MODE_IMPROVEMENTS,
    DomDistillerFeatures.READER_MODE_DISTILL_IN_APP
})
@EnableFeatures({
    ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID,
    ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID,
    ChromeFeatureList.SUBMENUS_IN_APP_MENU,
    ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS
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

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextWrapperTestRule =
            new OverrideContextWrapperTestRule();

    @Rule
    public FakeExtensionUiBackendRule mFakeExtensionUiBackendRule =
            new FakeExtensionUiBackendRule();

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
    @Mock private AppMenuHandler mAppMenuHandler;
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
    @Mock private TranslateBridge.Natives mTranslateBridgeJniMock;
    @Mock private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock private LargeIconBridge.Natives mLargeIconBridgeJni;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private PageZoomManager mPageZoomManagerMock;

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
    private MenuUiState mUpdateAvailableMenuUiState;

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
        when(mIdentityService.getIdentityManager(any(Profile.class))).thenReturn(mIdentityManager);
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        PageZoomUtils.setShouldShowMenuItemForTesting(false);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        FeedServiceBridgeJni.setInstanceForTesting(mFeedServiceBridgeJniMock);
        when(mSyncService.getAuthError())
                .thenReturn(new GoogleServiceAuthError(GoogleServiceAuthErrorState.NONE));
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isTrustedVaultRecoverabilityDegraded()).thenReturn(false);
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        AppBannerManagerJni.setInstanceForTesting(mAppBannerManagerJniMock);
        Mockito.when(mAppBannerManagerJniMock.getInstallableWebAppManifestId(any()))
                .thenReturn(null);
        WebFeedBridgeJni.setInstanceForTesting(mWebFeedBridgeJniMock);
        when(mWebFeedBridgeJniMock.isWebFeedEnabled()).thenReturn(true);
        UserPrefsJni.setInstanceForTesting(mUserPrefsNatives);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);

        SyncServiceFactory.setInstanceForTesting(mSyncService);

        IncognitoUtilsJni.setInstanceForTesting(mIncognitoUtilsJniMock);

        TranslateBridgeJni.setInstanceForTesting(mTranslateBridgeJniMock);
        Mockito.when(mTranslateBridgeJniMock.canManuallyTranslate(any(), anyBoolean()))
                .thenReturn(false);

        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
        doReturn(new MenuUiState()).when(mUpdateMenuItemHelper).getUiState();

        mUpdateAvailableMenuUiState = new MenuUiState();
        mUpdateAvailableMenuUiState.itemState = new MenuItemState();
        mUpdateAvailableMenuUiState.itemState.title = R.string.menu_update;
        mUpdateAvailableMenuUiState.itemState.titleColorId = R.color.default_text_color_error;
        mUpdateAvailableMenuUiState.itemState.icon = R.drawable.menu_update;

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
                        mReadAloudControllerSupplier,
                        mPageZoomManagerMock);
        BaseRobolectricTestRule.runAllBackgroundAndUi();
        mTabbedAppMenuPropertiesDelegate = Mockito.spy(delegate);

        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_URL);
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT);

        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);

        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);

        DomDistillerUrlUtilsJni.setInstanceForTesting(mDomDistillerUrlUtilsJni);
    }

    @After
    public void tearDown() {
        AccessibilityState.setIsKnownScreenReaderEnabledForTesting(false);
    }

    @Nullable
    private MVCListAdapter.ListItem findItemById(MVCListAdapter.ModelList modelList, int id) {
        for (MVCListAdapter.ListItem listItem : modelList) {
            if (listItem.model.get(AppMenuItemProperties.MENU_ITEM_ID) == id) {
                return listItem;
            }
        }
        return null;
    }

    private void assertMenuItemsAreEqual(
            MVCListAdapter.ModelList modelList, Integer... expectedItems) {
        List<Integer> actualItems = new ArrayList<>();
        for (MVCListAdapter.ListItem item : modelList) {
            actualItems.add(item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        }

        assertThat(
                "Populated menu items were:" + getMenuTitles(modelList),
                actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private void assertHasSubMenuItemIds(
            MVCListAdapter.ListItem parentItem, Integer... expectedItems) {
        assertNotNull("Parent item is null", parentItem);
        assertTrue(
                "Parent item is not a submenu",
                parentItem.model.containsKey(AppMenuItemWithSubmenuProperties.SUBMENU_ITEMS));
        List<MVCListAdapter.ListItem> subItems =
                parentItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_ITEMS);
        assertNotNull("Submenu item list is null", subItems);

        List<Integer> actualItems = new ArrayList<>();
        for (MVCListAdapter.ListItem item : subItems) {
            actualItems.add(item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        }

        // Create a new ModelList and add the sub-items to it just for the error message.
        MVCListAdapter.ModelList subModelList = new MVCListAdapter.ModelList();
        if (subItems != null) {
            for (MVCListAdapter.ListItem item : subItems) {
                subModelList.add(item);
            }
        }

        assertThat(
                "Populated submenu items were:" + getMenuTitles(subModelList),
                actualItems,
                Matchers.contains(expectedItems));
    }

    private void assertMenuTitlesAreEqual(
            MVCListAdapter.ModelList modelList, Integer... expectedTitles) {
        Context context = ContextUtils.getApplicationContext();
        for (int i = 0; i < modelList.size(); i++) {
            MVCListAdapter.ListItem listItem = modelList.get(i);
            CharSequence title =
                    listItem.model.containsKey(AppMenuItemProperties.TITLE)
                            ? listItem.model.get(AppMenuItemProperties.TITLE)
                            : null;
            Assert.assertEquals(
                    expectedTitles[i] == 0 ? null : context.getString(expectedTitles[i]), title);
        }
    }

    private void assertActionBarItemsAreEqual(
            MVCListAdapter.ModelList modelList, Integer... expectedItems) {
        MVCListAdapter.ListItem iconRow = findItemById(modelList, R.id.icon_row_menu_id);
        assertNotNull(iconRow);
        List<Integer> actualItems = new ArrayList<>();
        for (MVCListAdapter.ListItem icon :
                iconRow.model.get(AppMenuItemProperties.ADDITIONAL_ICONS)) {
            actualItems.add(icon.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        }

        assertThat(
                "Populated action bar items were:"
                        + getMenuTitles(iconRow.model.get(AppMenuItemProperties.ADDITIONAL_ICONS)),
                actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private void assertMenuItemsHaveIcons(
            MVCListAdapter.ModelList modelList, Integer... expectedItems) {
        List<Integer> actualItems = new ArrayList<>();
        for (MVCListAdapter.ListItem item : modelList) {
            if (item.model.containsKey(AppMenuItemProperties.ICON)
                    && item.model.get(AppMenuItemProperties.ICON) != null) {
                actualItems.add(item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
            }
        }

        assertThat(
                "menu items with icons were:" + getMenuTitles(modelList),
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
    @EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
    public void testShouldShowIconRow_Tablet_MissingToolbarComponents() {
        doReturn(true).when(mToolbarManager).areAnyToolbarComponentsMissingForWidth(any());
        when(mDecorView.getWidth())
                .thenReturn(
                        (int)
                                (600
                                        * ContextUtils.getApplicationContext()
                                                .getResources()
                                                .getDisplayMetrics()
                                                .density));
        assertTrue(mTabbedAppMenuPropertiesDelegate.shouldShowIconRow());
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
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
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
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<Integer> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                R.id.icon_row_menu_id,
                                R.id.new_tab_menu_id,
                                R.id.new_incognito_tab_menu_id,
                                R.id.add_to_group_menu_id,
                                R.id.divider_line_id,
                                R.id.open_history_menu_id,
                                R.id.quick_delete_menu_id,
                                R.id.quick_delete_divider_line_id,
                                R.id.downloads_menu_id,
                                R.id.all_bookmarks_menu_id,
                                R.id.recent_tabs_menu_id,
                                R.id.divider_line_id,
                                R.id.preferences_id,
                                R.id.help_id));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(R.id.extensions_parent_menu_id);
        }
        assertMenuItemsAreEqual(modelList, expectedItems.toArray(new Integer[0]));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
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
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<Integer> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                R.id.icon_row_menu_id,
                                R.id.new_tab_menu_id,
                                R.id.new_incognito_tab_menu_id,
                                R.id.add_to_group_menu_id,
                                R.id.divider_line_id,
                                R.id.open_history_menu_id,
                                R.id.quick_delete_menu_id,
                                R.id.quick_delete_divider_line_id,
                                R.id.downloads_menu_id,
                                R.id.all_bookmarks_menu_id,
                                R.id.recent_tabs_menu_id,
                                R.id.divider_line_id,
                                R.id.share_menu_id,
                                R.id.find_in_page_id,
                                R.id.open_with_id,
                                R.id.divider_line_id,
                                R.id.preferences_id,
                                R.id.help_id));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(R.id.extensions_parent_menu_id);
        }
        assertMenuItemsAreEqual(modelList, expectedItems.toArray(new Integer[0]));
    }

    private void testPageMenuItems_RegularPage(boolean shouldShowNewIncognitoTab) {
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<Integer> expectedItems = new ArrayList<>();
        List<Integer> expectedTitles = new ArrayList<>();

        expectedItems.add(R.id.icon_row_menu_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.new_tab_menu_id);
        expectedTitles.add(R.string.menu_new_tab);

        if (shouldShowNewIncognitoTab) {
            expectedItems.add(R.id.new_incognito_tab_menu_id);
            expectedTitles.add(R.string.menu_new_incognito_tab);
        }

        expectedItems.add(R.id.add_to_group_menu_id);
        expectedTitles.add(R.string.menu_add_tab_to_new_group);
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
        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(R.id.extensions_parent_menu_id);
            expectedTitles.add(R.string.menu_extensions);
        }
        expectedItems.add(R.id.divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.share_menu_id);
        expectedTitles.add(R.string.menu_share_page);
        expectedItems.add(R.id.find_in_page_id);
        expectedTitles.add(R.string.menu_find_in_page);
        expectedItems.add(R.id.translate_id);
        expectedTitles.add(R.string.menu_translate);
        expectedItems.add(R.id.universal_install);
        expectedTitles.add(R.string.menu_add_to_homescreen);
        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(R.id.request_desktop_site_id);
            expectedTitles.add(R.string.menu_request_desktop_site);
        }
        expectedItems.add(R.id.auto_dark_web_contents_id);
        expectedTitles.add(R.string.menu_auto_dark_web_contents);
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
        assertMenuItemsAreEqual(modelList, expectedItems.toArray(new Integer[0]));
        assertMenuTitlesAreEqual(modelList, expectedTitles.toArray(new Integer[0]));
        assertActionBarItemsAreEqual(modelList, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testPageMenuItems_Phone_RegularPage() {
        testPageMenuItems_RegularPage(/* shouldShowNewIncognitoTab= */ true);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @EnableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testPageMenuItems_Phone_RegularPage_incognitoWindowEnabled() {
        testPageMenuItems_RegularPage(/* shouldShowNewIncognitoTab= */ false);
    }

    private void testPageMenuItems_IncognitoPage(boolean isIncognitoWindow) {
        setUpMocksForPageMenu();
        when(mTab.isIncognito()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        setMenuOptions(new MenuOptions().withShowTranslate().withAutoDarkEnabled());

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<Integer> expectedItems = new ArrayList<>();
        List<Integer> expectedTitles = new ArrayList<>();

        expectedItems.add(R.id.icon_row_menu_id);
        expectedTitles.add(0);

        if (!isIncognitoWindow) {
            expectedItems.add(R.id.new_tab_menu_id);
            expectedTitles.add(R.string.menu_new_tab);
        }

        expectedItems.add(R.id.new_incognito_tab_menu_id);
        expectedTitles.add(R.string.menu_new_incognito_tab);
        expectedItems.add(R.id.add_to_group_menu_id);
        expectedTitles.add(R.string.menu_add_tab_to_new_group);
        expectedItems.add(R.id.divider_line_id);
        expectedTitles.add(0);
        if (!isIncognitoWindow) {
            expectedItems.add(R.id.open_history_menu_id);
            expectedTitles.add(R.string.menu_history);
        }
        expectedItems.add(R.id.downloads_menu_id);
        expectedTitles.add(R.string.menu_downloads);
        expectedItems.add(R.id.all_bookmarks_menu_id);
        expectedTitles.add(R.string.menu_bookmarks);
        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(R.id.extensions_parent_menu_id);
            expectedTitles.add(R.string.menu_extensions);
        }
        expectedItems.add(R.id.divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.share_menu_id);
        expectedTitles.add(R.string.menu_share_page);
        expectedItems.add(R.id.find_in_page_id);
        expectedTitles.add(R.string.menu_find_in_page);
        expectedItems.add(R.id.translate_id);
        expectedTitles.add(R.string.menu_translate);
        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(R.id.request_desktop_site_id);
            expectedTitles.add(R.string.menu_request_desktop_site);
        }
        expectedItems.add(R.id.auto_dark_web_contents_id);
        expectedTitles.add(R.string.menu_auto_dark_web_contents);
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
        assertMenuItemsAreEqual(modelList, expectedItems.toArray(new Integer[0]));
        assertMenuTitlesAreEqual(modelList, expectedTitles.toArray(new Integer[0]));
        assertActionBarItemsAreEqual(modelList, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testPageMenuItems_Phone_IncognitoPage() {
        testPageMenuItems_IncognitoPage(/* isIncognitoWindow= */ false);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @EnableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testPageMenuItems_Phone_IncognitoPage_incognitoWindowEnabled() {
        testPageMenuItems_IncognitoPage(/* isIncognitoWindow= */ true);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testPageMenuItems_Phone_RegularPage_WithPwa() {
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<Integer> expectedItems = new ArrayList<>();
        List<Integer> expectedTitles = new ArrayList<>();

        expectedItems.add(R.id.icon_row_menu_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.new_tab_menu_id);
        expectedTitles.add(R.string.menu_new_tab);
        expectedItems.add(R.id.new_incognito_tab_menu_id);
        expectedTitles.add(R.string.menu_new_incognito_tab);
        expectedItems.add(R.id.add_to_group_menu_id);
        expectedTitles.add(R.string.menu_add_tab_to_new_group);
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
        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(R.id.extensions_parent_menu_id);
            expectedTitles.add(R.string.menu_extensions);
        }
        expectedItems.add(R.id.divider_line_id);
        expectedTitles.add(0);
        expectedItems.add(R.id.share_menu_id);
        expectedTitles.add(R.string.menu_share_page);
        expectedItems.add(R.id.find_in_page_id);
        expectedTitles.add(R.string.menu_find_in_page);
        expectedItems.add(R.id.translate_id);
        expectedTitles.add(R.string.menu_translate);
        expectedItems.add(R.id.universal_install);
        expectedTitles.add(R.string.menu_add_to_homescreen);
        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(R.id.request_desktop_site_id);
            expectedTitles.add(R.string.menu_request_desktop_site);
        }
        expectedItems.add(R.id.auto_dark_web_contents_id);
        expectedTitles.add(R.string.menu_auto_dark_web_contents);
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
        assertMenuItemsAreEqual(modelList, expectedItems.toArray(new Integer[0]));
        assertMenuTitlesAreEqual(modelList, expectedTitles.toArray(new Integer[0]));
        assertActionBarItemsAreEqual(modelList, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testPageMenuItems_DesktopAndroid() {
        mOverrideContextWrapperTestRule.setIsDesktop(true);
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<Integer> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                R.id.icon_row_menu_id,
                                R.id.new_tab_menu_id,
                                R.id.new_incognito_tab_menu_id,
                                R.id.add_to_group_menu_id,
                                R.id.divider_line_id,
                                R.id.open_history_menu_id,
                                R.id.quick_delete_menu_id,
                                R.id.quick_delete_divider_line_id,
                                R.id.downloads_menu_id,
                                R.id.all_bookmarks_menu_id,
                                R.id.recent_tabs_menu_id,
                                R.id.divider_line_id,
                                R.id.share_menu_id,
                                R.id.find_in_page_id,
                                R.id.translate_id,
                                R.id.universal_install,
                                // Request desktop site is hidden.
                                R.id.auto_dark_web_contents_id,
                                R.id.divider_line_id,
                                R.id.preferences_id,
                                R.id.help_id));
        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(R.id.extensions_parent_menu_id);
        }
        assertMenuItemsAreEqual(modelList, expectedItems.toArray(new Integer[0]));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItemsIcons_Phone_RegularPage_iconsAfterMenuItems() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions().withAllSet().setNativePage(false));
        doReturn(false).when(mTabbedAppMenuPropertiesDelegate).shouldShowIconBeforeItem();

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Integer[] expectedItems = {R.id.update_menu_id, R.id.reader_mode_prefs_id};
        assertMenuItemsHaveIcons(modelList, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
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
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<Integer> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                R.id.update_menu_id,
                                R.id.new_tab_menu_id,
                                R.id.new_incognito_tab_menu_id,
                                R.id.add_to_group_menu_id,
                                R.id.open_history_menu_id,
                                R.id.quick_delete_menu_id,
                                R.id.downloads_menu_id,
                                R.id.all_bookmarks_menu_id,
                                R.id.recent_tabs_menu_id,
                                R.id.translate_id,
                                R.id.share_menu_id,
                                R.id.find_in_page_id,
                                R.id.universal_install,
                                R.id.reader_mode_prefs_id,
                                R.id.auto_dark_web_contents_id,
                                R.id.preferences_id,
                                R.id.help_id));
        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(R.id.extensions_parent_menu_id);
        }
        assertMenuItemsHaveIcons(modelList, expectedItems.toArray(new Integer[0]));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures({
        ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID,
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW
    })
    public void testOverviewMenuItems_Phone_SelectTabs() {
        setUpMocksForOverviewMenu();
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Integer[] expectedItems = {
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.close_all_tabs_menu_id,
            R.id.menu_select_tabs,
            R.id.quick_delete_menu_id,
            R.id.preferences_id
        };
        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @DisableFeatures({ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID})
    @EnableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOverviewMenuItems_Tablet_SelectTabs_incognitoWindowEnabled() {
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        setUpIncognitoMocks();
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isMultiInstanceRunning()).thenReturn(false);

        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Integer[] expectedItems = {
            R.id.new_tab_menu_id,
            R.id.new_window_menu_id,
            R.id.new_incognito_window_menu_id,
            R.id.close_all_tabs_menu_id,
            R.id.menu_select_tabs,
            R.id.quick_delete_menu_id,
            R.id.preferences_id
        };
        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures({
        ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID,
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW
    })
    public void testOverviewMenuItems_Phone_IncognitoWindow() {
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        setUpIncognitoMocks();

        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Integer[] expectedItems = {
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.close_all_incognito_tabs_menu_id,
            R.id.menu_select_tabs,
            R.id.preferences_id
        };
        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @DisableFeatures({ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID})
    @EnableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOverviewMenuItems_Tablet_IncognitoWindow_incognitoWindowEnabled() {
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        setUpIncognitoMocks();
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isMultiInstanceRunning()).thenReturn(false);

        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Integer[] expectedItems = {
            R.id.new_incognito_tab_menu_id,
            R.id.new_window_menu_id,
            R.id.new_incognito_window_menu_id,
            R.id.close_all_incognito_tabs_menu_id,
            R.id.menu_select_tabs,
            R.id.preferences_id
        };
        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures({
        ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID,
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW
    })
    public void testOverviewMenuItems_Phone_NoTabs() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getTotalTabCount()).thenReturn(0);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Integer[] expectedItems = {
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.close_all_tabs_menu_id,
            R.id.menu_select_tabs,
            R.id.quick_delete_menu_id,
            R.id.preferences_id
        };
        assertMenuItemsAreEqual(modelList, expectedItems);
        PropertyModel closeAllTabsModel = modelList.get(2).model;
        assertEquals(
                R.id.close_all_tabs_menu_id,
                closeAllTabsModel.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertFalse(closeAllTabsModel.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures({
        ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID,
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW
    })
    public void testOverviewMenuItems_Phone_NoIncognitoTabs() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Integer[] expectedItems = {
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.close_all_incognito_tabs_menu_id,
            R.id.menu_select_tabs,
            R.id.preferences_id
        };
        assertMenuItemsAreEqual(modelList, expectedItems);
        PropertyModel closeAllTabsModel = modelList.get(2).model;
        assertEquals(
                R.id.close_all_incognito_tabs_menu_id,
                closeAllTabsModel.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertFalse(closeAllTabsModel.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOverviewMenuItems_Phone_SelectTabs_tabGroupEntryPointsFeatureEnabled() {
        setUpMocksForOverviewMenu();
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Integer[] expectedItems = {
            R.id.new_tab_menu_id,
            R.id.new_incognito_tab_menu_id,
            R.id.new_tab_group_menu_id,
            R.id.close_all_tabs_menu_id,
            R.id.menu_select_tabs,
            R.id.quick_delete_menu_id,
            R.id.preferences_id
        };
        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    private void checkOverviewMenuItems(boolean newIncognitoWindowEnabled) {
        setUpIncognitoMocks();
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        when(mTabModel.getCount()).thenReturn(0);

        assertEquals(
                MenuGroup.TABLET_EMPTY_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<Integer> expectedItems = new ArrayList<>(List.of(R.id.new_tab_menu_id));

        if (newIncognitoWindowEnabled) {
            expectedItems.add(R.id.new_window_menu_id);
            expectedItems.add(R.id.new_incognito_window_menu_id);
        } else {
            expectedItems.add(R.id.new_incognito_tab_menu_id);
        }

        expectedItems.add(R.id.preferences_id);
        expectedItems.add(R.id.quick_delete_menu_id);

        assertMenuItemsAreEqual(modelList, expectedItems.toArray(new Integer[0]));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOverviewMenuItems_Tablet_NoTabs() {
        checkOverviewMenuItems(/* newIncognitoWindowEnabled= */ false);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOverviewMenuItems_Tablet_NoTabs_withNewIncognitoWindow() {
        checkOverviewMenuItems(/* newIncognitoWindowEnabled= */ true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testMenuItems_Accessibility_ImageDescriptions() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        when(mTab.isNativePage()).thenReturn(false);
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class));
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowTranslateMenuItem(any(Tab.class));

        // Ensure the get image descriptions option is shown as needed
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);

        // Test specific setup
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        AccessibilityState.setIsKnownScreenReaderEnabledForTesting(true);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        ArrayList<Integer> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                R.id.icon_row_menu_id,
                                R.id.new_tab_menu_id,
                                R.id.new_incognito_tab_menu_id,
                                R.id.add_to_group_menu_id,
                                R.id.divider_line_id,
                                R.id.open_history_menu_id,
                                R.id.quick_delete_menu_id,
                                R.id.quick_delete_divider_line_id,
                                R.id.downloads_menu_id,
                                R.id.all_bookmarks_menu_id,
                                R.id.recent_tabs_menu_id,
                                R.id.divider_line_id,
                                R.id.share_menu_id,
                                R.id.get_image_descriptions_id,
                                R.id.find_in_page_id,
                                R.id.universal_install,
                                R.id.auto_dark_web_contents_id,
                                R.id.divider_line_id,
                                R.id.preferences_id,
                                R.id.help_id));
        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(R.id.request_desktop_site_id);
        }
        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(R.id.extensions_parent_menu_id);
        }

        assertMenuItemsAreEqual(modelList, expectedItems.toArray(new Integer[0]));

        // Ensure the text of the menu item is correct
        assertEquals(
                "Get image descriptions",
                findItemById(modelList, R.id.get_image_descriptions_id)
                        .model
                        .get(AppMenuItemProperties.TITLE));

        // Enable the feature and ensure text changes
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(true);

        modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertEquals(
                "Stop image descriptions",
                findItemById(modelList, R.id.get_image_descriptions_id)
                        .model
                        .get(AppMenuItemProperties.TITLE));

        // Setup no wifi condition, and "only on wifi" user option.
        DeviceConditions noWifi =
                new DeviceConditions(false, 75, ConnectionType.CONNECTION_2G, false, false, true);
        DeviceConditions.setForTesting(noWifi);
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI))
                .thenReturn(true);

        modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertEquals(
                "Get image descriptions",
                findItemById(modelList, R.id.get_image_descriptions_id)
                        .model
                        .get(AppMenuItemProperties.TITLE));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testPageMenuItems_Phone_RegularPage_managed_users() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowManagedByMenuItem(any(Tab.class));

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        ArrayList<Integer> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                R.id.icon_row_menu_id,
                                R.id.new_tab_menu_id,
                                R.id.new_incognito_tab_menu_id,
                                R.id.add_to_group_menu_id,
                                R.id.divider_line_id,
                                R.id.open_history_menu_id,
                                R.id.quick_delete_menu_id,
                                R.id.quick_delete_divider_line_id,
                                R.id.downloads_menu_id,
                                R.id.all_bookmarks_menu_id,
                                R.id.recent_tabs_menu_id,
                                R.id.divider_line_id,
                                R.id.share_menu_id,
                                R.id.find_in_page_id,
                                R.id.universal_install,
                                R.id.auto_dark_web_contents_id,
                                R.id.divider_line_id,
                                R.id.preferences_id,
                                R.id.help_id,
                                R.id.managed_by_divider_line_id,
                                R.id.managed_by_menu_id));

        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(R.id.request_desktop_site_id);
        }
        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(R.id.extensions_parent_menu_id);
        }

        assertMenuItemsAreEqual(modelList, expectedItems.toArray(new Integer[0]));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testPageMenuItems_Phone_RegularPage_locally_supervised_users() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowContentFilterHelpCenterMenuItem(any(Tab.class));
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowManagedByMenuItem(any(Tab.class));

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        // TODO(crbug.com/427240031): Stop asserting on menu items that are not subject of this
        // test.
        ArrayList<Integer> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                R.id.icon_row_menu_id,
                                R.id.new_tab_menu_id,
                                R.id.new_incognito_tab_menu_id,
                                R.id.add_to_group_menu_id,
                                R.id.divider_line_id,
                                R.id.open_history_menu_id,
                                R.id.quick_delete_menu_id,
                                R.id.quick_delete_divider_line_id,
                                R.id.downloads_menu_id,
                                R.id.all_bookmarks_menu_id,
                                R.id.recent_tabs_menu_id,
                                R.id.divider_line_id,
                                R.id.share_menu_id,
                                R.id.find_in_page_id,
                                R.id.universal_install,
                                R.id.auto_dark_web_contents_id,
                                R.id.divider_line_id,
                                R.id.preferences_id,
                                R.id.help_id,
                                R.id.menu_item_content_filter_divider_line_id,
                                R.id.menu_item_content_filter_help_center_id));

        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(R.id.request_desktop_site_id);
        }
        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(R.id.extensions_parent_menu_id);
        }

        assertMenuItemsAreEqual(modelList, expectedItems.toArray(new Integer[0]));
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
        MVCListAdapter.ModelList modelList = createMenuForMultiWindow();
        assertFalse(isMenuVisible(modelList, R.id.new_window_menu_id));

        // Multi-window mode, with a single instance (no adjacent instance running) makes
        // the menu visible.
        doReturn(false).when(mMultiWindowModeStateDispatcher).isChromeRunningInAdjacentWindow();
        mIsMultiWindow = true;

        modelList = createMenuForMultiWindow();
        assertTrue(isMenuVisible(modelList, R.id.new_window_menu_id));

        // On tablet, we show 'New Window' by default.
        mIsTabletScreen = true;
        mIsMultiWindow = false;
        modelList = createMenuForMultiWindow();
        assertTrue(isMenuVisible(modelList, R.id.new_window_menu_id));

        for (int i = 0; i < MultiWindowUtils.getMaxInstances(); ++i) {
            createInstance(i, "https://url" + i);
        }

        MVCListAdapter.ModelList modelList2 = createMenuForMultiWindow();
        assertFalse(isMenuVisible(modelList2, R.id.new_window_menu_id));
    }

    @Test
    public void testPageMenuItems_instanceSwitcher_moveTabToOtherWindow() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);

        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .instanceSwitcherWithMultiInstanceEnabled();
        mIsMoveToOtherWindowSupported = true;

        MVCListAdapter.ModelList modelList = createMenuForMultiWindow();
        assertTrue(isMenuVisible(modelList, R.id.move_to_other_window_menu_id));
    }

    @Test
    public void testPageMenuItems_instanceSwitcher_manageAllWindow() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);

        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .instanceSwitcherWithMultiInstanceEnabled();

        createInstance(0, "https://url0");

        MVCListAdapter.ModelList modelList = createMenuForMultiWindow();
        assertFalse(isMenuVisible(modelList, R.id.manage_all_windows_menu_id));

        createInstance(1, "https://url1");

        MVCListAdapter.ModelList modelList2 = createMenuForMultiWindow();
        assertTrue(isMenuVisible(modelList2, R.id.manage_all_windows_menu_id));
    }

    @Test
    public void testPageMenuItems_universalInstall() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        MVCListAdapter.ModelList modelList = createMenuForMultiWindow();
        assertTrue(isMenuVisible(modelList, R.id.universal_install));
        assertFalse(isMenuVisible(modelList, R.id.open_webapk_id));
    }

    @Test
    public void managedByMenuItem_ChromeManagementPage() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions().withShowAddToHomeScreen());
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowManagedByMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertTrue(isMenuVisible(modelList, R.id.managed_by_menu_id));
    }

    @Test
    public void contentFilterHelpCenterItem_ChromeManagementPage() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions().withShowAddToHomeScreen());
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowContentFilterHelpCenterMenuItem(any(Tab.class));

        Assert.assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertTrue(isMenuVisible(modelList, R.id.menu_item_content_filter_help_center_id));
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

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        verify(mIncognitoReauthControllerMock).isReauthPageShowing();

        MVCListAdapter.ListItem item = findItemById(modelList, R.id.new_incognito_tab_menu_id);
        assertFalse(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testNewIncognitoTabOption_FromRegularMode_WithReauthNotInProgress() {
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());

        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        verifyNoMoreInteractions(mIncognitoReauthControllerMock);

        MVCListAdapter.ListItem item = findItemById(modelList, R.id.new_incognito_tab_menu_id);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_IMPROVEMENTS + ":always_on_entry_point/false")
    public void readerModeEntryPointDisabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(isMenuVisible(modelList, R.id.reader_mode_menu_id));
    }

    @Test
    @DisableFeatures(DomDistillerFeatures.READER_MODE_IMPROVEMENTS + ":always_on_entry_point/false")
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void readerModeEntryPointEnabledWhenDistillingInApp() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertTrue(isMenuVisible(modelList, R.id.reader_mode_menu_id));
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_IMPROVEMENTS + ":always_on_entry_point/true")
    public void readerModeEntryPointEnabled_ShowReadingMode() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(false);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Context context = ContextUtils.getApplicationContext();
        assertTrue(
                isMenuVisibleWithCorrectTitle(
                        modelList,
                        R.id.reader_mode_menu_id,
                        context.getString(R.string.show_reading_mode_text)));
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_IMPROVEMENTS + ":always_on_entry_point/true")
    public void readerModeEntryPointEnabled_HideReadingMode() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.CHROME_DISTILLER_EXAMPLE_URL);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Context context = ContextUtils.getApplicationContext();
        assertTrue(
                isMenuVisibleWithCorrectTitle(
                        modelList,
                        R.id.reader_mode_menu_id,
                        context.getString(R.string.hide_reading_mode_text)));
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void readerModeEntryPointEnabled_chromePage() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(new GURL(UrlConstants.NTP_URL));

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Context context = ContextUtils.getApplicationContext();
        assertFalse(
                isMenuVisibleWithCorrectTitle(
                        modelList,
                        R.id.reader_mode_menu_id,
                        context.getString(R.string.hide_reading_mode_text)));
    }

    @Test
    public void pageZoomMenuOption_NotVisibleInReadingMode() {
        setUpMocksForPageMenu();
        PageZoomUtils.setShouldShowMenuItemForTesting(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.CHROME_DISTILLER_EXAMPLE_URL);
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(isMenuVisible(modelList, R.id.page_zoom_id));
    }

    private MVCListAdapter.ModelList setUpMenuWithIncognitoReauthPage(boolean isShowing) {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        prepareMocksForGroupTabsOnTabModel(mIncognitoTabModel);
        doReturn(isShowing).when(mIncognitoReauthControllerMock).isReauthPageShowing();

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        verify(mIncognitoReauthControllerMock, atLeastOnce()).isReauthPageShowing();
        return modelList;
    }

    @Test
    public void testSelectTabsOption_IsEnabled_InIncognitoMode_When_IncognitoReauthIsNotShowing() {
        MVCListAdapter.ModelList modelList =
                setUpMenuWithIncognitoReauthPage(/* isShowing= */ false);
        MVCListAdapter.ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testSelectTabsOption_IsDisabled_InIncognitoMode_When_IncognitoReauthIsShowing() {
        MVCListAdapter.ModelList modelList =
                setUpMenuWithIncognitoReauthPage(/* isShowing= */ true);
        MVCListAdapter.ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertFalse(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testSelectTabsOption_IsEnabled_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        prepareMocksForGroupTabsOnTabModel(mTabModel);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MVCListAdapter.ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testSelectTabsOption_IsDisabled_InRegularMode_TabStateNotInitialized() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        prepareMocksForGroupTabsOnTabModel(mTabModel);

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MVCListAdapter.ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertFalse(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testSelectTabsOption_IsEnabledOneTab_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(1);
        Tab mockTab1 = mock(Tab.class);
        when(mTabModel.getTabAt(0)).thenReturn(mockTab1);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MVCListAdapter.ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testSelectTabsOption_IsDisabled_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        MVCListAdapter.ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertFalse(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION})
    public void testCustomizeNewTabPageOption() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(UrlConstants.NTP_URL));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        when(mActivityTabProvider.get()).thenReturn(ntpTab);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        MVCListAdapter.ListItem item = findItemById(modelList, R.id.ntp_customization_id);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FEED_AUDIO_OVERVIEWS})
    public void testListenToFeedMenuItem_available() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(UrlConstants.NTP_URL));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        when(mActivityTabProvider.get()).thenReturn(ntpTab);
        when(mReadAloudController.isAvailable()).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.listen_to_feed_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FEED_AUDIO_OVERVIEWS})
    public void testListenToFeedMenuItem_unavailableWhenNotNtp() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(UrlConstants.NTP_URL));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mReadAloudController.isAvailable()).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.listen_to_feed_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FEED_AUDIO_OVERVIEWS})
    public void testListenToFeedMenuItem_unavailableWhenFeedDisabled() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(UrlConstants.NTP_URL));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        when(mActivityTabProvider.get()).thenReturn(ntpTab);
        when(mReadAloudController.isAvailable()).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.listen_to_feed_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FEED_AUDIO_OVERVIEWS})
    public void testListenToFeedMenuItem_unavailableWhenFeedHidden() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(UrlConstants.NTP_URL));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        when(mActivityTabProvider.get()).thenReturn(ntpTab);
        when(mReadAloudController.isAvailable()).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.listen_to_feed_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FEED_AUDIO_OVERVIEWS})
    public void testListenToFeedMenuItem_unavailableWhenReadAloudNotAvailable() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(UrlConstants.NTP_URL));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        when(mActivityTabProvider.get()).thenReturn(ntpTab);
        when(mReadAloudController.isAvailable()).thenReturn(false);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.listen_to_feed_id));
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
        for (int i = 0; i < currentWindowInstances; ++i) {
            createInstance(i, "https://url" + i);
        }
        mShadowPackageManager.setSystemFeature(PackageManager.FEATURE_AUTOMOTIVE, isAutomotive);
        doReturn(isInstanceSwitcherEnabled)
                .when(mTabbedAppMenuPropertiesDelegate)
                .instanceSwitcherWithMultiInstanceEnabled();
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
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
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
        assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowNewIncognitoWindow());
        verify(mTabbedAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testShouldShowNewMenu_isTabletSizedScreen_returnsTrue_withNewIncognitoWindow() {
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
        assertTrue(mTabbedAppMenuPropertiesDelegate.shouldShowNewIncognitoWindow());
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
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.readaloud_menu_id));
    }

    @Test
    public void testReadAloudMenuItem_notReadable() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mReadAloudController.isReadable(any())).thenReturn(false);
        setUpMocksForPageMenu();
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.readaloud_menu_id));
    }

    @Test
    public void testReadAloudMenuItem_readable() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mReadAloudController.isReadable(any())).thenReturn(true);
        setUpMocksForPageMenu();
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.readaloud_menu_id));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)
    public void testAiWebMenuItem_shouldAppearOnWebPages() {
        var aiAssistantService = mock(AiAssistantService.class);
        AiAssistantService.setInstanceForTesting(aiAssistantService);
        when(aiAssistantService.canShowAiForTab(any(), eq(mTab))).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        setUpMocksForPageMenu();

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertTrue(
                "AI Web menu item should be visible",
                isMenuVisible(modelList, R.id.ai_web_menu_id));
        assertFalse(
                "AI PDF menu item should not be visible",
                isMenuVisible(modelList, R.id.ai_pdf_menu_id));
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

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(
                "AI Web menu item should not be visible",
                isMenuVisible(modelList, R.id.ai_web_menu_id));
        assertTrue(
                "AI PDF menu item should be visible",
                isMenuVisible(modelList, R.id.ai_pdf_menu_id));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)
    public void testAiMenuItems_shouldNotAppearIfDisabled() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        setUpMocksForPageMenu();

        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(
                "AI Web menu item should not be visible",
                isMenuVisible(modelList, R.id.ai_web_menu_id));
        assertFalse(
                "AI PDF menu item should not be visible",
                isMenuVisible(modelList, R.id.ai_pdf_menu_id));
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
        mTabbedAppMenuPropertiesDelegate.getMenuItems();
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
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_ExtensionsSubmenu() {
        if (!ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            return;
        }

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).shouldShowIconBeforeItem();

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        MVCListAdapter.ListItem parentItem =
                findItemById(modelList, R.id.extensions_parent_menu_id);
        assertNotNull("Extensions parent menu item not found.", parentItem);

        assertEquals(
                ContextUtils.getApplicationContext().getString(R.string.menu_extensions),
                parentItem.model.get(AppMenuItemProperties.TITLE));
        assertNotNull(
                "Parent extension item should have an icon.",
                parentItem.model.get(AppMenuItemProperties.ICON));

        assertHasSubMenuItemIds(
                parentItem, R.id.extensions_menu_id, R.id.extensions_webstore_menu_id);

        List<MVCListAdapter.ListItem> subItems =
                parentItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_ITEMS);

        MVCListAdapter.ListItem manageItem = subItems.get(0);
        assertEquals(
                R.id.extensions_menu_id, manageItem.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                ContextUtils.getApplicationContext().getString(R.string.menu_manage_extensions),
                manageItem.model.get(AppMenuItemProperties.TITLE));
        assertNotNull(
                "Manage Extensions item should have an icon.",
                manageItem.model.get(AppMenuItemProperties.ICON));

        MVCListAdapter.ListItem webstoreItem = subItems.get(1);
        assertEquals(
                R.id.extensions_webstore_menu_id,
                webstoreItem.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                ContextUtils.getApplicationContext().getString(R.string.menu_chrome_webstore),
                webstoreItem.model.get(AppMenuItemProperties.TITLE));
        assertNotNull(
                "Visit Chrome Web Store item should have an icon.",
                webstoreItem.model.get(AppMenuItemProperties.ICON));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testPageMenuItems_ExtensionsItem_SubmenusDisabled() {
        if (!ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            return;
        }

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        MVCListAdapter.ListItem parentItem =
                findItemById(modelList, R.id.extensions_parent_menu_id);
        assertNull("Extensions parent menu item should NOT be present.", parentItem);

        MVCListAdapter.ListItem originalItem = findItemById(modelList, R.id.extensions_menu_id);
        assertNotNull("Original extensions menu item should be present.", originalItem);

        assertFalse(
                "Original extensions item should not have submenu properties.",
                originalItem.model.containsKey(AppMenuItemWithSubmenuProperties.SUBMENU_ITEMS));
        assertEquals(
                ContextUtils.getApplicationContext().getString(R.string.menu_extensions),
                originalItem.model.get(AppMenuItemProperties.TITLE));
    }

    @Test
    public void getFooterResourceId_incognito_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mTab.isIncognito()).thenReturn(true);

        assertThat(
                "Footer should not be a WebFeed footer",
                mTabbedAppMenuPropertiesDelegate.buildFooterView(mAppMenuHandler),
                anyOf(nullValue(), not(instanceOf(WebFeedMainMenuItem.class))));
    }

    @Test
    public void getFooterResourceId_offlinePage_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mOfflinePageUtils.isOfflinePage(mTab)).thenReturn(true);

        assertThat(
                "Footer should not be a WebFeed footer",
                mTabbedAppMenuPropertiesDelegate.buildFooterView(mAppMenuHandler),
                anyOf(nullValue(), not(instanceOf(WebFeedMainMenuItem.class))));
    }

    @Test
    public void getFooterResourceId_nonHttpUrl_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mTab.getOriginalUrl()).thenReturn(JUnitTestGURLs.NTP_URL);

        assertThat(
                "Footer should not be a WebFeed footer",
                mTabbedAppMenuPropertiesDelegate.buildFooterView(mAppMenuHandler),
                anyOf(nullValue(), not(instanceOf(WebFeedMainMenuItem.class))));
    }

    @Test
    public void getFooterResourceId_signedOutUser_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);

        assertThat(
                "Footer should not be a WebFeed footer",
                mTabbedAppMenuPropertiesDelegate.buildFooterView(mAppMenuHandler),
                anyOf(nullValue(), not(instanceOf(WebFeedMainMenuItem.class))));
    }

    @Test
    public void getFooterResourceId_httpsUrl_returnsWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();

        assertThat(
                "Footer should be a WebFeed footer",
                mTabbedAppMenuPropertiesDelegate.buildFooterView(mAppMenuHandler),
                instanceOf(WebFeedMainMenuItem.class));
    }

    @Test
    public void getFooterResourceId_dseOff_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(false);

        assertThat(
                "Footer should not be a WebFeed footer",
                mTabbedAppMenuPropertiesDelegate.buildFooterView(mAppMenuHandler),
                anyOf(nullValue(), not(instanceOf(WebFeedMainMenuItem.class))));
    }

    @Test
    public void getFooterResourceId_dseOn_returnsWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);

        assertThat(
                "Footer should be a WebFeed footer",
                mTabbedAppMenuPropertiesDelegate.buildFooterView(mAppMenuHandler),
                instanceOf(WebFeedMainMenuItem.class));
    }

    @Test
    public void getFooterResourceId_signedOutUser_dseOn_doesNotReturnWebFeedMenuItem() {
        setUpMocksForWebFeedFooter();
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);

        assertThat(
                "Footer should not be a WebFeed footer",
                mTabbedAppMenuPropertiesDelegate.buildFooterView(mAppMenuHandler),
                anyOf(nullValue(), not(instanceOf(WebFeedMainMenuItem.class))));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void testAddToGroup() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.add_to_group_menu_id));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void testAddToGroup_noParity() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.add_to_group_menu_id));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    @DisableFeatures(ChromeFeatureList.TAB_MODEL_INIT_FIXES)
    public void testAddToGroup_preInitNoFixes() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.add_to_group_menu_id));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID,
        ChromeFeatureList.TAB_MODEL_INIT_FIXES
    })
    public void testAddToGroup_preInit() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        MVCListAdapter.ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.add_to_group_menu_id));
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
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowContentFilterHelpCenterMenuItem(any(Tab.class));
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowAutoDarkItem(any(Tab.class), eq(false));
        doReturn(false)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowAutoDarkItem(any(Tab.class), eq(true));

        setUpIncognitoMocks();
    }

    private MVCListAdapter.ModelList createMenuForMultiWindow() {
        doReturn(mIsMultiWindow).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        doReturn(mIsMultiWindowApiSupported)
                .when(mMultiWindowModeStateDispatcher)
                .canEnterMultiWindowMode();
        doReturn(mIsMultiInstance).when(mMultiWindowModeStateDispatcher).isMultiInstanceRunning();
        doReturn(mIsTabletScreen).when(mTabbedAppMenuPropertiesDelegate).isTabletSizeScreen();
        doReturn(mIsMoveToOtherWindowSupported)
                .when(mMultiWindowModeStateDispatcher)
                .isMoveToOtherWindowSupported(mTabModelSelector);
        return mTabbedAppMenuPropertiesDelegate.getMenuItems();
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
                MVCListAdapter.ModelList modelList = createMenuForMultiWindow();
                if (showNewWindow != null) {
                    if (showNewWindow) {
                        assertTrue(getFlags(), isMenuVisible(modelList, R.id.new_window_menu_id));
                    } else {
                        assertFalse(getFlags(), isMenuVisible(modelList, R.id.new_window_menu_id));
                    }
                }
                if (showMoveWindow != null) {
                    if (showMoveWindow) {
                        assertTrue(
                                getFlags(),
                                isMenuVisible(modelList, R.id.move_to_other_window_menu_id));
                    } else {
                        assertFalse(
                                getFlags(),
                                isMenuVisible(modelList, R.id.move_to_other_window_menu_id));
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

    private boolean isMenuVisible(MVCListAdapter.ModelList modelList, int itemId) {
        return findItemById(modelList, itemId) != null;
    }

    private boolean isMenuVisibleWithCorrectTitle(
            MVCListAdapter.ModelList modelList, int itemId, String expectedTitle) {
        MVCListAdapter.ListItem menuItem = findItemById(modelList, itemId);
        if (menuItem == null) return false;
        return menuItem.model.get(AppMenuItemProperties.TITLE) == expectedTitle;
    }

    private String getMenuTitles(MVCListAdapter.ModelList modelList) {
        StringBuilder items = new StringBuilder();
        for (MVCListAdapter.ListItem item : modelList) {
            CharSequence title =
                    item.model.containsKey(AppMenuItemProperties.TITLE)
                            ? item.model.get(AppMenuItemProperties.TITLE)
                            : null;
            if (title == null) {
                if (item.type == AppMenuHandler.AppMenuItemType.BUTTON_ROW) {
                    title = "Icon Row";
                } else if (item.type == AppMenuHandler.AppMenuItemType.DIVIDER) {
                    title = "Divider";
                }
            }
            items.append("\n")
                    .append(title)
                    .append(":")
                    .append(item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
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
        doReturn(options.showUpdate() ? mUpdateAvailableMenuUiState : new MenuUiState())
                .when(mUpdateMenuItemHelper)
                .getUiState();
        doReturn(options.showMoveToOtherWindow())
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowMoveToOtherWindow();
        doReturn(options.showReaderModePrefs())
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowReaderModePrefs(any(Tab.class));
        doReturn(options.showPaintPreview())
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class));
        when(mWebsitePreferenceBridgeJniMock.getContentSetting(any(), anyInt(), any(), any()))
                .thenReturn(
                        options.isAutoDarkEnabled()
                                ? ContentSetting.DEFAULT
                                : ContentSetting.BLOCK);
    }
}
