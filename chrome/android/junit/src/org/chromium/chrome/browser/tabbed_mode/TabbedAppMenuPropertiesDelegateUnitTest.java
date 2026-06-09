// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

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
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.annotation.Nullable;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl.MenuGroup;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feedback.FeedbackPolicyManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtilsJni;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedGroup;
import org.chromium.chrome.browser.ntp.RecentlyClosedTab;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
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
import org.chromium.chrome.browser.ui.appmenu.AppMenuRecentEntryItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTabItemProperties;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.extensions.ExtensionsBuildflags;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionUiBackendRule;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.lens.LensOverlayTabHelper;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
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
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;
import org.chromium.net.ConnectionType;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.function.BiConsumer;

/** Unit tests for {@link TabbedAppMenuPropertiesDelegate}. */
// TODO(crbug.com/376238770): Removes ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION from
// @DisableFeatures() and adds "Customize New Tab Page" to all expectedItems list once the feature
// flag is turned on by default.

@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.FEED_AUDIO_OVERVIEWS,
    ChromeFeatureList.LENS_OVERLAY_ANDROID,
    ChromeFeatureList.TASK_MANAGER_CLANK,
    ContentFeatureList.ANDROID_DEV_TOOLS_FRONTEND,
    DomDistillerFeatures.READER_MODE_DISTILL_IN_APP,
    // TODO(crbug.com/504757384): Add test for three dot menu flag.
    ChromeFeatureList.THREE_DOT_MENU_BACK_BUTTON,
    TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS
})
@EnableFeatures({
    ChromeFeatureList.SUBMENUS_IN_APP_MENU,
    ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM
})
public class TabbedAppMenuPropertiesDelegateUnitTest {
    // Constants defining flags that determines multi-window menu items visibility.
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

    @Rule public TestName mTestName = new TestName();

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
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private Profile mProfile;
    @Mock private AppMenuDelegate mAppMenuDelegate;
    @Mock private ModalDialogManager mDialogManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private OfflinePageUtils.Internal mOfflinePageUtils;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private IdentityServicesProvider mIdentityService;
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
    @Mock private TranslateBridge.Natives mTranslateBridgeJniMock;
    @Mock private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock private LargeIconBridge.Natives mLargeIconBridgeJni;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private PageZoomManager mPageZoomManagerMock;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;
    @Mock private HubManager mHubManager;
    @Mock private PaneManager mPaneManager;
    @Mock private Pane mPane;
    @Mock private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private FeedbackPolicyManager mFeedbackPolicyManager;
    @Mock private RecentlyClosedEntriesManager mRecentlyClosedEntriesManager;

    private ShadowPackageManager mShadowPackageManager;

    private FakeBookmarkModel mBookmarkModel;
    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();
    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<IncognitoReauthController>
            mIncognitoReauthControllerSupplier = new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<HubManager> mHubManagerSupplier = new OneshotSupplierImpl<>();
    private final SettableMonotonicObservableSupplier<BookmarkModel> mBookmarkModelSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<ReadAloudController>
            mReadAloudControllerSupplier = ObservableSuppliers.createMonotonic();

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

    // Represents a hierarchical menu item used for structural testing and assertions.
    public static class MenuItem {
        public final Object property;
        public final MenuItem[] children;

        public MenuItem(Object property, MenuItem... children) {
            this.property = property;
            this.children = children;
        }
    }

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        mShadowPackageManager = Shadows.shadowOf(context.getPackageManager());

        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());

        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        mIncognitoReauthControllerSupplier.set(mIncognitoReauthControllerMock);
        if (!mTestName.getMethodName().equals("testReadAloudMenuItem_readAloudNotEnabled")) {
            mReadAloudControllerSupplier.set(mReadAloudController);
        }
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        ManagedBrowserUtilsJni.setInstanceForTesting(mManagedBrowserUtilsJniMock);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        WebsitePreferenceBridgeJni.setInstanceForTesting(mWebsitePreferenceBridgeJniMock);
        OfflinePageUtils.setInstanceForTesting(mOfflinePageUtils);
        when(mIdentityService.getSigninManager(any(Profile.class))).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        IdentityServicesProvider.setInstanceForTests(mIdentityService);
        when(mIdentityService.getIdentityManager(any(Profile.class))).thenReturn(mIdentityManager);
        when(mIdentityManager.hasPrimaryAccount()).thenReturn(true);
        PageZoomUtils.setShouldShowMenuItemForTesting(false);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        FeedServiceBridgeJni.setInstanceForTesting(mFeedServiceBridgeJniMock);
        when(mSyncService.getAuthError())
                .thenReturn(new GoogleServiceAuthError(GoogleServiceAuthErrorState.NONE));
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isTrustedVaultRecoverabilityDegraded()).thenReturn(false);
        AppBannerManagerJni.setInstanceForTesting(mAppBannerManagerJniMock);
        Mockito.when(mAppBannerManagerJniMock.getInstallableWebAppManifestId(any()))
                .thenReturn(null);
        UserPrefsJni.setInstanceForTesting(mUserPrefsNatives);
        FeedbackPolicyManager.setInstanceForTesting(mFeedbackPolicyManager);
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);
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

        mHubManagerSupplier.set(mHubManager);
        when(mHubManager.getPaneManager()).thenReturn(mPaneManager);
        when(mPaneManager.getFocusedPaneSupplier())
                .thenReturn(ObservableSuppliers.createMonotonic(mPane));
        when(mPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);

        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        PowerBookmarkUtils.setPowerBookmarkMetaForTesting(PowerBookmarkMeta.newBuilder().build());

        mBookmarkModel = FakeBookmarkModel.createModel();
        mBookmarkModel.setEditBookmarksEnabled(true);
        mBookmarkModelSupplier.set(mBookmarkModel);
        mBookmarkModel.addBookmark(
                mBookmarkModel.getDesktopFolderId(), 0, "Bookmark 1", JUnitTestGURLs.URL_1);
        mBookmarkModel.addBookmark(
                mBookmarkModel.getDesktopFolderId(), 1, "Bookmark 2", JUnitTestGURLs.URL_2);
        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 2, "Folder 1");
        mBookmarkModel.addBookmark(folderId, 0, "Bookmark in folder 1", JUnitTestGURLs.URL_3);
        mBookmarkModel.addBookmark(folderId, 1, "Bookmark in folder 2", JUnitTestGURLs.SEARCH_URL);

        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);

        when(mRecentlyClosedEntriesManager.getRecentlyClosedEntries())
                .thenReturn(new ArrayList<>());

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
                        mDialogManager,
                        mSnackbarManager,
                        mIncognitoReauthControllerSupplier,
                        mReadAloudControllerSupplier,
                        mPageZoomManagerMock,
                        mHubManagerSupplier,
                        /* openInAppMenuItemProvider= */ null,
                        /* recentlyClosedEntriesManagerSupplier= */ () ->
                                mRecentlyClosedEntriesManager);
        RobolectricUtil.runAllBackgroundAndUi();
        mTabbedAppMenuPropertiesDelegate = Mockito.spy(delegate);

        MultiWindowTestUtils.resetInstanceInfo();

        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);

        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);

        DomDistillerUrlUtilsJni.setInstanceForTesting(mDomDistillerUrlUtilsJni);
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);

        mTabbedAppMenuPropertiesDelegate.setImageFetcherForTesting(mBookmarkImageFetcher);
    }

    @After
    public void tearDown() {
        AccessibilityState.setIsKnownScreenReaderEnabledForTesting(false);
    }

    @Nullable
    private static ListItem findItemById(Iterable<ListItem> items, int id) {
        for (ListItem item : items) {
            if (item.model.get(AppMenuItemProperties.MENU_ITEM_ID) == id) {
                return item;
            }
        }
        return null;
    }

    private void assertMenuTreesAreEqual(
            Iterable<ListItem> items,
            List<MenuItem> expectedNodes,
            BiConsumer<ListItem, Object> assertionLogic) {
        List<ListItem> itemList = new ArrayList<>();
        for (ListItem item : items) {
            itemList.add(item);
        }

        assertEquals("Mismatched item count.", expectedNodes.size(), itemList.size());

        for (int i = 0; i < expectedNodes.size(); i++) {
            assertMenuTreesAreEqualRecursively(
                    itemList.get(i), expectedNodes.get(i), assertionLogic);
        }
    }

    private void assertMenuTreesAreEqualRecursively(
            ListItem item, MenuItem expectedNode, BiConsumer<ListItem, Object> assertionLogic) {
        assertionLogic.accept(item, expectedNode.property);

        boolean hasSubItems =
                item.model.containsKey(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER);
        assertEquals("Mismatched children.", expectedNode.children.length > 0, hasSubItems);

        if (!hasSubItems) return;

        List<ListItem> subItems =
                item.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        Assert.assertNotNull(subItems);

        assertEquals("Mismatched children count.", expectedNode.children.length, subItems.size());

        for (int i = 0; i < subItems.size(); i++) {
            assertMenuTreesAreEqualRecursively(
                    subItems.get(i), expectedNode.children[i], assertionLogic);
        }
    }

    private void assertMenuItemsAreEqual(Iterable<ListItem> items, List<MenuItem> expectedItems) {

        assertMenuTreesAreEqual(
                items,
                expectedItems,
                (item, expectedId) -> {
                    assertEquals(
                            "We got " + getMenuTitle(item) + ", which was unexpected.",
                            (int) expectedId,
                            item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
                });
    }

    private void assertMenuTitlesAreEqual(Iterable<ListItem> items, List<MenuItem> expectedTitles) {
        Context context = ContextUtils.getApplicationContext();
        assertMenuTreesAreEqual(
                items,
                expectedTitles,
                (item, expected) -> {
                    CharSequence title =
                            item.model.containsKey(AppMenuItemProperties.TITLE)
                                    ? item.model.get(AppMenuItemProperties.TITLE)
                                    : null;
                    if (expected instanceof Integer) {
                        int expectedTitleRes = (Integer) expected;
                        String expectedTitleString =
                                (expectedTitleRes == 0)
                                        ? null
                                        : context.getString(expectedTitleRes);
                        assertEquals("Mismatched title:", expectedTitleString, title);
                    } else {
                        assertEquals("Mismatched title:", expected, title);
                    }
                });
    }

    private void assertMenuItemsHaveIcons(Iterable<ListItem> items, List<MenuItem> expectedItems) {

        assertMenuTreesAreEqual(
                items,
                expectedItems,
                (item, expectedId) -> {
                    if (item.type != AppMenuHandler.AppMenuItemType.BUTTON_ROW
                            && item.type != AppMenuHandler.AppMenuItemType.DIVIDER
                            && item.type != AppMenuHandler.AppMenuItemType.EMPTY) {
                        boolean hasIcon =
                                item.model.containsKey(AppMenuItemProperties.ICON)
                                        && item.model.get(AppMenuItemProperties.ICON) != null;
                        boolean hasIconSupplier =
                                item.model.containsKey(AppMenuItemProperties.ICON_SUPPLIER)
                                        && item.model.get(AppMenuItemProperties.ICON_SUPPLIER)
                                                != null;
                        Assert.assertTrue(
                                "Item should have an icon: " + getMenuTitle(item),
                                hasIcon || hasIconSupplier);
                    }
                });
    }

    private void assertActionBarItemsAreEqual(ModelList modelList, Integer... expectedItems) {
        ListItem iconRow = findItemById(modelList, R.id.icon_row_menu_id);
        assertNotNull(iconRow);
        List<Integer> actualItems = new ArrayList<>();
        for (ListItem icon : iconRow.model.get(AppMenuItemProperties.ADDITIONAL_ICONS)) {
            actualItems.add(icon.model.get(AppMenuItemProperties.MENU_ITEM_ID));
        }

        assertThat(
                "Populated action bar items were:"
                        + getMenuTitles(iconRow.model.get(AppMenuItemProperties.ADDITIONAL_ICONS)),
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

    /** Preparation to mock {@link TabModel} methods which play a part to enable group tabs. */
    private void prepareMocksForGroupTabsOnTabModel(TabModel tabmodel) {
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
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.icon_row_menu_id),
                                item(R.id.new_tab_menu_id),
                                item(R.id.new_incognito_tab_menu_id),
                                item(
                                        R.id.tab_groups_parent_menu_id,
                                        item(R.id.add_to_group_menu_id),
                                        item(R.id.create_new_tab_group_menu_id)),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.passwords_and_autofill_parent_menu_id,
                                        item(R.id.google_password_manager_menu_id),
                                        item(R.id.payment_methods_menu_id),
                                        item(R.id.addresses_and_more_menu_id)),
                                item(
                                        R.id.history_parent_menu_id,
                                        item(R.id.open_history_menu_id),
                                        item(R.id.recent_tabs_menu_id),
                                        item(R.id.quick_delete_menu_id)),
                                item(R.id.downloads_menu_id),
                                item(
                                        R.id.bookmarks_parent_menu_id,
                                        item(R.id.all_bookmarks_menu_id),
                                        item(R.id.bookmark_this_page_menu_id),
                                        item(R.id.toggle_bookmarks_bar_menu_id),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.reading_list_parent_menu_id,
                                                item(R.id.add_to_reading_list_menu_id),
                                                item(R.id.show_reading_list_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(R.id.bookmark_menu_id),
                                        item(R.id.bookmark_menu_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_menu_id),
                                                item(R.id.bookmark_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_folder_menu_id, item(0))),
                                        item(R.id.bookmark_folder_menu_id, item(0)))));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(
                    item(
                            R.id.extensions_parent_menu_id,
                            item(R.id.extensions_menu_menu_id),
                            item(R.id.manage_extensions_menu_id),
                            item(R.id.extensions_webstore_menu_id)));
        }

        expectedItems.addAll(
                Arrays.asList(
                        item(R.id.divider_line_id),
                        item(R.id.preferences_id),
                        item(R.id.ntp_customization_id),
                        item(
                                R.id.help_parent_menu_id,
                                item(R.id.about_chrome_menu_id),
                                item(R.id.help_id),
                                item(R.id.report_issue_menu_id))));

        assertMenuItemsAreEqual(modelList, expectedItems);
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
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.icon_row_menu_id),
                                item(R.id.new_tab_menu_id),
                                item(R.id.new_incognito_tab_menu_id),
                                item(
                                        R.id.tab_groups_parent_menu_id,
                                        item(R.id.add_to_group_menu_id),
                                        item(R.id.create_new_tab_group_menu_id)),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.passwords_and_autofill_parent_menu_id,
                                        item(R.id.google_password_manager_menu_id),
                                        item(R.id.payment_methods_menu_id),
                                        item(R.id.addresses_and_more_menu_id)),
                                item(
                                        R.id.history_parent_menu_id,
                                        item(R.id.open_history_menu_id),
                                        item(R.id.recent_tabs_menu_id),
                                        item(R.id.quick_delete_menu_id)),
                                item(R.id.info_menu_id),
                                item(R.id.page_info_divider_line_id),
                                item(R.id.downloads_menu_id),
                                item(
                                        R.id.bookmarks_parent_menu_id,
                                        item(R.id.all_bookmarks_menu_id),
                                        item(R.id.bookmark_this_page_menu_id),
                                        item(R.id.toggle_bookmarks_bar_menu_id),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.reading_list_parent_menu_id,
                                                item(R.id.add_to_reading_list_menu_id),
                                                item(R.id.show_reading_list_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(R.id.bookmark_menu_id),
                                        item(R.id.bookmark_menu_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_menu_id),
                                                item(R.id.bookmark_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_folder_menu_id, item(0))),
                                        item(R.id.bookmark_folder_menu_id, item(0)))));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(
                    item(
                            R.id.extensions_parent_menu_id,
                            item(R.id.extensions_menu_menu_id),
                            item(R.id.manage_extensions_menu_id),
                            item(R.id.extensions_webstore_menu_id)));
        }

        expectedItems.addAll(
                Arrays.asList(
                        item(R.id.divider_line_id),
                        item(
                                R.id.save_and_share_parent_menu_id,
                                item(R.id.share_menu_id),
                                item(R.id.copy_link_menu_id),
                                item(R.id.send_to_devices_menu_id),
                                item(R.id.qr_code_menu_id)),
                        item(R.id.find_in_page_id),
                        item(R.id.open_with_id),
                        item(R.id.divider_line_id),
                        item(R.id.preferences_id),
                        item(
                                R.id.help_parent_menu_id,
                                item(R.id.about_chrome_menu_id),
                                item(R.id.help_id),
                                item(R.id.report_issue_menu_id))));

        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    private void testPageMenuItems_RegularPage() {
        setUpMocksForPageMenu();
        setMenuOptions(
                new MenuOptions()
                        .withShowTranslate()
                        .withShowAddToHomeScreen()
                        .withAutoDarkEnabled());
        when(mTabModel.getCount()).thenReturn(1);

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems = new ArrayList<>();
        List<MenuItem> expectedTitles = new ArrayList<>();

        expectedItems.add(item(R.id.icon_row_menu_id));
        expectedTitles.add(item(0));

        expectedItems.add(item(R.id.new_tab_menu_id));
        expectedTitles.add(item(R.string.menu_new_tab));

        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            expectedItems.add(item(R.id.new_incognito_tab_menu_id));
            expectedTitles.add(item(R.string.menu_new_incognito_tab));
        }

        expectedItems.add(
                item(
                        R.id.tab_groups_parent_menu_id,
                        item(R.id.add_to_group_menu_id),
                        item(R.id.create_new_tab_group_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_tab_groups,
                        item(R.string.menu_add_tab_to_new_group),
                        item(R.string.menu_create_new_tab_group)));

        expectedItems.add(item(R.id.divider_line_id));
        expectedTitles.add(item(0));

        expectedItems.add(
                item(
                        R.id.passwords_and_autofill_parent_menu_id,
                        item(R.id.google_password_manager_menu_id),
                        item(R.id.payment_methods_menu_id),
                        item(R.id.addresses_and_more_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_passwords_and_autofill,
                        item(R.string.menu_google_password_manager),
                        item(R.string.menu_payment_methods),
                        item(R.string.menu_addresses_and_more)));
        expectedItems.add(
                item(
                        R.id.history_parent_menu_id,
                        item(R.id.open_history_menu_id),
                        item(R.id.recent_tabs_menu_id),
                        item(R.id.quick_delete_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_history,
                        item(R.string.menu_history),
                        item(R.string.menu_recent_tabs),
                        item(R.string.menu_quick_delete)));

        if (!mTabbedAppMenuPropertiesDelegate.isTabletSizeScreen()) {
            expectedItems.add(item(R.id.info_menu_id));
            expectedTitles.add(item(R.string.menu_site_controls));

            expectedItems.add(item(R.id.page_info_divider_line_id));
            expectedTitles.add(item(0));
        }

        expectedItems.add(item(R.id.downloads_menu_id));
        expectedTitles.add(item(R.string.menu_downloads));

        expectedItems.add(
                item(
                        R.id.bookmarks_parent_menu_id,
                        item(R.id.all_bookmarks_menu_id),
                        item(R.id.bookmark_this_page_menu_id),
                        item(R.id.toggle_bookmarks_bar_menu_id),
                        item(R.id.divider_line_id),
                        item(
                                R.id.reading_list_parent_menu_id,
                                item(R.id.add_to_reading_list_menu_id),
                                item(R.id.show_reading_list_menu_id)),
                        item(R.id.divider_line_id),
                        item(R.id.bookmark_menu_id),
                        item(R.id.bookmark_menu_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_menu_id),
                                item(R.id.bookmark_menu_id)),
                        item(R.id.divider_line_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_folder_menu_id, item(0))),
                        item(R.id.bookmark_folder_menu_id, item(0))));
        expectedTitles.add(
                item(
                        R.string.menu_bookmarks,
                        item(R.string.menu_bookmarks),
                        item(R.string.menu_bookmark_this_page),
                        item(R.string.menu_show_bookmarks_bar),
                        item(0),
                        item(
                                R.string.menu_reading_list,
                                item(R.string.menu_add_to_reading_list),
                                item(R.string.menu_show_reading_list)),
                        item(0),
                        item("Bookmark 1"),
                        item("Bookmark 2"),
                        item(
                                "Folder 1",
                                item("Bookmark in folder 1"),
                                item("Bookmark in folder 2")),
                        item(0),
                        item(R.string.menu_mobile_bookmarks, item("Partner bookmarks", item(0))),
                        item(R.string.menu_other_bookmarks, item(0))));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(
                    item(
                            R.id.extensions_parent_menu_id,
                            item(R.id.extensions_menu_menu_id),
                            item(R.id.manage_extensions_menu_id),
                            item(R.id.extensions_webstore_menu_id)));
            expectedTitles.add(
                    item(
                            R.string.menu_extensions,
                            item(R.string.menu_extensions_menu),
                            item(R.string.menu_manage_extensions),
                            item(R.string.menu_chrome_webstore)));
        }

        expectedItems.add(item(R.id.divider_line_id));
        expectedTitles.add(item(0));

        expectedItems.add(
                item(
                        R.id.save_and_share_parent_menu_id,
                        item(R.id.universal_install),
                        item(R.id.divider_line_id),
                        item(R.id.share_menu_id),
                        item(R.id.copy_link_menu_id),
                        item(R.id.send_to_devices_menu_id),
                        item(R.id.qr_code_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_save_and_share,
                        item(R.string.menu_install_create_shortcut),
                        item(0),
                        item(R.string.menu_share_page),
                        item(R.string.menu_copy_link),
                        item(R.string.menu_send_to_devices),
                        item(R.string.menu_qr_code)));

        expectedItems.add(item(R.id.find_in_page_id));
        expectedTitles.add(item(R.string.menu_find_in_page));

        expectedItems.add(item(R.id.translate_id));
        expectedTitles.add(item(R.string.menu_translate));

        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(item(R.id.request_desktop_site_id));
            expectedTitles.add(item(R.string.menu_request_desktop_site));
        }

        expectedItems.add(item(R.id.auto_dark_web_contents_id));
        expectedTitles.add(item(R.string.menu_auto_dark_web_contents));

        expectedItems.add(item(R.id.divider_line_id));
        expectedTitles.add(item(0));

        expectedItems.add(item(R.id.preferences_id));
        expectedTitles.add(item(R.string.menu_settings));

        expectedItems.add(
                item(
                        R.id.help_parent_menu_id,
                        item(R.id.about_chrome_menu_id),
                        item(R.id.help_id),
                        item(R.id.report_issue_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_help,
                        item(R.string.menu_about_chrome),
                        item(R.string.menu_help_center),
                        item(R.string.menu_report_issue)));

        Integer[] expectedActionBarItems =
                ChromeFeatureList.sThreeDotMenuBackButton.isEnabled()
                        ? new Integer[] {
                            R.id.forward_menu_id,
                            R.id.back_menu_id,
                            R.id.bookmark_this_page_id,
                            R.id.offline_page_id,
                            R.id.reload_menu_id
                        }
                        : new Integer[] {
                            R.id.forward_menu_id,
                            R.id.bookmark_this_page_id,
                            R.id.offline_page_id,
                            R.id.info_menu_id,
                            R.id.reload_menu_id
                        };

        assertMenuItemsAreEqual(modelList, expectedItems);
        assertMenuTitlesAreEqual(modelList, expectedTitles);
        assertActionBarItemsAreEqual(modelList, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage() {
        testPageMenuItems_RegularPage();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testPageMenuItems_Tablet_RegularPage() {
        testPageMenuItems_RegularPage();
    }

    private void testPageMenuItems_IncognitoPage() {
        setUpMocksForPageMenu();
        when(mTab.isIncognito()).thenReturn(true);
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        setMenuOptions(new MenuOptions().withShowTranslate().withAutoDarkEnabled());

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems = new ArrayList<>();
        List<MenuItem> expectedTitles = new ArrayList<>();

        expectedItems.add(item(R.id.icon_row_menu_id));
        expectedTitles.add(item(0));

        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            expectedItems.add(item(R.id.new_tab_menu_id));
            expectedTitles.add(item(R.string.menu_new_tab));
        }

        expectedItems.add(item(R.id.new_incognito_tab_menu_id));
        expectedTitles.add(item(R.string.menu_new_incognito_tab));

        expectedItems.add(
                item(
                        R.id.tab_groups_parent_menu_id,
                        item(R.id.add_to_group_menu_id),
                        item(R.id.create_new_tab_group_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_tab_groups,
                        item(R.string.menu_add_tab_to_new_group),
                        item(R.string.menu_create_new_tab_group)));

        expectedItems.add(item(R.id.divider_line_id));
        expectedTitles.add(item(0));

        expectedItems.add(
                item(
                        R.id.passwords_and_autofill_parent_menu_id,
                        item(R.id.google_password_manager_menu_id),
                        item(R.id.payment_methods_menu_id),
                        item(R.id.addresses_and_more_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_passwords_and_autofill,
                        item(R.string.menu_google_password_manager),
                        item(R.string.menu_payment_methods),
                        item(R.string.menu_addresses_and_more)));
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            expectedItems.add(item(R.id.history_parent_menu_id, item(R.id.open_history_menu_id)));
            expectedTitles.add(item(R.string.menu_history, item(R.string.menu_history)));
        }

        if (!mTabbedAppMenuPropertiesDelegate.isTabletSizeScreen()) {
            expectedItems.add(item(R.id.info_menu_id));
            expectedTitles.add(item(R.string.menu_site_controls));

            expectedItems.add(item(R.id.page_info_divider_line_id));
            expectedTitles.add(item(0));
        }

        expectedItems.add(item(R.id.downloads_menu_id));
        expectedTitles.add(item(R.string.menu_downloads));

        expectedItems.add(
                item(
                        R.id.bookmarks_parent_menu_id,
                        item(R.id.all_bookmarks_menu_id),
                        item(R.id.bookmark_this_page_menu_id),
                        item(R.id.toggle_bookmarks_bar_menu_id),
                        item(R.id.divider_line_id),
                        item(
                                R.id.reading_list_parent_menu_id,
                                item(R.id.add_to_reading_list_menu_id),
                                item(R.id.show_reading_list_menu_id)),
                        item(R.id.divider_line_id),
                        item(R.id.bookmark_menu_id),
                        item(R.id.bookmark_menu_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_menu_id),
                                item(R.id.bookmark_menu_id)),
                        item(R.id.divider_line_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_folder_menu_id, item(0))),
                        item(R.id.bookmark_folder_menu_id, item(0))));
        expectedTitles.add(
                item(
                        R.string.menu_bookmarks,
                        item(R.string.menu_bookmarks),
                        item(R.string.menu_bookmark_this_page),
                        item(R.string.menu_show_bookmarks_bar),
                        item(0),
                        item(
                                R.string.menu_reading_list,
                                item(R.string.menu_add_to_reading_list),
                                item(R.string.menu_show_reading_list)),
                        item(0),
                        item("Bookmark 1"),
                        item("Bookmark 2"),
                        item(
                                "Folder 1",
                                item("Bookmark in folder 1"),
                                item("Bookmark in folder 2")),
                        item(0),
                        item(R.string.menu_mobile_bookmarks, item("Partner bookmarks", item(0))),
                        item(R.string.menu_other_bookmarks, item(0))));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(
                    item(
                            R.id.extensions_parent_menu_id,
                            item(R.id.extensions_menu_menu_id),
                            item(R.id.manage_extensions_menu_id),
                            item(R.id.extensions_webstore_menu_id)));
            expectedTitles.add(
                    item(
                            R.string.menu_extensions,
                            item(R.string.menu_extensions_menu),
                            item(R.string.menu_manage_extensions),
                            item(R.string.menu_chrome_webstore)));
        }

        expectedItems.add(item(R.id.divider_line_id));
        expectedTitles.add(item(0));

        expectedItems.add(
                item(
                        R.id.save_and_share_parent_menu_id,
                        item(R.id.share_menu_id),
                        item(R.id.copy_link_menu_id),
                        item(R.id.send_to_devices_menu_id),
                        item(R.id.qr_code_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_save_and_share,
                        item(R.string.menu_share_page),
                        item(R.string.menu_copy_link),
                        item(R.string.menu_send_to_devices),
                        item(R.string.menu_qr_code)));

        expectedItems.add(item(R.id.find_in_page_id));
        expectedTitles.add(item(R.string.menu_find_in_page));

        expectedItems.add(item(R.id.translate_id));
        expectedTitles.add(item(R.string.menu_translate));

        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(item(R.id.request_desktop_site_id));
            expectedTitles.add(item(R.string.menu_request_desktop_site));
        }

        expectedItems.add(item(R.id.auto_dark_web_contents_id));
        expectedTitles.add(item(R.string.menu_auto_dark_web_contents));

        expectedItems.add(item(R.id.divider_line_id));
        expectedTitles.add(item(0));

        expectedItems.add(item(R.id.preferences_id));
        expectedTitles.add(item(R.string.menu_settings));

        expectedItems.add(
                item(
                        R.id.help_parent_menu_id,
                        item(R.id.about_chrome_menu_id),
                        item(R.id.help_id),
                        item(R.id.report_issue_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_help,
                        item(R.string.menu_about_chrome),
                        item(R.string.menu_help_center),
                        item(R.string.menu_report_issue)));

        assertMenuItemsAreEqual(modelList, expectedItems);
        assertMenuTitlesAreEqual(modelList, expectedTitles);

        Integer[] expectedActionBarItems =
                ChromeFeatureList.sThreeDotMenuBackButton.isEnabled()
                        ? new Integer[] {
                            R.id.forward_menu_id,
                            R.id.back_menu_id,
                            R.id.bookmark_this_page_id,
                            R.id.offline_page_id,
                            R.id.reload_menu_id
                        }
                        : new Integer[] {
                            R.id.forward_menu_id,
                            R.id.bookmark_this_page_id,
                            R.id.offline_page_id,
                            R.id.info_menu_id,
                            R.id.reload_menu_id
                        };
        assertActionBarItemsAreEqual(modelList, expectedActionBarItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_IncognitoPage() {
        testPageMenuItems_IncognitoPage();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testPageMenuItems_Tablet_IncognitoPage() {
        testPageMenuItems_IncognitoPage();
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
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems = new ArrayList<>();
        List<MenuItem> expectedTitles = new ArrayList<>();

        expectedItems.add(item(R.id.icon_row_menu_id));
        expectedTitles.add(item(0));

        expectedItems.add(item(R.id.new_tab_menu_id));
        expectedTitles.add(item(R.string.menu_new_tab));

        expectedItems.add(item(R.id.new_incognito_tab_menu_id));
        expectedTitles.add(item(R.string.menu_new_incognito_tab));

        expectedItems.add(
                item(
                        R.id.tab_groups_parent_menu_id,
                        item(R.id.add_to_group_menu_id),
                        item(R.id.create_new_tab_group_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_tab_groups,
                        item(R.string.menu_add_tab_to_new_group),
                        item(R.string.menu_create_new_tab_group)));

        expectedItems.add(item(R.id.divider_line_id));
        expectedTitles.add(item(0));

        expectedItems.add(
                item(
                        R.id.passwords_and_autofill_parent_menu_id,
                        item(R.id.google_password_manager_menu_id),
                        item(R.id.payment_methods_menu_id),
                        item(R.id.addresses_and_more_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_passwords_and_autofill,
                        item(R.string.menu_google_password_manager),
                        item(R.string.menu_payment_methods),
                        item(R.string.menu_addresses_and_more)));
        expectedItems.add(
                item(
                        R.id.history_parent_menu_id,
                        item(R.id.open_history_menu_id),
                        item(R.id.recent_tabs_menu_id),
                        item(R.id.quick_delete_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_history,
                        item(R.string.menu_history),
                        item(R.string.menu_recent_tabs),
                        item(R.string.menu_quick_delete)));

        expectedItems.add(item(R.id.info_menu_id));
        expectedTitles.add(item(R.string.menu_site_controls));

        expectedItems.add(item(R.id.page_info_divider_line_id));
        expectedTitles.add(item(0));

        expectedItems.add(item(R.id.downloads_menu_id));
        expectedTitles.add(item(R.string.menu_downloads));

        expectedItems.add(
                item(
                        R.id.bookmarks_parent_menu_id,
                        item(R.id.all_bookmarks_menu_id),
                        item(R.id.bookmark_this_page_menu_id),
                        item(R.id.toggle_bookmarks_bar_menu_id),
                        item(R.id.divider_line_id),
                        item(
                                R.id.reading_list_parent_menu_id,
                                item(R.id.add_to_reading_list_menu_id),
                                item(R.id.show_reading_list_menu_id)),
                        item(R.id.divider_line_id),
                        item(R.id.bookmark_menu_id),
                        item(R.id.bookmark_menu_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_menu_id),
                                item(R.id.bookmark_menu_id)),
                        item(R.id.divider_line_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_folder_menu_id, item(0))),
                        item(R.id.bookmark_folder_menu_id, item(0))));
        expectedTitles.add(
                item(
                        R.string.menu_bookmarks,
                        item(R.string.menu_bookmarks),
                        item(R.string.menu_bookmark_this_page),
                        item(R.string.menu_show_bookmarks_bar),
                        item(0),
                        item(
                                R.string.menu_reading_list,
                                item(R.string.menu_add_to_reading_list),
                                item(R.string.menu_show_reading_list)),
                        item(0),
                        item("Bookmark 1"),
                        item("Bookmark 2"),
                        item(
                                "Folder 1",
                                item("Bookmark in folder 1"),
                                item("Bookmark in folder 2")),
                        item(0),
                        item(R.string.menu_mobile_bookmarks, item("Partner bookmarks", item(0))),
                        item(R.string.menu_other_bookmarks, item(0))));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(
                    item(
                            R.id.extensions_parent_menu_id,
                            item(R.id.extensions_menu_menu_id),
                            item(R.id.manage_extensions_menu_id),
                            item(R.id.extensions_webstore_menu_id)));
            expectedTitles.add(
                    item(
                            R.string.menu_extensions,
                            item(R.string.menu_extensions_menu),
                            item(R.string.menu_manage_extensions),
                            item(R.string.menu_chrome_webstore)));
        }

        expectedItems.add(item(R.id.divider_line_id));
        expectedTitles.add(item(0));

        expectedItems.add(
                item(
                        R.id.save_and_share_parent_menu_id,
                        item(R.id.universal_install),
                        item(R.id.divider_line_id),
                        item(R.id.share_menu_id),
                        item(R.id.copy_link_menu_id),
                        item(R.id.send_to_devices_menu_id),
                        item(R.id.qr_code_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_save_and_share,
                        item(R.string.menu_install_create_shortcut),
                        item(0),
                        item(R.string.menu_share_page),
                        item(R.string.menu_copy_link),
                        item(R.string.menu_send_to_devices),
                        item(R.string.menu_qr_code)));

        expectedItems.add(item(R.id.find_in_page_id));
        expectedTitles.add(item(R.string.menu_find_in_page));

        expectedItems.add(item(R.id.translate_id));
        expectedTitles.add(item(R.string.menu_translate));

        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(item(R.id.request_desktop_site_id));
            expectedTitles.add(item(R.string.menu_request_desktop_site));
        }

        expectedItems.add(item(R.id.auto_dark_web_contents_id));
        expectedTitles.add(item(R.string.menu_auto_dark_web_contents));

        expectedItems.add(item(R.id.divider_line_id));
        expectedTitles.add(item(0));

        expectedItems.add(item(R.id.preferences_id));
        expectedTitles.add(item(R.string.menu_settings));

        expectedItems.add(
                item(
                        R.id.help_parent_menu_id,
                        item(R.id.about_chrome_menu_id),
                        item(R.id.help_id),
                        item(R.id.report_issue_menu_id)));
        expectedTitles.add(
                item(
                        R.string.menu_help,
                        item(R.string.menu_about_chrome),
                        item(R.string.menu_help_center),
                        item(R.string.menu_report_issue)));

        Integer[] expectedActionBarItems =
                ChromeFeatureList.sThreeDotMenuBackButton.isEnabled()
                        ? new Integer[] {
                            R.id.forward_menu_id,
                            R.id.back_menu_id,
                            R.id.bookmark_this_page_id,
                            R.id.offline_page_id,
                            R.id.reload_menu_id
                        }
                        : new Integer[] {
                            R.id.forward_menu_id,
                            R.id.bookmark_this_page_id,
                            R.id.offline_page_id,
                            R.id.info_menu_id,
                            R.id.reload_menu_id
                        };

        assertMenuItemsAreEqual(modelList, expectedItems);
        assertMenuTitlesAreEqual(modelList, expectedTitles);
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
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.icon_row_menu_id),
                                item(R.id.new_tab_menu_id),
                                item(R.id.new_incognito_tab_menu_id),
                                item(
                                        R.id.tab_groups_parent_menu_id,
                                        item(R.id.add_to_group_menu_id),
                                        item(R.id.create_new_tab_group_menu_id)),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.passwords_and_autofill_parent_menu_id,
                                        item(R.id.google_password_manager_menu_id),
                                        item(R.id.payment_methods_menu_id),
                                        item(R.id.addresses_and_more_menu_id)),
                                item(
                                        R.id.history_parent_menu_id,
                                        item(R.id.open_history_menu_id),
                                        item(R.id.recent_tabs_menu_id),
                                        item(R.id.quick_delete_menu_id)),
                                item(R.id.info_menu_id),
                                item(R.id.page_info_divider_line_id),
                                item(R.id.downloads_menu_id),
                                item(
                                        R.id.bookmarks_parent_menu_id,
                                        item(R.id.all_bookmarks_menu_id),
                                        item(R.id.bookmark_this_page_menu_id),
                                        item(R.id.toggle_bookmarks_bar_menu_id),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.reading_list_parent_menu_id,
                                                item(R.id.add_to_reading_list_menu_id),
                                                item(R.id.show_reading_list_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(R.id.bookmark_menu_id),
                                        item(R.id.bookmark_menu_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_menu_id),
                                                item(R.id.bookmark_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_folder_menu_id, item(0))),
                                        item(R.id.bookmark_folder_menu_id, item(0)))));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(
                    item(
                            R.id.extensions_parent_menu_id,
                            item(R.id.extensions_menu_menu_id),
                            item(R.id.manage_extensions_menu_id),
                            item(R.id.extensions_webstore_menu_id)));
        }

        expectedItems.addAll(
                Arrays.asList(
                        item(R.id.divider_line_id),
                        item(
                                R.id.save_and_share_parent_menu_id,
                                item(R.id.universal_install),
                                item(R.id.divider_line_id),
                                item(R.id.share_menu_id),
                                item(R.id.copy_link_menu_id),
                                item(R.id.send_to_devices_menu_id),
                                item(R.id.qr_code_menu_id)),
                        item(R.id.find_in_page_id),
                        item(R.id.translate_id),
                        // Request desktop site is hidden.
                        item(R.id.auto_dark_web_contents_id),
                        item(R.id.divider_line_id),
                        item(R.id.preferences_id),
                        item(
                                R.id.help_parent_menu_id,
                                item(R.id.about_chrome_menu_id),
                                item(R.id.help_id),
                                item(R.id.report_issue_menu_id))));

        assertMenuItemsAreEqual(modelList, expectedItems);
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
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.icon_row_menu_id),
                                item(R.id.update_menu_id),
                                item(R.id.new_tab_menu_id),
                                item(R.id.new_incognito_tab_menu_id),
                                item(
                                        R.id.tab_groups_parent_menu_id,
                                        item(R.id.add_to_group_menu_id),
                                        item(R.id.create_new_tab_group_menu_id)),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.passwords_and_autofill_parent_menu_id,
                                        item(R.id.google_password_manager_menu_id),
                                        item(R.id.payment_methods_menu_id),
                                        item(R.id.addresses_and_more_menu_id)),
                                item(
                                        R.id.history_parent_menu_id,
                                        item(R.id.open_history_menu_id),
                                        item(R.id.recent_tabs_menu_id),
                                        item(R.id.quick_delete_menu_id)),
                                item(R.id.info_menu_id),
                                item(R.id.page_info_divider_line_id),
                                item(R.id.downloads_menu_id),
                                item(
                                        R.id.bookmarks_parent_menu_id,
                                        item(R.id.all_bookmarks_menu_id),
                                        item(R.id.bookmark_this_page_menu_id),
                                        item(R.id.toggle_bookmarks_bar_menu_id),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.reading_list_parent_menu_id,
                                                item(R.id.add_to_reading_list_menu_id),
                                                item(R.id.show_reading_list_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(R.id.bookmark_menu_id),
                                        item(R.id.bookmark_menu_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_menu_id),
                                                item(R.id.bookmark_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_folder_menu_id, item(0))),
                                        item(R.id.bookmark_folder_menu_id, item(0)))));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(
                    item(
                            R.id.extensions_parent_menu_id,
                            item(R.id.extensions_menu_menu_id),
                            item(R.id.manage_extensions_menu_id),
                            item(R.id.extensions_webstore_menu_id)));
        }

        expectedItems.addAll(
                Arrays.asList(
                        item(R.id.divider_line_id),
                        item(
                                R.id.save_and_share_parent_menu_id,
                                item(R.id.universal_install),
                                item(R.id.divider_line_id),
                                item(R.id.share_menu_id),
                                item(R.id.copy_link_menu_id),
                                item(R.id.send_to_devices_menu_id),
                                item(R.id.qr_code_menu_id)),
                        item(R.id.find_in_page_id),
                        item(R.id.translate_id),
                        item(R.id.auto_dark_web_contents_id),
                        item(R.id.reader_mode_prefs_id),
                        item(R.id.divider_line_id),
                        item(R.id.preferences_id),
                        item(
                                R.id.help_parent_menu_id,
                                item(R.id.about_chrome_menu_id),
                                item(R.id.help_id),
                                item(R.id.report_issue_menu_id))));

        assertMenuItemsHaveIcons(modelList, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOverviewMenuItems_Tablet_SelectTabs_incognitoWindowEnabled() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        setUpIncognitoMocks();
        when(mTabbedAppMenuPropertiesDelegate.isMultiInstanceEnabled()).thenReturn(true);

        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                Arrays.asList(
                        item(R.id.new_tab_menu_id),
                        item(R.id.new_window_menu_id),
                        item(R.id.new_incognito_window_menu_id),
                        item(R.id.new_tab_group_menu_id),
                        item(R.id.close_all_tabs_menu_id),
                        item(R.id.menu_select_tabs),
                        item(R.id.quick_delete_menu_id),
                        item(R.id.preferences_id));

        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOverviewMenuItems_Phone_IncognitoWindow() {
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        setUpIncognitoMocks();

        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                Arrays.asList(
                        item(R.id.new_tab_menu_id),
                        item(R.id.new_incognito_tab_menu_id),
                        item(R.id.new_tab_group_menu_id),
                        item(R.id.close_all_incognito_tabs_menu_id),
                        item(R.id.menu_select_tabs),
                        item(R.id.preferences_id));

        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOverviewMenuItems_Tablet_IncognitoWindow_incognitoWindowEnabled() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        setUpIncognitoMocks();
        when(mTabbedAppMenuPropertiesDelegate.isMultiInstanceEnabled()).thenReturn(true);

        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                Arrays.asList(
                        item(R.id.new_incognito_tab_menu_id),
                        item(R.id.new_window_menu_id),
                        item(R.id.new_incognito_window_menu_id),
                        item(R.id.new_tab_group_menu_id),
                        item(R.id.close_all_incognito_tabs_menu_id),
                        item(R.id.menu_select_tabs),
                        item(R.id.preferences_id));

        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOverviewMenuItems_Phone_NoTabs() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getTotalTabCount()).thenReturn(0);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                Arrays.asList(
                        item(R.id.new_tab_menu_id),
                        item(R.id.new_incognito_tab_menu_id),
                        item(R.id.new_tab_group_menu_id),
                        item(R.id.close_all_tabs_menu_id),
                        item(R.id.menu_select_tabs),
                        item(R.id.quick_delete_menu_id),
                        item(R.id.preferences_id));

        assertMenuItemsAreEqual(modelList, expectedItems);
        PropertyModel closeAllTabsModel =
                findItemById(modelList, R.id.close_all_tabs_menu_id).model;
        assertFalse(closeAllTabsModel.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOverviewMenuItems_Phone_NoIncognitoTabs() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                Arrays.asList(
                        item(R.id.new_tab_menu_id),
                        item(R.id.new_incognito_tab_menu_id),
                        item(R.id.new_tab_group_menu_id),
                        item(R.id.close_all_incognito_tabs_menu_id),
                        item(R.id.menu_select_tabs),
                        item(R.id.preferences_id));

        assertMenuItemsAreEqual(modelList, expectedItems);

        PropertyModel closeAllTabsModel =
                findItemById(modelList, R.id.close_all_incognito_tabs_menu_id).model;
        assertFalse(closeAllTabsModel.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOverviewMenuItems_Phone_SelectTabs() {
        setUpMocksForOverviewMenu();
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                Arrays.asList(
                        item(R.id.new_tab_menu_id),
                        item(R.id.new_incognito_tab_menu_id),
                        item(R.id.new_tab_group_menu_id),
                        item(R.id.close_all_tabs_menu_id),
                        item(R.id.menu_select_tabs),
                        item(R.id.quick_delete_menu_id),
                        item(R.id.preferences_id));

        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    // Update this to work with the feature when launched.
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOverviewMenuItems_Phone_SelectTabs_NotTabSwitcherPane() {
        setUpMocksForOverviewMenu();
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        when(mPane.getPaneId()).thenReturn(PaneId.BOOKMARKS);
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());
        assertEquals(MenuGroup.OVERVIEW_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                Arrays.asList(
                        item(R.id.new_tab_menu_id),
                        item(R.id.new_incognito_tab_menu_id),
                        item(R.id.new_tab_group_menu_id),
                        item(R.id.close_all_tabs_menu_id),
                        item(R.id.quick_delete_menu_id),
                        item(R.id.preferences_id));

        assertMenuItemsAreEqual(modelList, expectedItems);
    }

    private void checkOverviewMenuItems(boolean newIncognitoWindowEnabled) {
        setUpIncognitoMocks();
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        when(mTabModel.getCount()).thenReturn(0);

        assertEquals(
                MenuGroup.TABLET_EMPTY_MODE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        Assert.assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowPageMenu());

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems = new ArrayList<>();

        expectedItems.add(item(R.id.new_tab_menu_id));

        if (newIncognitoWindowEnabled) {
            expectedItems.add(item(R.id.new_window_menu_id));
            expectedItems.add(item(R.id.new_incognito_window_menu_id));
        } else {
            expectedItems.add(item(R.id.new_incognito_tab_menu_id));
        }

        expectedItems.add(item(R.id.preferences_id));
        expectedItems.add(item(R.id.quick_delete_menu_id));

        assertMenuItemsAreEqual(modelList, expectedItems);
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
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        when(mTabbedAppMenuPropertiesDelegate.isMultiInstanceEnabled()).thenReturn(true);
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

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.icon_row_menu_id),
                                item(R.id.new_tab_menu_id),
                                item(R.id.new_incognito_tab_menu_id),
                                item(
                                        R.id.tab_groups_parent_menu_id,
                                        item(R.id.add_to_group_menu_id),
                                        item(R.id.create_new_tab_group_menu_id)),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.passwords_and_autofill_parent_menu_id,
                                        item(R.id.google_password_manager_menu_id),
                                        item(R.id.payment_methods_menu_id),
                                        item(R.id.addresses_and_more_menu_id)),
                                item(
                                        R.id.history_parent_menu_id,
                                        item(R.id.open_history_menu_id),
                                        item(R.id.recent_tabs_menu_id),
                                        item(R.id.quick_delete_menu_id)),
                                item(R.id.info_menu_id),
                                item(R.id.page_info_divider_line_id),
                                item(R.id.downloads_menu_id),
                                item(
                                        R.id.bookmarks_parent_menu_id,
                                        item(R.id.all_bookmarks_menu_id),
                                        item(R.id.bookmark_this_page_menu_id),
                                        item(R.id.toggle_bookmarks_bar_menu_id),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.reading_list_parent_menu_id,
                                                item(R.id.add_to_reading_list_menu_id),
                                                item(R.id.show_reading_list_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(R.id.bookmark_menu_id),
                                        item(R.id.bookmark_menu_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_menu_id),
                                                item(R.id.bookmark_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_folder_menu_id, item(0))),
                                        item(R.id.bookmark_folder_menu_id, item(0)))));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(
                    item(
                            R.id.extensions_parent_menu_id,
                            item(R.id.extensions_menu_menu_id),
                            item(R.id.manage_extensions_menu_id),
                            item(R.id.extensions_webstore_menu_id)));
        }

        expectedItems.addAll(
                Arrays.asList(
                        item(R.id.divider_line_id),
                        item(
                                R.id.save_and_share_parent_menu_id,
                                item(R.id.universal_install),
                                item(R.id.divider_line_id),
                                item(R.id.share_menu_id),
                                item(R.id.copy_link_menu_id),
                                item(R.id.send_to_devices_menu_id),
                                item(R.id.qr_code_menu_id)),
                        item(R.id.find_in_page_id)));

        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(item(R.id.request_desktop_site_id));
        }

        expectedItems.addAll(
                Arrays.asList(
                        item(R.id.auto_dark_web_contents_id),
                        item(R.id.get_image_descriptions_id),
                        item(R.id.divider_line_id),
                        item(R.id.preferences_id),
                        item(
                                R.id.help_parent_menu_id,
                                item(R.id.about_chrome_menu_id),
                                item(R.id.help_id),
                                item(R.id.report_issue_menu_id))));

        assertMenuItemsAreEqual(modelList, expectedItems);

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

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        List<MenuItem> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.icon_row_menu_id),
                                item(R.id.new_tab_menu_id),
                                item(R.id.new_incognito_tab_menu_id),
                                item(
                                        R.id.tab_groups_parent_menu_id,
                                        item(R.id.add_to_group_menu_id),
                                        item(R.id.create_new_tab_group_menu_id)),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.passwords_and_autofill_parent_menu_id,
                                        item(R.id.google_password_manager_menu_id),
                                        item(R.id.payment_methods_menu_id),
                                        item(R.id.addresses_and_more_menu_id)),
                                item(
                                        R.id.history_parent_menu_id,
                                        item(R.id.open_history_menu_id),
                                        item(R.id.recent_tabs_menu_id),
                                        item(R.id.quick_delete_menu_id)),
                                item(R.id.info_menu_id),
                                item(R.id.page_info_divider_line_id),
                                item(R.id.downloads_menu_id),
                                item(
                                        R.id.bookmarks_parent_menu_id,
                                        item(R.id.all_bookmarks_menu_id),
                                        item(R.id.bookmark_this_page_menu_id),
                                        item(R.id.toggle_bookmarks_bar_menu_id),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.reading_list_parent_menu_id,
                                                item(R.id.add_to_reading_list_menu_id),
                                                item(R.id.show_reading_list_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(R.id.bookmark_menu_id),
                                        item(R.id.bookmark_menu_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_menu_id),
                                                item(R.id.bookmark_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_folder_menu_id, item(0))),
                                        item(R.id.bookmark_folder_menu_id, item(0)))));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(
                    item(
                            R.id.extensions_parent_menu_id,
                            item(R.id.extensions_menu_menu_id),
                            item(R.id.manage_extensions_menu_id),
                            item(R.id.extensions_webstore_menu_id)));
        }

        expectedItems.addAll(
                Arrays.asList(
                        item(R.id.divider_line_id),
                        item(
                                R.id.save_and_share_parent_menu_id,
                                item(R.id.universal_install),
                                item(R.id.divider_line_id),
                                item(R.id.share_menu_id),
                                item(R.id.copy_link_menu_id),
                                item(R.id.send_to_devices_menu_id),
                                item(R.id.qr_code_menu_id)),
                        item(R.id.find_in_page_id)));

        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(item(R.id.request_desktop_site_id));
        }

        expectedItems.addAll(
                Arrays.asList(
                        item(R.id.auto_dark_web_contents_id),
                        item(R.id.divider_line_id),
                        item(R.id.preferences_id),
                        item(
                                R.id.help_parent_menu_id,
                                item(R.id.about_chrome_menu_id),
                                item(R.id.help_id),
                                item(R.id.report_issue_menu_id)),
                        item(R.id.managed_by_divider_line_id),
                        item(R.id.managed_by_menu_id)));

        assertMenuItemsAreEqual(modelList, expectedItems);
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

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        // TODO(crbug.com/427240031): Stop asserting on menu items that are not subject of this
        // test.
        List<MenuItem> expectedItems =
                new ArrayList<>(
                        Arrays.asList(
                                item(R.id.icon_row_menu_id),
                                item(R.id.new_tab_menu_id),
                                item(R.id.new_incognito_tab_menu_id),
                                item(
                                        R.id.tab_groups_parent_menu_id,
                                        item(R.id.add_to_group_menu_id),
                                        item(R.id.create_new_tab_group_menu_id)),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.passwords_and_autofill_parent_menu_id,
                                        item(R.id.google_password_manager_menu_id),
                                        item(R.id.payment_methods_menu_id),
                                        item(R.id.addresses_and_more_menu_id)),
                                item(
                                        R.id.history_parent_menu_id,
                                        item(R.id.open_history_menu_id),
                                        item(R.id.recent_tabs_menu_id),
                                        item(R.id.quick_delete_menu_id)),
                                item(R.id.info_menu_id),
                                item(R.id.page_info_divider_line_id),
                                item(R.id.downloads_menu_id),
                                item(
                                        R.id.bookmarks_parent_menu_id,
                                        item(R.id.all_bookmarks_menu_id),
                                        item(R.id.bookmark_this_page_menu_id),
                                        item(R.id.toggle_bookmarks_bar_menu_id),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.reading_list_parent_menu_id,
                                                item(R.id.add_to_reading_list_menu_id),
                                                item(R.id.show_reading_list_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(R.id.bookmark_menu_id),
                                        item(R.id.bookmark_menu_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_menu_id),
                                                item(R.id.bookmark_menu_id)),
                                        item(R.id.divider_line_id),
                                        item(
                                                R.id.bookmark_folder_menu_id,
                                                item(R.id.bookmark_folder_menu_id, item(0))),
                                        item(R.id.bookmark_folder_menu_id, item(0)))));

        if (ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS) {
            expectedItems.add(
                    item(
                            R.id.extensions_parent_menu_id,
                            item(R.id.extensions_menu_menu_id),
                            item(R.id.manage_extensions_menu_id),
                            item(R.id.extensions_webstore_menu_id)));
        }

        expectedItems.addAll(
                Arrays.asList(
                        item(R.id.divider_line_id),
                        item(
                                R.id.save_and_share_parent_menu_id,
                                item(R.id.universal_install),
                                item(R.id.divider_line_id),
                                item(R.id.share_menu_id),
                                item(R.id.copy_link_menu_id),
                                item(R.id.send_to_devices_menu_id),
                                item(R.id.qr_code_menu_id)),
                        item(R.id.find_in_page_id)));

        if (!DeviceInfo.isDesktop()) {
            expectedItems.add(item(R.id.request_desktop_site_id));
        }

        expectedItems.addAll(
                Arrays.asList(
                        item(R.id.auto_dark_web_contents_id),
                        item(R.id.divider_line_id),
                        item(R.id.preferences_id),
                        item(
                                R.id.help_parent_menu_id,
                                item(R.id.about_chrome_menu_id),
                                item(R.id.help_id),
                                item(R.id.report_issue_menu_id)),
                        item(R.id.menu_item_content_filter_divider_line_id),
                        item(R.id.menu_item_content_filter_help_center_id)));
        assertMenuItemsAreEqual(modelList, expectedItems);
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
    @Config(sdk = 31)
    public void testPageMenuItems_multiInstance_newWindow() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        MultiWindowTestUtils.enableMultiInstance();

        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 0,
                /* url= */ "https://url0",
                /* tabCount= */ 1,
                /* taskId= */ 123);

        // On phone, we do not show 'New Window'.
        mIsTabletScreen = false;
        mIsMultiWindowApiSupported = true;
        ModelList modelList = createMenuForMultiWindow();
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
            MultiWindowTestUtils.createInstance(
                    /* instanceId= */ i,
                    /* url= */ "https://url" + i,
                    /* tabCount= */ 1,
                    /* taskId= */ i);
        }

        ModelList modelList2 = createMenuForMultiWindow();
        assertFalse(isMenuVisible(modelList2, R.id.new_window_menu_id));
    }

    @Test
    @Config(sdk = 31)
    public void testPageMenuItems_multiInstance_moveTabToOtherWindow() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);

        MultiWindowTestUtils.enableMultiInstance();
        mIsMoveToOtherWindowSupported = true;

        ModelList modelList = createMenuForMultiWindow();
        assertTrue(isMenuVisible(modelList, R.id.move_to_other_window_menu_id));
    }

    @Test
    @Config(sdk = 31)
    public void testPageMenuItems_multiInstance_manageAllWindow() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        MultiWindowTestUtils.enableMultiInstance();

        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 0, /* url= */ "https://url0", /* tabCount= */ 1, /* taskId= */ 0);

        ModelList modelList = createMenuForMultiWindow();
        assertFalse(isMenuVisible(modelList, R.id.manage_all_windows_menu_id));

        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, /* url= */ "https://url1", /* tabCount= */ 1, /* taskId= */ 1);

        ModelList modelList2 = createMenuForMultiWindow();
        assertTrue(isMenuVisible(modelList2, R.id.manage_all_windows_menu_id));
    }

    @Test
    public void testPageMenuItems_universalInstall() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.SEARCH_URL);
        ModelList modelList = createMenuForMultiWindow();
        assertTrue(
                isMenuVisibleInSubmenu(
                        modelList, R.id.save_and_share_parent_menu_id, R.id.universal_install));

        assertFalse(
                isMenuVisibleInSubmenu(
                        modelList, R.id.save_and_share_parent_menu_id, R.id.open_webapk_id));
    }

    @Test
    public void managedByMenuItem_ChromeManagementPage() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions().withShowAddToHomeScreen());
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowManagedByMenuItem(any(Tab.class));

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertTrue(isMenuVisible(modelList, R.id.managed_by_menu_id));
    }

    @Test
    public void contentFilterHelpCenterItem_ChromeManagementPage() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions().withShowAddToHomeScreen());
        doReturn(true)
                .when(mTabbedAppMenuPropertiesDelegate)
                .shouldShowContentFilterHelpCenterMenuItem(any(Tab.class));

        assertEquals(MenuGroup.PAGE_MENU, mTabbedAppMenuPropertiesDelegate.getMenuGroup());
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

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

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        verify(mIncognitoReauthControllerMock).isReauthPageShowing();

        ListItem item = findItemById(modelList, R.id.new_incognito_tab_menu_id);
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
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        verifyNoMoreInteractions(mIncognitoReauthControllerMock);

        ListItem item = findItemById(modelList, R.id.new_incognito_tab_menu_id);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void readerModeEntryPointDisabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(isMenuVisible(modelList, R.id.reader_mode_menu_id));
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void readerModeEntryPointEnabledWhenDistillingInApp() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertTrue(
                isMenuVisible(
                        createModelList(getSubmenuItems(modelList, R.id.more_tools_menu_id)),
                        R.id.reader_mode_menu_id));
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void readerModeEntryPointEnabled_ShowReadingMode() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Context context = ContextUtils.getApplicationContext();
        assertTrue(
                isMenuVisibleWithCorrectTitle(
                        createModelList(getSubmenuItems(modelList, R.id.more_tools_menu_id)),
                        R.id.reader_mode_menu_id,
                        context.getString(R.string.show_reading_mode_text)));
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void readerModeEntryPointEnabled_HideReadingMode() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.CHROME_DISTILLER_EXAMPLE_URL);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Context context = ContextUtils.getApplicationContext();
        assertTrue(
                isMenuVisibleWithCorrectTitle(
                        createModelList(getSubmenuItems(modelList, R.id.more_tools_menu_id)),
                        R.id.reader_mode_menu_id,
                        context.getString(R.string.hide_reading_mode_text)));
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void readerModeEntryPointEnabled_chromePage() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(new GURL(getOriginalNativeNtpUrl()));

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        Context context = ContextUtils.getApplicationContext();
        assertFalse(
                isMenuVisibleWithCorrectTitle(
                        modelList,
                        R.id.reader_mode_menu_id,
                        context.getString(R.string.hide_reading_mode_text)));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void nameWindowMenuVisible_Api31Enabled() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertTrue(
                isMenuVisible(
                        createModelList(getSubmenuItems(modelList, R.id.more_tools_menu_id)),
                        R.id.name_window_menu_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void nameWindowMenuHidden_Api31Disabled() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(
                isMenuVisible(
                        createModelList(getSubmenuItems(modelList, R.id.more_tools_menu_id)),
                        R.id.name_window_menu_id));
    }

    /**
     * Helper to set up a regular page menu on a tablet and return the More Tools submenu.
     *
     * @param isVerticalTabsEnabled The state to write to VERTICAL_TABS_ENABLED preferences.
     * @return The populated ModelList for the More Tools submenu container.
     */
    private ModelList setUpPageMenuAndGetMoreToolsSubmenu(boolean isVerticalTabsEnabled) {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        when(mTabModel.getCount()).thenReturn(1);

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, isVerticalTabsEnabled);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        List<ListItem> moreToolsItems = getSubmenuItems(modelList, R.id.more_tools_menu_id);
        return createModelList(moreToolsItems);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    @Config(qualifiers = "sw600dp")
    public void tabLayoutToggleItem_ShowsVerticalOption_WhenHorizontalActive() {
        // Set up the menu with vertical tabs disabled.
        ModelList moreToolsSubmenu =
                setUpPageMenuAndGetMoreToolsSubmenu(/* isVerticalTabsEnabled= */ false);

        // Verify that the menu item exists in the submenu.
        assertTrue(
                "Vertical tabs menu item should exist in More tools",
                isMenuVisible(moreToolsSubmenu, R.id.toggle_tab_layout_menu_id));

        // Verify that it has the correct title in the submenu.
        assertTrue(
                "Title should match",
                isMenuVisibleWithCorrectTitle(
                        moreToolsSubmenu,
                        R.id.toggle_tab_layout_menu_id,
                        ContextUtils.getApplicationContext()
                                .getString(
                                        org.chromium.chrome.tab_ui.R.string.show_tabs_vertically)));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    @Config(qualifiers = "sw600dp")
    public void tabLayoutToggleItem_ShowsHorizontalOption_WhenVerticalActive() {
        // Set up the menu with vertical tabs enabled.
        ModelList moreToolsSubmenu =
                setUpPageMenuAndGetMoreToolsSubmenu(/* isVerticalTabsEnabled= */ true);

        // The menu item id stays identical.
        assertTrue(
                "Vertical tabs item should exist in more tools",
                isMenuVisible(moreToolsSubmenu, R.id.toggle_tab_layout_menu_id));

        // Verify that the title dynamically changed to "Show tabs horizontally"
        assertTrue(
                "Title should shift to horizontal option",
                isMenuVisibleWithCorrectTitle(
                        moreToolsSubmenu,
                        R.id.toggle_tab_layout_menu_id,
                        ContextUtils.getApplicationContext()
                                .getString(
                                        org.chromium.chrome.tab_ui.R.string
                                                .show_tabs_horizontally)));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @EnableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ENTRY_POINT + ":show_app_menu_item/true"
    })
    public void testDefaultBrowserPromo_Enabled() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        doReturn(true).when(mMockDefaultBrowserPromoUtils).shouldShowAppMenuItemEntryPoint();

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        // Verify that that the menu item exists.
        assertTrue(
                "Default Browser Promo item should be visible",
                isMenuVisible(modelList, R.id.default_browser_promo_menu_id));

        // Verify that it has the correct title.
        assertTrue(
                "Title should match",
                isMenuVisibleWithCorrectTitle(
                        modelList,
                        R.id.default_browser_promo_menu_id,
                        ContextUtils.getApplicationContext()
                                .getString(R.string.make_chrome_default)));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @DisableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ENTRY_POINT)
    public void testDefaultBrowserPromo_Disabled() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(
                "Default Browser Promo item should not be visible",
                isMenuVisible(modelList, R.id.default_browser_promo_menu_id));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @EnableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ENTRY_POINT + ":show_app_menu_item/false"
    })
    public void testDefaultBrowserPromo_SettingsOnly_HidesAppMenu() {
        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());

        // Even if the utils say we show it based on eligibility, the feature param false should
        // hide it.
        doReturn(true).when(mMockDefaultBrowserPromoUtils).shouldShowAppMenuItemEntryPoint();

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(
                "Default Browser Promo item should be hidden by the param",
                isMenuVisible(modelList, R.id.default_browser_promo_menu_id));
    }

    @Test
    public void pageZoomMenuOption_NotVisibleInReadingMode() {
        setUpMocksForPageMenu();
        PageZoomUtils.setShouldShowMenuItemForTesting(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.CHROME_DISTILLER_EXAMPLE_URL);
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(isMenuVisible(modelList, R.id.page_zoom_id));
    }

    private ModelList setUpMenuWithIncognitoReauthPage(boolean isShowing) {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        prepareMocksForGroupTabsOnTabModel(mIncognitoTabModel);
        doReturn(isShowing).when(mIncognitoReauthControllerMock).isReauthPageShowing();

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        verify(mIncognitoReauthControllerMock, atLeastOnce()).isReauthPageShowing();
        return modelList;
    }

    @Test
    public void testSelectTabsOption_IsEnabled_InIncognitoMode_When_IncognitoReauthIsNotShowing() {
        ModelList modelList = setUpMenuWithIncognitoReauthPage(/* isShowing= */ false);
        ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testSelectTabsOption_IsDisabled_InIncognitoMode_When_IncognitoReauthIsShowing() {
        ModelList modelList = setUpMenuWithIncognitoReauthPage(/* isShowing= */ true);
        ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertFalse(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testSelectTabsOption_IsEnabled_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        prepareMocksForGroupTabsOnTabModel(mTabModel);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testSelectTabsOption_IsDisabled_InRegularMode_TabStateNotInitialized() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        prepareMocksForGroupTabsOnTabModel(mTabModel);

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertFalse(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testSelectTabsOption_IsEnabledOneTab_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(1);
        Tab mockTab1 = mock(Tab.class);
        when(mTabModel.getTabAt(0)).thenReturn(mockTab1);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testSelectTabsOption_IsDisabled_InRegularMode_IndependentOfIncognitoReauth() {
        setUpMocksForOverviewMenu();
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        // Check group tabs enabled decision in regular mode doesn't depend on re-auth.
        verify(mIncognitoReauthControllerMock, times(0)).isReauthPageShowing();

        ListItem item = findItemById(modelList, R.id.menu_select_tabs);
        assertFalse(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testCustomizeNewTabPageOption() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(getOriginalNativeNtpUrl()));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        mActivityTabProvider.setForTesting(ntpTab);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        ListItem item = findItemById(modelList, R.id.ntp_customization_id);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FEED_AUDIO_OVERVIEWS})
    public void testListenToFeedMenuItem_available() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(getOriginalNativeNtpUrl()));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        mActivityTabProvider.setForTesting(ntpTab);
        when(mReadAloudController.isAvailable()).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.listen_to_feed_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FEED_AUDIO_OVERVIEWS})
    public void testListenToFeedMenuItem_unavailableWhenNotNtp() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(getOriginalNativeNtpUrl()));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        mActivityTabProvider.setForTesting(mTab);
        when(mReadAloudController.isAvailable()).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.listen_to_feed_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FEED_AUDIO_OVERVIEWS})
    public void testListenToFeedMenuItem_unavailableWhenFeedDisabled() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(getOriginalNativeNtpUrl()));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        mActivityTabProvider.setForTesting(ntpTab);
        when(mReadAloudController.isAvailable()).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(false);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.listen_to_feed_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FEED_AUDIO_OVERVIEWS})
    public void testListenToFeedMenuItem_unavailableWhenFeedHidden() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(getOriginalNativeNtpUrl()));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        mActivityTabProvider.setForTesting(ntpTab);
        when(mReadAloudController.isAvailable()).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.listen_to_feed_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FEED_AUDIO_OVERVIEWS})
    public void testListenToFeedMenuItem_unavailableWhenReadAloudNotAvailable() {
        MockTab ntpTab = new MockTab(1, mProfile);
        ntpTab.setUrl(new GURL(getOriginalNativeNtpUrl()));

        setUpMocksForPageMenu();
        setMenuOptions(new MenuOptions());
        mActivityTabProvider.setForTesting(ntpTab);
        when(mReadAloudController.isAvailable()).thenReturn(false);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE)).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.listen_to_feed_id));
    }

    private boolean doTestShouldShowNewMenu(
            boolean isAutomotive,
            boolean isMultiInstanceEnabled,
            int currentWindowInstances,
            boolean isTabletSizeScreen,
            boolean isChromeRunningInAdjacentWindow,
            boolean isInMultiWindowMode,
            boolean isInMultiDisplayMode,
            boolean isMultiInstanceRunning) {
        if (isMultiInstanceEnabled) {
            MultiWindowTestUtils.enableMultiInstance();
            for (int i = 0; i < currentWindowInstances; ++i) {
                MultiWindowTestUtils.createInstance(
                        /* instanceId= */ i,
                        /* url= */ "https://url" + i,
                        /* tabCount= */ 1,
                        /* taskId= */ i);
            }
        }
        mShadowPackageManager.setSystemFeature(PackageManager.FEATURE_AUTOMOTIVE, isAutomotive);
        doReturn(isMultiInstanceEnabled)
                .when(mTabbedAppMenuPropertiesDelegate)
                .isMultiInstanceEnabled();
        doReturn(isTabletSizeScreen).when(mTabbedAppMenuPropertiesDelegate).isTabletSizeScreen();
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
        int windowLimit = 10;
        MultiWindowUtils.setMaxInstancesForTesting(windowLimit);
        assertFalse(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isMultiInstanceEnabled= */ true,
                        /* currentWindowInstances= */ windowLimit,
                        /* isTabletSizeScreen= */ true,
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
                        /* isMultiInstanceEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ true,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ false));
        verify(mTabbedAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
    }

    @Test
    public void testShouldShowNewMenu_multiInstanceDisabled_isAutomotive_returnsFalse() {
        assertFalse(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ true,
                        /* isMultiInstanceEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ true,
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
                        /* isMultiInstanceEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ true,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ false));
        assertFalse(mTabbedAppMenuPropertiesDelegate.shouldShowNewIncognitoWindow());
        verify(mTabbedAppMenuPropertiesDelegate, atLeastOnce()).isTabletSizeScreen();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testShouldShowNewMenu_isTabletSizedScreen_returnsTrue_withNewIncognitoWindow() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        assertTrue(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isMultiInstanceEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ true,
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
                        /* isMultiInstanceEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
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
                        /* isMultiInstanceEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
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
                        /* isMultiInstanceEnabled= */ true,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
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
                        /* isMultiInstanceEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ true));
        verify(mTabbedAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
    }

    @Test
    public void testShouldShowNewMenu_multiInstanceDisabled_multiWindowMode_returnsTrue() {
        assertTrue(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isMultiInstanceEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ true,
                        /* isInMultiDisplayMode= */ false,
                        /* isMultiInstanceRunning= */ false));
    }

    @Test
    public void testShouldShowNewMenu_multiInstanceDisabled_multiDisplayMode_returnsTrue() {
        assertTrue(
                doTestShouldShowNewMenu(
                        /* isAutomotive= */ false,
                        /* isMultiInstanceEnabled= */ false,
                        /* currentWindowInstances= */ 1,
                        /* isTabletSizeScreen= */ false,
                        /* isChromeRunningInAdjacentWindow= */ false,
                        /* isInMultiWindowMode= */ false,
                        /* isInMultiDisplayMode= */ true,
                        /* isMultiInstanceRunning= */ false));
    }

    private boolean doTestShouldShowMoveToOtherWindowMenu(
            boolean isInMultiDisplayMode, boolean isMoveToOtherWindowSupported) {
        doReturn(false).when(mTabbedAppMenuPropertiesDelegate).isMultiInstanceEnabled();
        doReturn(true).when(mTabbedAppMenuPropertiesDelegate).isTabletSizeScreen();
        doReturn(false).when(mMultiWindowModeStateDispatcher).isChromeRunningInAdjacentWindow();
        doReturn(false).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        doReturn(isInMultiDisplayMode).when(mMultiWindowModeStateDispatcher).isInMultiDisplayMode();
        doReturn(false).when(mMultiWindowModeStateDispatcher).isMultiInstanceRunning();
        doReturn(isMoveToOtherWindowSupported)
                .when(mMultiWindowModeStateDispatcher)
                .isMoveToOtherWindowSupported(any());

        return mTabbedAppMenuPropertiesDelegate.shouldShowMoveToOtherWindow();
    }

    @Test
    public void testShouldShowMoveToOtherWindow_returnsTrue() {
        assertTrue(
                doTestShouldShowMoveToOtherWindowMenu(
                        /* isInMultiDisplayMode= */ false,
                        /* isMoveToOtherWindowSupported= */ true));
    }

    @Test
    public void testShouldShowMoveToOtherWindow_dispatcherReturnsFalse_returnsFalse() {
        assertFalse(
                doTestShouldShowMoveToOtherWindowMenu(
                        /* isInMultiDisplayMode= */ true,
                        /* isMoveToOtherWindowSupported= */ false));
        verify(mTabbedAppMenuPropertiesDelegate, never()).isTabletSizeScreen();
    }

    @Test
    public void testReadAloudMenuItem_readAloudNotEnabled() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        setUpMocksForPageMenu();
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.readaloud_menu_id));
    }

    @Test
    public void testReadAloudMenuItem_notReadable() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mReadAloudController.isReadable(any())).thenReturn(false);
        setUpMocksForPageMenu();
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.readaloud_menu_id));
    }

    @Test
    public void testReadAloudMenuItem_readable() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mReadAloudController.isReadable(any())).thenReturn(true);
        setUpMocksForPageMenu();
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.readaloud_menu_id));
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
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getModelList();
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
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        ListItem parentItem = findItemById(modelList, R.id.extensions_parent_menu_id);
        assertNotNull("Extensions parent menu item not found.", parentItem);

        List<ListItem> isolatedExtensionMenu = Arrays.asList(parentItem);

        List<MenuItem> expectedItems =
                Arrays.asList(
                        item(
                                R.id.extensions_parent_menu_id,
                                item(R.id.extensions_menu_menu_id),
                                item(R.id.manage_extensions_menu_id),
                                item(R.id.extensions_webstore_menu_id)));

        List<MenuItem> expectedTitles =
                Arrays.asList(
                        item(
                                R.string.menu_extensions,
                                item(R.string.menu_extensions_menu),
                                item(R.string.menu_manage_extensions),
                                item(R.string.menu_chrome_webstore)));

        assertMenuItemsAreEqual(isolatedExtensionMenu, expectedItems);
        assertMenuTitlesAreEqual(isolatedExtensionMenu, expectedTitles);
        assertMenuItemsHaveIcons(isolatedExtensionMenu, expectedItems);
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
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        ListItem parentItem = findItemById(modelList, R.id.extensions_parent_menu_id);
        assertNull("Extensions parent menu item should NOT be present.", parentItem);

        ListItem originalItem = findItemById(modelList, R.id.extensions_menu_menu_id);
        assertNotNull("Original extensions menu item should be present.", originalItem);

        assertFalse(
                "Original extensions item should not have submenu properties.",
                originalItem.model.containsKey(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER));
        assertEquals(
                ContextUtils.getApplicationContext().getString(R.string.menu_extensions_menu),
                originalItem.model.get(AppMenuItemProperties.TITLE));
    }

    @Test
    public void testAddToGroup() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.tab_groups_parent_menu_id));
        assertTrue(
                isMenuVisible(
                        createModelList(getSubmenuItems(modelList, R.id.tab_groups_parent_menu_id)),
                        R.id.add_to_group_menu_id));
        assertTrue(
                isMenuVisible(
                        createModelList(getSubmenuItems(modelList, R.id.tab_groups_parent_menu_id)),
                        R.id.create_new_tab_group_menu_id));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testAddToGroup_SubmenusDisabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.add_to_group_menu_id));
        assertFalse(isMenuVisible(modelList, R.id.tab_groups_parent_menu_id));
    }

    @Test
    public void testAddToGroup_preInit() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.add_to_reading_list_menu_id));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testAddToGroup_preInit_SubmenusDisabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertFalse(isMenuVisible(modelList, R.id.add_to_group_menu_id));
    }

    private void setUpMocksForPageMenu() {
        mActivityTabProvider.setForTesting(mTab);
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

    private ModelList createMenuForMultiWindow() {
        doReturn(mIsMultiWindow).when(mMultiWindowModeStateDispatcher).isInMultiWindowMode();
        doReturn(mIsMultiWindowApiSupported)
                .when(mTabbedAppMenuPropertiesDelegate)
                .isMultiInstanceEnabled();
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
                ModelList modelList = createMenuForMultiWindow();
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

    private boolean isMenuVisible(ModelList modelList, int itemId) {
        return findItemById(modelList, itemId) != null;
    }

    private ModelList createModelList(List<ListItem> items) {
        ModelList modelList = new ModelList();
        if (items != null) {
            for (ListItem item : items) {
                modelList.add(item);
            }
        }
        return modelList;
    }

    private List<ListItem> getSubmenuItems(ModelList modelList, int parentId) {
        for (ListItem item : modelList) {
            if (item.model.get(AppMenuItemProperties.MENU_ITEM_ID) == parentId) {
                return item.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
            }
        }
        return null;
    }

    private boolean isMenuVisibleInSubmenu(ModelList modelList, int parentId, int itemId) {
        List<ListItem> submenu = getSubmenuItems(modelList, parentId);

        if (submenu == null) return false;

        for (ListItem subItem : submenu) {
            if (subItem.model.get(AppMenuItemProperties.MENU_ITEM_ID) == itemId) {
                return true;
            }
        }
        return false;
    }

    private boolean isMenuVisibleWithCorrectTitle(
            ModelList modelList, int itemId, String expectedTitle) {
        ListItem menuItem = findItemById(modelList, itemId);
        if (menuItem == null) return false;
        return menuItem.model.get(AppMenuItemProperties.TITLE) == expectedTitle;
    }

    private String getMenuTitle(ListItem item) {
        CharSequence title =
                item.model.containsKey(AppMenuItemProperties.TITLE)
                        ? item.model.get(AppMenuItemProperties.TITLE)
                        : null;
        if (title == null) {
            if (item.type == AppMenuHandler.AppMenuItemType.BUTTON_ROW) {
                title = "Icon Row";
            } else if (item.type == AppMenuHandler.AppMenuItemType.DIVIDER) {
                title = "Divider";
            } else if (item.type == AppMenuHandler.AppMenuItemType.EMPTY) {
                title = "Item for empty submenu";
            }
        }

        return title.toString();
    }

    private String getMenuTitles(Iterable<ListItem> modelList) {
        StringBuilder items = new StringBuilder();
        for (ListItem item : modelList) {
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

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_OVERLAY_ANDROID)
    public void lensOverlayItemEnabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertTrue(isMenuVisible(modelList, R.id.lens_overlay_menu_id));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_OVERLAY_ANDROID)
    public void lensOverlayItemDisabled_Incognito() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(isMenuVisible(modelList, R.id.lens_overlay_menu_id));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_OVERLAY_ANDROID)
    public void lensOverlayItemDisabled_NotHttp() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isIncognito()).thenReturn(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(isMenuVisible(modelList, R.id.lens_overlay_menu_id));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.LENS_OVERLAY_ANDROID)
    public void lensOverlayItemDisabled_FeatureDisabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(isMenuVisible(modelList, R.id.lens_overlay_menu_id));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LENS_OVERLAY_ANDROID)
    public void lensOverlayItemDisabled_OverlayShowing() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(false);

        // 1. Default state: Overlay is NOT showing.
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.lens_overlay_menu_id));
        ListItem item = findItemById(modelList, R.id.lens_overlay_menu_id);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));

        // 2. State change: Overlay is NOW showing.
        LensOverlayTabHelper.setOverlayShowing(mTab, true);
        modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.lens_overlay_menu_id));
        item = findItemById(modelList, R.id.lens_overlay_menu_id);
        assertFalse(item.model.get(AppMenuItemProperties.ENABLED));

        // 3. State reset: Overlay is NO LONGER showing.
        LensOverlayTabHelper.setOverlayShowing(mTab, false);
        modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        assertTrue(isMenuVisible(modelList, R.id.lens_overlay_menu_id));
        item = findItemById(modelList, R.id.lens_overlay_menu_id);
        assertTrue(item.model.get(AppMenuItemProperties.ENABLED));
    }

    @Test
    public void testHelpMenuItem_PolicyDisabled() {
        setUpMocksForPageMenu();
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        ListItem helpParentItem = findItemById(modelList, R.id.help_parent_menu_id);
        assertNotNull(helpParentItem);

        assertTrue(
                helpParentItem.model.containsKey(
                        AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER));
        List<ListItem> subItems =
                helpParentItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        assertNotNull(subItems);

        // Should contain "About Chrome" and "Help Center", but NOT "Report an issue".
        assertNotNull(findItemById(subItems, R.id.about_chrome_menu_id));
        assertNotNull(findItemById(subItems, R.id.help_id));
        assertNull(findItemById(subItems, R.id.report_issue_menu_id));
    }

    @Test
    public void testHelpMenuItem_PolicyEnabled() {
        setUpMocksForPageMenu();
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        ListItem helpParentItem = findItemById(modelList, R.id.help_parent_menu_id);
        assertNotNull(helpParentItem);

        assertTrue(
                helpParentItem.model.containsKey(
                        AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER));
        List<ListItem> subItems =
                helpParentItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        assertNotNull(subItems);

        // Should contain "About Chrome", "Help Center" and "Report an issue".
        assertNotNull(findItemById(subItems, R.id.about_chrome_menu_id));
        assertNotNull(findItemById(subItems, R.id.help_id));
        assertNotNull(findItemById(subItems, R.id.report_issue_menu_id));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void glicItemEnabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(false);
        // TODO (crbug.com/518937617): Use GlicEnabling.setEnabledForTesting
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertTrue(isMenuVisible(modelList, R.id.glic_menu_id));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void glicItemDisabled_Incognito() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(true);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(isMenuVisible(modelList, R.id.glic_menu_id));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC)
    public void glicItemDisabled_FeatureDisabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(false);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(isMenuVisible(modelList, R.id.glic_menu_id));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void glicItemDisabled_NotEnabled() {
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(false);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();

        assertFalse(isMenuVisible(modelList, R.id.glic_menu_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testToggleBookmarksBarMenuItemString() {
        when(mTab.getUrl()).thenReturn(org.chromium.url.JUnitTestGURLs.EXAMPLE_URL);
        setUpMocksForPageMenu();

        // Bookmark bar is visible.
        when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR)).thenReturn(true);
        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        ListItem bookmarksParent = findItemById(modelList, R.id.bookmarks_parent_menu_id);
        assertNotNull(bookmarksParent);
        List<ListItem> subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem toggleItem = null;
        for (ListItem item : subItems) {
            if (item.model.get(AppMenuItemProperties.MENU_ITEM_ID)
                    == R.id.toggle_bookmarks_bar_menu_id) {
                toggleItem = item;
                break;
            }
        }
        assertNotNull(toggleItem);
        assertEquals(
                ContextUtils.getApplicationContext().getString(R.string.menu_hide_bookmarks_bar),
                toggleItem.model.get(AppMenuItemProperties.TITLE));

        // Bookmark bar is hidden.
        when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        bookmarksParent = findItemById(modelList, R.id.bookmarks_parent_menu_id);
        subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        toggleItem = null;
        for (ListItem item : subItems) {
            if (item.model.get(AppMenuItemProperties.MENU_ITEM_ID)
                    == R.id.toggle_bookmarks_bar_menu_id) {
                toggleItem = item;
                break;
            }
        }
        assertNotNull(toggleItem);
        assertEquals(
                ContextUtils.getApplicationContext().getString(R.string.menu_show_bookmarks_bar),
                toggleItem.model.get(AppMenuItemProperties.TITLE));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testBookmarkMenu_NoBookmarks() {
        mBookmarkModel.removeAllUserBookmarks();
        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        ListItem bookmarksParent = findItemById(modelList, R.id.bookmarks_parent_menu_id);
        assertNotNull(bookmarksParent);

        List<ListItem> subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        List<MenuItem> expectedSubItems =
                Arrays.asList(
                        item(R.id.all_bookmarks_menu_id),
                        item(R.id.bookmark_this_page_menu_id),
                        item(R.id.toggle_bookmarks_bar_menu_id),
                        item(R.id.divider_line_id),
                        item(
                                R.id.reading_list_parent_menu_id,
                                item(R.id.add_to_reading_list_menu_id),
                                item(R.id.show_reading_list_menu_id)),
                        item(R.id.divider_line_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_folder_menu_id, item(0))),
                        item(R.id.bookmark_folder_menu_id, item(0)));

        assertMenuItemsAreEqual(subItems, expectedSubItems);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testBookmarkMenu_NestedFolders() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Folder 2");
        mBookmarkModel.addBookmark(folderId, 0, "Bookmark 1", JUnitTestGURLs.URL_1);
        BookmarkId nestedFolderId = mBookmarkModel.addFolder(folderId, 1, "Nested Folder");
        mBookmarkModel.addBookmark(nestedFolderId, 0, "Nested Bookmark", JUnitTestGURLs.URL_2);

        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        ListItem bookmarksParent = findItemById(modelList, R.id.bookmarks_parent_menu_id);
        assertNotNull(bookmarksParent);

        List<ListItem> subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        List<MenuItem> expectedSubItems =
                Arrays.asList(
                        item(R.id.all_bookmarks_menu_id),
                        item(R.id.bookmark_this_page_menu_id),
                        item(R.id.toggle_bookmarks_bar_menu_id),
                        item(R.id.divider_line_id),
                        item(
                                R.id.reading_list_parent_menu_id,
                                item(R.id.add_to_reading_list_menu_id),
                                item(R.id.show_reading_list_menu_id)),
                        item(R.id.divider_line_id),
                        item(R.id.bookmark_menu_id),
                        item(R.id.bookmark_menu_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_menu_id),
                                item(R.id.bookmark_menu_id)),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_menu_id),
                                item(R.id.bookmark_folder_menu_id, item(R.id.bookmark_menu_id))),
                        item(R.id.divider_line_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_folder_menu_id, item(0))),
                        item(R.id.bookmark_folder_menu_id, item(0)));

        assertMenuItemsAreEqual(subItems, expectedSubItems);

        List<MenuItem> expectedTitles =
                Arrays.asList(
                        item(R.string.menu_bookmarks),
                        item(R.string.menu_bookmark_this_page),
                        item(R.string.menu_show_bookmarks_bar),
                        item(0),
                        item(
                                R.string.menu_reading_list,
                                item(R.string.menu_add_to_reading_list),
                                item(R.string.menu_show_reading_list)),
                        item(0),
                        item("Bookmark 1"),
                        item("Bookmark 2"),
                        item(
                                "Folder 1",
                                item("Bookmark in folder 1"),
                                item("Bookmark in folder 2")),
                        item(
                                "Folder 2",
                                item("Bookmark 1"),
                                item("Nested Folder", item("Nested Bookmark"))),
                        item(0),
                        item(R.string.menu_mobile_bookmarks, item("Partner bookmarks", item(0))),
                        item(R.string.menu_other_bookmarks, item(0)));
        assertMenuTitlesAreEqual(subItems, expectedTitles);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testBookmarkMenu_EmptyFolder() {
        mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Empty Folder");

        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        ListItem bookmarksParent = findItemById(modelList, R.id.bookmarks_parent_menu_id);
        assertNotNull(bookmarksParent);

        List<ListItem> subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        List<MenuItem> expectedSubItems =
                Arrays.asList(
                        item(R.id.all_bookmarks_menu_id),
                        item(R.id.bookmark_this_page_menu_id),
                        item(R.id.toggle_bookmarks_bar_menu_id),
                        item(R.id.divider_line_id),
                        item(
                                R.id.reading_list_parent_menu_id,
                                item(R.id.add_to_reading_list_menu_id),
                                item(R.id.show_reading_list_menu_id)),
                        item(R.id.divider_line_id),
                        item(R.id.bookmark_menu_id),
                        item(R.id.bookmark_menu_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_menu_id),
                                item(R.id.bookmark_menu_id)),
                        item(R.id.bookmark_folder_menu_id, item(0)),
                        item(R.id.divider_line_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_folder_menu_id, item(0))),
                        item(R.id.bookmark_folder_menu_id, item(0)));

        assertMenuItemsAreEqual(subItems, expectedSubItems);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    @SuppressWarnings("unchecked")
    public void testBookmarkMenu_Favicons() {
        BookmarkId bookmarkId = mBookmarkModel.getChildAt(mBookmarkModel.getDesktopFolderId(), 0);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);

        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        ListItem bookmarksParent = findItemById(modelList, R.id.bookmarks_parent_menu_id);
        List<ListItem> subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        ListItem bookmarkListItem = findItemById(subItems, R.id.bookmark_menu_id);
        assertNotNull(bookmarkListItem);

        LazyOneshotSupplier<Drawable> iconSupplier =
                bookmarkListItem.model.get(AppMenuItemProperties.ICON_SUPPLIER);
        assertNotNull(iconSupplier);

        Drawable mockFavicon = mock(Drawable.class);
        doAnswer(
                        invocation -> {
                            ((Callback<Drawable>) invocation.getArgument(1)).onResult(mockFavicon);
                            return null;
                        })
                .when(mBookmarkImageFetcher)
                .fetchFaviconForBookmark(eq(bookmarkItem), any());

        // Accessing the supplier should trigger the fetch.
        iconSupplier.get();

        verify(mBookmarkImageFetcher).fetchFaviconForBookmark(eq(bookmarkItem), any());
        assertEquals(mockFavicon, iconSupplier.get());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testTabGroupsSubmenu_WithGroups() {
        TabModel tabModel = Mockito.mock(TabModel.class);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(tabModel);
        when(tabModel.getProfile()).thenReturn(mProfile);

        Token token1 = new Token(1L, 1L);
        when(tabModel.getTabGroupCount()).thenReturn(1);
        when(tabModel.getAllTabGroupIds()).thenReturn(java.util.Set.of(token1));
        when(tabModel.getTabGroupTitle(token1)).thenReturn("Group 1");
        when(tabModel.getTabGroupColorWithFallback(token1))
                .thenReturn(org.chromium.components.tab_groups.TabGroupColorId.BLUE);

        Tab tab1 = Mockito.mock(Tab.class);
        when(tab1.getId()).thenReturn(101);
        when(tab1.getTitle()).thenReturn("Tab 1");
        when(tab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        Tab tab2 = Mockito.mock(Tab.class);
        when(tab2.getId()).thenReturn(102);
        when(tab2.getTitle()).thenReturn("Tab 2");
        when(tab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);

        when(tabModel.getTabsInGroup(token1)).thenReturn(Arrays.asList(tab1, tab2));

        setUpMocksForPageMenu();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ListItem tabGroupsParent =
                findItemById(
                        mTabbedAppMenuPropertiesDelegate.getMenuItems(),
                        R.id.tab_groups_parent_menu_id);
        assertNotNull(tabGroupsParent);

        List<ListItem> tabGroupsSubmenuItems =
                tabGroupsParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        List<MenuItem> expectedItems =
                Arrays.asList(
                        item(R.id.add_to_group_menu_id),
                        item(R.id.create_new_tab_group_menu_id),
                        item(R.id.divider_line_id),
                        item(
                                R.id.tab_group_menu_item_id,
                                item(R.id.tab_group_tab_menu_item),
                                item(R.id.tab_group_tab_menu_item)));
        assertMenuItemsAreEqual(tabGroupsSubmenuItems, expectedItems);

        List<MenuItem> expectedTitles =
                Arrays.asList(
                        item(R.string.menu_add_tab_to_group),
                        item(R.string.menu_create_new_tab_group),
                        item(0),
                        item("Group 1", item("Tab 1"), item("Tab 2")));
        assertMenuTitlesAreEqual(tabGroupsSubmenuItems, expectedTitles);

        ListItem groupItem = tabGroupsSubmenuItems.get(3);
        List<ListItem> tabsSubmenuItems =
                groupItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        ListItem tabItem1 = tabsSubmenuItems.get(0);
        assertEquals(AppMenuHandler.AppMenuItemType.TAB, tabItem1.type);
        assertEquals(101, tabItem1.model.get(AppMenuTabItemProperties.TAB_ID));
        Bundle bundle1 = mTabbedAppMenuPropertiesDelegate.getBundleForMenuItem(tabItem1.model);
        assertNotNull("Bundle 1 should not be null", bundle1);
        assertEquals(101, bundle1.getInt(AppMenuPropertiesDelegateImpl.TAB_ID_BUNDLE_KEY));

        ListItem tabItem2 = tabsSubmenuItems.get(1);
        assertEquals(AppMenuHandler.AppMenuItemType.TAB, tabItem2.type);
        assertEquals(102, tabItem2.model.get(AppMenuTabItemProperties.TAB_ID));
        Bundle bundle2 = mTabbedAppMenuPropertiesDelegate.getBundleForMenuItem(tabItem2.model);
        assertNotNull("Bundle 2 should not be null", bundle2);
        assertEquals(102, bundle2.getInt(AppMenuPropertiesDelegateImpl.TAB_ID_BUNDLE_KEY));
    }

    private Tab setUpMockTabGroup(TabModel tabModel, boolean isIncognito, boolean hasGroupId) {
        Token token1 = new Token(1L, 1L);
        when(tabModel.getTabGroupCount()).thenReturn(1);
        when(tabModel.getAllTabGroupIds()).thenReturn(java.util.Set.of(token1));
        when(tabModel.getTabGroupTitle(token1)).thenReturn("Group 1");
        when(tabModel.getTabGroupColorWithFallback(token1))
                .thenReturn(org.chromium.components.tab_groups.TabGroupColorId.BLUE);

        Tab tab = Mockito.mock(Tab.class);
        when(tab.getId()).thenReturn(101);
        when(tab.getTitle()).thenReturn("Tab 1");
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(tab.getTabGroupId()).thenReturn(hasGroupId ? token1 : null);
        when(tab.isOffTheRecord()).thenReturn(isIncognito);
        when(tab.isInitialized()).thenReturn(true);
        when(tab.isDestroyed()).thenReturn(false);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());

        when(tabModel.getTabsInGroup(token1)).thenReturn(Arrays.asList(tab));

        return tab;
    }

    @Test
    public void testTabGroupsSubmenu_Favicons_GroupedNonIncognito() {
        // Configure an non-incognito TabModel.
        TabModel tabModel = Mockito.mock(TabModel.class);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(tabModel);
        when(tabModel.getProfile()).thenReturn(mProfile);
        setUpMockTabGroup(tabModel, /* isIncognito= */ false, /* hasGroupId= */ true);
        GURL tabUrl = JUnitTestGURLs.URL_1;

        setUpMocksForPageMenu();

        // Intercept the JNI callback and invoke it synchronously with a mock favicon bitmap.
        Answer<Boolean> faviconCallbackAnswer =
                invocation -> {
                    FaviconHelper.FaviconImageCallback callback = invocation.getArgument(5);
                    callback.onFaviconAvailable(
                            Bitmap.createBitmap(10, 10, android.graphics.Bitmap.Config.ARGB_8888),
                            tabUrl);
                    return true;
                };

        // Should call {@code getForeignFaviconImageForURL()} because it is not incognito.
        doAnswer(faviconCallbackAnswer)
                .when(mFaviconHelperJniMock)
                .getForeignFaviconImageForURL(
                        eq(1L), eq(mProfile), eq(tabUrl), anyInt(), eq(false), any());

        // Get the first tab group item in the menu.
        ListItem tabGroupItem =
                findItemById(
                        findItemById(
                                        mTabbedAppMenuPropertiesDelegate.getMenuItems(),
                                        R.id.tab_groups_parent_menu_id)
                                .model
                                .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                                .get(),
                        R.id.tab_group_menu_item_id);
        assertNotNull(tabGroupItem);

        // Get the first tab item in that group.
        LazyOneshotSupplier<Drawable> iconSupplier =
                tabGroupItem
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get()
                        .get(0)
                        .model
                        .get(AppMenuItemProperties.ICON_SUPPLIER);

        Drawable drawable = iconSupplier.get();
        assertNotNull(drawable);
    }

    @Test
    public void testTabGroupsSubmenu_Favicons_GroupedIncognito() {
        // Configure an incognito TabModel.
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        setUpMockTabGroup(mIncognitoTabModel, /* isIncognito= */ true, /* hasGroupId= */ true);
        GURL tabUrl = JUnitTestGURLs.URL_1;

        setUpMocksForPageMenu();
        when(mTab.isIncognito()).thenReturn(true);

        // Intercept the JNI callback and invoke it synchronously with a mock favicon bitmap.
        Answer<Boolean> faviconCallbackAnswer =
                invocation -> {
                    FaviconHelper.FaviconImageCallback callback = invocation.getArgument(5);
                    callback.onFaviconAvailable(
                            Bitmap.createBitmap(10, 10, android.graphics.Bitmap.Config.ARGB_8888),
                            tabUrl);
                    return true;
                };

        // Should call {@code getLocalFaviconImageForURL()} because it is incognito.
        doAnswer(faviconCallbackAnswer)
                .when(mFaviconHelperJniMock)
                .getLocalFaviconImageForURL(
                        eq(1L), eq(mProfile), eq(tabUrl), anyInt(), eq(false), any());

        // Get the first tab group item in the menu.
        ListItem tabGroupItem =
                findItemById(
                        findItemById(
                                        mTabbedAppMenuPropertiesDelegate.getMenuItems(),
                                        R.id.tab_groups_parent_menu_id)
                                .model
                                .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                                .get(),
                        R.id.tab_group_menu_item_id);
        assertNotNull(tabGroupItem);

        // Get the first tab item in that group.
        LazyOneshotSupplier<Drawable> iconSupplier =
                tabGroupItem
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get()
                        .get(0)
                        .model
                        .get(AppMenuItemProperties.ICON_SUPPLIER);

        Drawable drawable = iconSupplier.get();
        assertNotNull(drawable);
    }

    @Test
    public void testHomepageMenuItem_shouldShow() {
        setUpMocksForPageMenu();

        HomepageManager homepageManagerMock = mock(HomepageManager.class);
        HomepageManager.setInstanceForTesting(homepageManagerMock);
        when(homepageManagerMock.shouldShowHomepageMenuItem()).thenReturn(true);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        ListItem homepageItem = findItemById(modelList, R.id.homepage_menu_id);
        assertNotNull("Homepage menu item should be visible", homepageItem);
    }

    @Test
    public void testHomepageMenuItem_shouldNotShow() {
        setUpMocksForPageMenu();

        HomepageManager homepageManagerMock = mock(HomepageManager.class);
        HomepageManager.setInstanceForTesting(homepageManagerMock);
        when(homepageManagerMock.shouldShowHomepageMenuItem()).thenReturn(false);

        ModelList modelList = mTabbedAppMenuPropertiesDelegate.getMenuItems();
        ListItem homepageItem = findItemById(modelList, R.id.homepage_menu_id);
        assertNull("Homepage menu item should not be visible", homepageItem);
    }

    @Test
    public void testHistorySubmenu_WithRecentEntries() {
        setUpMocksForPageMenu();

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
                                item(R.id.quick_delete_menu_id),
                                item(R.id.divider_line_id),
                                item(R.id.recent_entry_tab_menu_item),
                                item(R.id.recent_entry_tab_menu_item)));

        List<ListItem> items =
                findItemById(
                                mTabbedAppMenuPropertiesDelegate.getMenuItems(),
                                R.id.history_parent_menu_id)
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        assertMenuTreesAreEqual(
                items,
                expectedSubmenu,
                (item, expectedId) -> {
                    assertEquals(
                            "Mismatched item id",
                            expectedId,
                            item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
                });
    }

    @Test
    public void testHistorySubmenu_WithRecentlyClosedWindow() {
        setUpMocksForPageMenu();

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
                                item(R.id.quick_delete_menu_id),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.recent_entry_menu_item,
                                        item(R.id.recent_entry_window_menu_item),
                                        item(R.id.divider_line_id),
                                        item(R.id.recent_entry_tab_menu_item),
                                        item(R.id.recent_entry_tab_menu_item))));

        List<ListItem> items =
                findItemById(
                                mTabbedAppMenuPropertiesDelegate.getMenuItems(),
                                R.id.history_parent_menu_id)
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        assertMenuTreesAreEqual(
                items,
                expectedSubmenu,
                (item, expectedId) -> {
                    assertEquals(
                            "Mismatched item id",
                            expectedId,
                            item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
                });

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
                                item(R.string.menu_quick_delete),
                                item(0),
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
        // actions: History, Recent Tabs, Quick Delete, and the Divider).
        ListItem windowItem = items.get(4);
        List<ListItem> windowSubmenu =
                windowItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem restoreItem = windowSubmenu.get(0);
        assertEquals(
                closedWindow, restoreItem.model.get(AppMenuRecentEntryItemProperties.RECENT_ENTRY));
    }

    @Test
    public void testHistorySubmenu_WithUnnamedRecentlyClosedWindow() {
        setUpMocksForPageMenu();

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
                                item(R.id.quick_delete_menu_id),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.recent_entry_menu_item,
                                        item(R.id.recent_entry_window_menu_item))));

        List<ListItem> items =
                findItemById(
                                mTabbedAppMenuPropertiesDelegate.getMenuItems(),
                                R.id.history_parent_menu_id)
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        assertMenuTreesAreEqual(
                items,
                expectedSubmenu,
                (item, expectedId) -> {
                    assertEquals(
                            "Mismatched item id",
                            expectedId,
                            item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
                });

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
                                item(R.string.menu_quick_delete),
                                item(0),
                                item(tabsText, item(restoreText))));

        assertMenuTitlesAreEqual(items, expectedTitles);

        // Verify the recent entry itself in the model.
        // Index 4 is the first recently closed entry in the submenu (after the default history
        // actions: History, Recent Tabs, Quick Delete, and the Divider).
        ListItem windowItem = items.get(4);
        List<ListItem> windowSubmenu =
                windowItem.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem restoreItem = windowSubmenu.get(0);
        assertEquals(
                closedWindow, restoreItem.model.get(AppMenuRecentEntryItemProperties.RECENT_ENTRY));
    }

    private void runHistorySubmenuWithRecentlyClosedGroupTest(
            String title, List<MenuItem> expectedTitles) {
        setUpMocksForPageMenu();

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
                                item(R.id.quick_delete_menu_id),
                                item(R.id.divider_line_id),
                                item(
                                        R.id.recent_entry_menu_item,
                                        item(R.id.recent_entry_group_menu_item),
                                        item(R.id.divider_line_id),
                                        item(R.id.recent_entry_tab_menu_item),
                                        item(R.id.recent_entry_tab_menu_item))));

        List<ListItem> items =
                findItemById(
                                mTabbedAppMenuPropertiesDelegate.getMenuItems(),
                                R.id.history_parent_menu_id)
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();

        assertMenuTreesAreEqual(
                items,
                expectedSubmenu,
                (item, expectedId) -> {
                    assertEquals(
                            "Mismatched item id",
                            expectedId,
                            item.model.get(AppMenuItemProperties.MENU_ITEM_ID));
                });

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
                                item(R.string.menu_quick_delete),
                                item(0),
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
                                item(R.string.menu_quick_delete),
                                item(0),
                                item(
                                        tabsText,
                                        item(restoreText),
                                        item(0),
                                        item("Tab 1 Title"),
                                        item("Tab 2 Title"))));

        runHistorySubmenuWithRecentlyClosedGroupTest("", expectedTitles);
    }

    private static MenuItem item(Object id, MenuItem... subItems) {
        return new MenuItem(id, subItems);
    }
}
