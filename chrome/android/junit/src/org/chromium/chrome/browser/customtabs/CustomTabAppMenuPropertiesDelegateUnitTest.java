// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.test.core.app.ApplicationProvider;

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
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionsToolbarCoordinator;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.chrome.browser.ui.extensions.ExtensionUiBackend;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderLayoutCoordinator;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.url.GURL;

import java.util.ArrayList;

/** Unit tests for {@link CustomTabAppMenuPropertiesDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabAppMenuPropertiesDelegateUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mTab;
    @Mock private NavigationController mNavigationController;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private View mDecorView;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private WebContents mWebContents;
    @Mock private Profile mProfile;
    @Mock private TranslateBridge.Natives mTranslateBridgeJniMock;
    @Mock private ShoppingService mShoppingService;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;

    @Mock private Verifier mVerifier;

    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();
    private final SettableMonotonicObservableSupplier<BookmarkModel> mBookmarkModelSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<ReadAloudController>
            mReadAloudControllerSupplier = ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        mActivityTabProvider.setForTesting(mTab);
        when(mTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mTab.isNativePage()).thenReturn(false);
    }

    private boolean isMenuItemPresent(MVCListAdapter.ModelList modelList, @IdRes int itemId) {
        for (MVCListAdapter.ListItem item : modelList) {
            if (item.model.get(AppMenuItemProperties.MENU_ITEM_ID) == itemId) return true;
        }
        return false;
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_ADAPTIVE_BUTTON})
    public void enablePriceTrackingItemRow() {
        mBookmarkModelSupplier.set(mBookmarkModel);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());
        doReturn(mock(BookmarkId.class)).when(mBookmarkModel).getUserBookmarkIdForTab(any());
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mTab.getProfile()).thenReturn(mProfile);
        TranslateBridgeJni.setInstanceForTesting(mTranslateBridgeJniMock);
        Mockito.when(mTranslateBridgeJniMock.canManuallyTranslate(any(), anyBoolean()))
                .thenReturn(false);
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setIsPriceTracked(false).build())
                        .build();
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(any());
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        var delegate =
                new CustomTabAppMenuPropertiesDelegate(
                        context,
                        mActivityTabProvider,
                        mMultiWindowModeStateDispatcher,
                        mTabModelSelector,
                        mToolbarManager,
                        mDecorView,
                        mBookmarkModelSupplier,
                        mVerifier,
                        CustomTabsUiType.AUTH_TAB,
                        /* menuEntries= */ new ArrayList<>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        /* contextualPageActionControllerSupplier= */ SupplierUtils.ofNull(),
                        /* hasClientPackage= */ false,
                        /* pageZoomManager= */ null,
                        /* openInAppMenuItemProvider= */ null,
                        /* webAppHeaderLayoutCoordinatorSupplier= */ () -> null);
        MVCListAdapter.ModelList modelList = delegate.getMenuItems();
        assertTrue(isMenuItemPresent(modelList, R.id.enable_price_tracking_menu_id));
        assertFalse(isMenuItemPresent(modelList, R.id.disable_price_tracking_menu_id));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_ADAPTIVE_BUTTON})
    public void enablePriceInsightsMenu() {
        ContextualPageActionController cpac = mock(ContextualPageActionController.class);
        doReturn(true).when(cpac).hasPriceInsights();

        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        var delegate =
                new CustomTabAppMenuPropertiesDelegate(
                        context,
                        mActivityTabProvider,
                        mMultiWindowModeStateDispatcher,
                        mTabModelSelector,
                        mToolbarManager,
                        mDecorView,
                        mBookmarkModelSupplier,
                        mVerifier,
                        CustomTabsUiType.AUTH_TAB,
                        /* menuEntries= */ new ArrayList<>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        () -> cpac,
                        /* hasClientPackage= */ false,
                        /* pageZoomManager= */ null,
                        /* openInAppMenuItemProvider= */ null,
                        /* webAppHeaderLayoutCoordinatorSupplier= */ () -> null);
        MVCListAdapter.ModelList modelList = delegate.getMenuItems();
        assertTrue(isMenuItemPresent(modelList, R.id.price_insights_menu_id));
    }

    @Test
    public void authTabMenuItemVisibility() {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        var delegate =
                new CustomTabAppMenuPropertiesDelegate(
                        context,
                        mActivityTabProvider,
                        mMultiWindowModeStateDispatcher,
                        mTabModelSelector,
                        mToolbarManager,
                        mDecorView,
                        mBookmarkModelSupplier,
                        mVerifier,
                        CustomTabsUiType.AUTH_TAB,
                        /* menuEntries= */ new ArrayList<>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        /* contextualPageActionControllerSupplier= */ SupplierUtils.ofNull(),
                        /* hasClientPackage= */ false,
                        /* pageZoomManager= */ null,
                        /* openInAppMenuItemProvider= */ null,
                        /* webAppHeaderLayoutCoordinatorSupplier= */ () -> null);
        MVCListAdapter.ModelList modelList = delegate.getMenuItems();

        assertTrue(isMenuItemPresent(modelList, R.id.find_in_page_id));

        // Verify the following 5 menu items are hidden.
        assertFalse(isMenuItemPresent(modelList, R.id.bookmark_this_page_id));
        assertFalse(isMenuItemPresent(modelList, R.id.offline_page_id));
        assertFalse(isMenuItemPresent(modelList, R.id.share_menu_id));
        assertFalse(isMenuItemPresent(modelList, R.id.universal_install));
        assertFalse(isMenuItemPresent(modelList, R.id.open_in_browser_id));
    }

    @Test
    public void popupMenuItemVisibility() {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        var delegate =
                new CustomTabAppMenuPropertiesDelegate(
                        context,
                        mActivityTabProvider,
                        mMultiWindowModeStateDispatcher,
                        mTabModelSelector,
                        mToolbarManager,
                        mDecorView,
                        mBookmarkModelSupplier,
                        mVerifier,
                        CustomTabsUiType.POPUP,
                        /* menuEntries= */ new ArrayList<>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        /* contextualPageActionControllerSupplier= */ SupplierUtils.ofNull(),
                        /* hasClientPackage= */ false,
                        /* pageZoomManager= */ null,
                        /* openInAppMenuItemProvider= */ null,
                        /* webAppHeaderLayoutCoordinatorSupplier= */ () -> null);
        MVCListAdapter.ModelList modelList = delegate.getMenuItems();

        assertTrue(isMenuItemPresent(modelList, R.id.find_in_page_id));

        // Verify the following 6 menu items are hidden.
        assertFalse(isMenuItemPresent(modelList, R.id.open_in_browser_id));
        assertFalse(isMenuItemPresent(modelList, R.id.bookmark_this_page_id));
        assertFalse(isMenuItemPresent(modelList, R.id.offline_page_id));
        assertFalse(isMenuItemPresent(modelList, R.id.universal_install));
        assertFalse(isMenuItemPresent(modelList, R.id.request_desktop_site_id));
        assertFalse(isMenuItemPresent(modelList, R.id.readaloud_menu_id));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testExtensionsMenuItem_TwaWithExtensionsEnabled() {
        ExtensionUiBackend backend = mock(ExtensionUiBackend.class);
        when(backend.isEnabled(any())).thenReturn(true);
        ExtensionUi.setBackendForTesting(backend);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getModel(false)).thenReturn(tabModel);

        WebAppHeaderLayoutCoordinator headerCoordinator = mock(WebAppHeaderLayoutCoordinator.class);
        ExtensionsToolbarCoordinator extensionsToolbarCoordinator =
                mock(ExtensionsToolbarCoordinator.class);

        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        // 1. When header coordinator is null -> extensions menu item not shown.
        var delegateNoHeader =
                new CustomTabAppMenuPropertiesDelegate(
                        context,
                        mActivityTabProvider,
                        mMultiWindowModeStateDispatcher,
                        mTabModelSelector,
                        mToolbarManager,
                        mDecorView,
                        mBookmarkModelSupplier,
                        mVerifier,
                        CustomTabsUiType.TRUSTED_WEB_ACTIVITY,
                        /* menuEntries= */ new ArrayList<>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        /* contextualPageActionControllerSupplier= */ SupplierUtils.ofNull(),
                        /* hasClientPackage= */ false,
                        /* pageZoomManager= */ null,
                        /* openInAppMenuItemProvider= */ null,
                        /* webAppHeaderLayoutCoordinatorSupplier= */ () -> null);
        assertFalse(
                isMenuItemPresent(delegateNoHeader.getMenuItems(), R.id.extensions_parent_menu_id));

        // 2. When header coordinator exists, but extensions toolbar coordinator is null -> not
        // shown.
        when(headerCoordinator.getExtensionsToolbarCoordinator()).thenReturn(null);
        var delegateNoExtensionsToolbar =
                new CustomTabAppMenuPropertiesDelegate(
                        context,
                        mActivityTabProvider,
                        mMultiWindowModeStateDispatcher,
                        mTabModelSelector,
                        mToolbarManager,
                        mDecorView,
                        mBookmarkModelSupplier,
                        mVerifier,
                        CustomTabsUiType.TRUSTED_WEB_ACTIVITY,
                        /* menuEntries= */ new ArrayList<>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        /* contextualPageActionControllerSupplier= */ SupplierUtils.ofNull(),
                        /* hasClientPackage= */ false,
                        /* pageZoomManager= */ null,
                        /* openInAppMenuItemProvider= */ null,
                        /* webAppHeaderLayoutCoordinatorSupplier= */ () -> headerCoordinator);
        assertFalse(
                isMenuItemPresent(
                        delegateNoExtensionsToolbar.getMenuItems(),
                        R.id.extensions_parent_menu_id));

        // 3. When both header coordinator and extensions toolbar coordinator exist -> shown.
        when(headerCoordinator.getExtensionsToolbarCoordinator())
                .thenReturn(extensionsToolbarCoordinator);
        var delegateWithExtensions =
                new CustomTabAppMenuPropertiesDelegate(
                        context,
                        mActivityTabProvider,
                        mMultiWindowModeStateDispatcher,
                        mTabModelSelector,
                        mToolbarManager,
                        mDecorView,
                        mBookmarkModelSupplier,
                        mVerifier,
                        CustomTabsUiType.TRUSTED_WEB_ACTIVITY,
                        /* menuEntries= */ new ArrayList<>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        /* contextualPageActionControllerSupplier= */ SupplierUtils.ofNull(),
                        /* hasClientPackage= */ false,
                        /* pageZoomManager= */ null,
                        /* openInAppMenuItemProvider= */ null,
                        /* webAppHeaderLayoutCoordinatorSupplier= */ () -> headerCoordinator);
        assertTrue(
                isMenuItemPresent(
                        delegateWithExtensions.getMenuItems(), R.id.extensions_parent_menu_id));

        ExtensionUi.setBackendForTesting(null);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testExtensionsMenuItem_PreVanillaIceCream_NotShown() {
        ExtensionUiBackend backend = mock(ExtensionUiBackend.class);
        when(backend.isEnabled(any())).thenReturn(true);
        ExtensionUi.setBackendForTesting(backend);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getModel(false)).thenReturn(tabModel);

        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        var delegate =
                new CustomTabAppMenuPropertiesDelegate(
                        context,
                        mActivityTabProvider,
                        mMultiWindowModeStateDispatcher,
                        mTabModelSelector,
                        mToolbarManager,
                        mDecorView,
                        mBookmarkModelSupplier,
                        mVerifier,
                        CustomTabsUiType.TRUSTED_WEB_ACTIVITY,
                        /* menuEntries= */ new ArrayList<>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        /* contextualPageActionControllerSupplier= */ SupplierUtils.ofNull(),
                        /* hasClientPackage= */ false,
                        /* pageZoomManager= */ null,
                        /* openInAppMenuItemProvider= */ null,
                        /* webAppHeaderLayoutCoordinatorSupplier= */ () -> null);
        assertFalse(
                "Extensions menu item should not be shown on SDK < VANILLA_ICE_CREAM",
                isMenuItemPresent(delegate.getMenuItems(), R.id.extensions_parent_menu_id));

        ExtensionUi.setBackendForTesting(null);
    }

    private CustomTabAppMenuPropertiesDelegate createPropertiesDelegate(
            @CustomTabsUiType int uiType) {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        return new CustomTabAppMenuPropertiesDelegate(
                context,
                mActivityTabProvider,
                mMultiWindowModeStateDispatcher,
                mTabModelSelector,
                mToolbarManager,
                mDecorView,
                mBookmarkModelSupplier,
                mVerifier,
                uiType,
                /* menuEntries= */ new ArrayList<>(),
                /* isOpenedByChrome= */ true,
                /* showShare= */ true,
                /* showStar= */ true,
                /* showDownload= */ true,
                /* isIncognitoBranded= */ false,
                /* isOffTheRecord= */ false,
                /* isStartIconMenu= */ true,
                mReadAloudControllerSupplier,
                /* contextualPageActionControllerSupplier= */ SupplierUtils.ofNull(),
                /* hasClientPackage= */ false,
                /* pageZoomManager= */ null,
                /* openInAppMenuItemProvider= */ null,
                /* webAppHeaderLayoutCoordinatorSupplier= */ () -> null);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({ChromeFeatureList.DESKTOP_ANDROID_TWA_DISCLOSURES_HELP_LINK})
    public void testBuildFooterViewClickable_flagEnabled() {
        var delegate = createPropertiesDelegate(CustomTabsUiType.TRUSTED_WEB_ACTIVITY);

        AppMenuHandler appMenuHandler = mock(AppMenuHandler.class);
        View footer = delegate.buildFooterView(appMenuHandler);
        assertNotNull(footer);
        assertTrue(footer.hasOnClickListeners());
        assertTrue(footer.isClickable());
        assertTrue(footer.isFocusable());

        footer.performClick();
        verify(appMenuHandler).hideAppMenu();

        ShadowApplication shadowApplication =
                Shadows.shadowOf((Application) ApplicationProvider.getApplicationContext());
        Intent intent = shadowApplication.getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(
                "https://support.google.com/googlebook?p=web_powered_apps", intent.getDataString());
        assertTrue(intent.hasCategory(Intent.CATEGORY_BROWSABLE));
        assertEquals(
                Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
        assertTrue(intent.getBooleanExtra(IntentHandler.EXTRA_FROM_OPEN_IN_BROWSER, false));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @DisableFeatures({ChromeFeatureList.DESKTOP_ANDROID_TWA_DISCLOSURES_HELP_LINK})
    public void testBuildFooterViewClickable_flagDisabled() {
        var delegate = createPropertiesDelegate(CustomTabsUiType.TRUSTED_WEB_ACTIVITY);

        AppMenuHandler appMenuHandler = mock(AppMenuHandler.class);
        View footer = delegate.buildFooterView(appMenuHandler);
        assertNotNull(footer);
        assertFalse(footer.hasOnClickListeners());
        assertFalse(footer.isClickable());
        assertFalse(footer.isFocusable());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @EnableFeatures({ChromeFeatureList.DESKTOP_ANDROID_TWA_DISCLOSURES_HELP_LINK})
    public void testBuildFooterViewClickable_phoneFormFactor() {
        var delegate = createPropertiesDelegate(CustomTabsUiType.TRUSTED_WEB_ACTIVITY);

        AppMenuHandler appMenuHandler = mock(AppMenuHandler.class);
        View footer = delegate.buildFooterView(appMenuHandler);
        assertNotNull(footer);
        assertFalse(footer.hasOnClickListeners());
        assertFalse(footer.isClickable());
        assertFalse(footer.isFocusable());
    }
}
