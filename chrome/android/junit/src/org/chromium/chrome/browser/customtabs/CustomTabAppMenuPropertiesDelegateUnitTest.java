// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.Menu;
import android.view.View;
import android.widget.PopupMenu;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link CustomTabAppMenuPropertiesDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabAppMenuPropertiesDelegateUnitTest {
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private Tab mTab;
    @Mock private NavigationController mNavigationController;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private View mDecorView;

    @Mock private Verifier mVerifier;

    private ObservableSupplierImpl<BookmarkModel> mBookmarkModelSupplier =
            new ObservableSupplierImpl<>();
    private Supplier<ReadAloudController> mReadAloudControllerSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mTab.isNativePage()).thenReturn(false);
        Map<String, Boolean> featureMap = new HashMap<>();
        featureMap.put(ChromeFeatureList.READALOUD_IN_OVERFLOW_MENU_IN_CCT, false);
        FeatureList.setTestFeatures(featureMap);
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
                        /* isIncognito= */ false,
                        /* isOffTheRecord= */ false,
                        /* isStartIconMenu= */ true,
                        mReadAloudControllerSupplier,
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
}
