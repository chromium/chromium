// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.Menu;
import android.view.View;
import android.widget.PopupMenu;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
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
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;

/** Unit tests for {@link CustomTabAppMenuPropertiesDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.READALOUD_IN_OVERFLOW_MENU_IN_CCT,
    ContentFeatureList.ANDROID_OPEN_PDF_INLINE,
    ChromeFeatureList.ANDROID_OPEN_PDF_INLINE_BACKPORT
})
public class CustomTabAppMenuPropertiesDelegateUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private Tab mTab;
    @Mock private NavigationController mNavigationController;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
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

    private final ObservableSupplierImpl<BookmarkModel> mBookmarkModelSupplier =
            new ObservableSupplierImpl<>();
    private final Supplier<ReadAloudController> mReadAloudControllerSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mTab.isNativePage()).thenReturn(false);
    }

    private Menu createMenu(Context context, int menuResourceId) {
        PopupMenu tempMenu = new PopupMenu(context, mDecorView);
        tempMenu.inflate(menuResourceId);
        return tempMenu.getMenu();
    }

    private boolean isMenuVisible(Menu menu, int itemId) {
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).getItemId() == itemId) {
                return menu.getItem(i).isVisible();
            }
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
                        /* menuEntries= */ new ArrayList<String>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        /* contextualPageActionControllerSupplier */ () -> null,
                        /* hasClientPackage= */ false);
        Menu menu = createMenu(context, delegate.getAppMenuLayoutId());
        delegate.prepareMenu(menu, null);
        assertTrue(isMenuVisible(menu, R.id.enable_price_tracking_menu_id));
        assertFalse(isMenuVisible(menu, R.id.disable_price_tracking_menu_id));
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
                        /* menuEntries= */ new ArrayList<String>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        () -> cpac,
                        /* hasClientPackage= */ false);
        Menu menu = createMenu(context, delegate.getAppMenuLayoutId());
        delegate.prepareMenu(menu, null);
        assertTrue(isMenuVisible(menu, R.id.price_insights_menu_id));
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
                        /* menuEntries= */ new ArrayList<String>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        /* contextualPageActionControllerSupplier */ () -> null,
                        /* hasClientPackage= */ false);
        Menu menu = createMenu(context, delegate.getAppMenuLayoutId());
        delegate.prepareMenu(menu, null);

        assertTrue(isMenuVisible(menu, R.id.find_in_page_id));

        // Verify the following 5 menu items are hidden.
        assertFalse(isMenuVisible(menu, R.id.bookmark_this_page_id));
        assertFalse(isMenuVisible(menu, R.id.offline_page_id));
        assertFalse(isMenuVisible(menu, R.id.share_row_menu_id));
        assertFalse(isMenuVisible(menu, R.id.universal_install));
        assertFalse(isMenuVisible(menu, R.id.open_in_browser_id));
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
                        /* menuEntries= */ new ArrayList<String>(),
                        /* isOpenedByChrome= */ true,
                        /* showShare= */ true,
                        /* showStar= */ true,
                        /* showDownload= */ true,
                        /* isIncognitoBranded= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
                        /* contextualPageActionControllerSupplier */ () -> null,
                        /* hasClientPackage= */ false);
        Menu menu = createMenu(context, delegate.getAppMenuLayoutId());
        delegate.prepareMenu(menu, null);

        assertTrue(isMenuVisible(menu, R.id.find_in_page_id));

        // Verify the following 6 menu items are hidden.
        assertFalse(isMenuVisible(menu, R.id.open_in_browser_id));
        assertFalse(isMenuVisible(menu, R.id.bookmark_this_page_id));
        assertFalse(isMenuVisible(menu, R.id.offline_page_id));
        assertFalse(isMenuVisible(menu, R.id.universal_install));
        assertFalse(isMenuVisible(menu, R.id.request_desktop_site_row_menu_id));
        assertFalse(isMenuVisible(menu, R.id.readaloud_menu_id));
    }
}
