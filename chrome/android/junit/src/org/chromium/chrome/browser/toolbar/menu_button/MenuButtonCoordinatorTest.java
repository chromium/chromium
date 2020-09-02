// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.ui.util.TokenHolder;

/**
 * Unit tests for ToolbarAppMenuManager.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MenuButtonCoordinatorTest {
    @Mock
    private BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    @Mock
    private Activity mActivity;
    @Mock
    private MenuButtonCoordinator.SetFocusFunction mFocusFunction;
    @Mock
    private AppMenuCoordinator mAppMenuCoordinator;
    @Mock
    private AppMenuHandler mAppMenuHandler;
    @Mock
    private AppMenuButtonHelper mAppMenuButtonHelper;
    @Mock
    MenuButton mMenuButton;
    @Mock
    private AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    @Mock
    private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock
    private Runnable mRequestRenderRunnable;

    private UpdateMenuItemHelper.MenuUiState mMenuUiState;
    private ObservableSupplierImpl<AppMenuCoordinator> mAppMenuSupplier;
    private MenuButtonCoordinator mMenuButtonCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mAppMenuHandler).when(mAppMenuCoordinator).getAppMenuHandler();
        doReturn(mAppMenuButtonHelper).when(mAppMenuHandler).createAppMenuButtonHelper();
        doReturn(mAppMenuPropertiesDelegate)
                .when(mAppMenuCoordinator)
                .getAppMenuPropertiesDelegate();
        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
        mAppMenuSupplier = new ObservableSupplierImpl<>();
        mMenuUiState = new UpdateMenuItemHelper.MenuUiState();
        doReturn(mMenuUiState).when(mUpdateMenuItemHelper).getUiState();

        mMenuButtonCoordinator =
                new MenuButtonCoordinator(mAppMenuSupplier, mControlsVisibilityDelegate, mActivity,
                        mFocusFunction, mRequestRenderRunnable, true, () -> false, mMenuButton);
    }

    @Test
    public void testInitialization() {
        mAppMenuSupplier.set(mAppMenuCoordinator);
        verify(mAppMenuHandler).addObserver(mMenuButtonCoordinator);
        verify(mAppMenuHandler).createAppMenuButtonHelper();
    }

    @Test
    public void testAppMenuVisiblityChange_badgeShowing() {
        mAppMenuSupplier.set(mAppMenuCoordinator);
        doReturn(42)
                .when(mControlsVisibilityDelegate)
                .showControlsPersistentAndClearOldToken(TokenHolder.INVALID_TOKEN);
        doReturn(true).when(mMenuButton).isShowingAppMenuUpdateBadge();
        mMenuButtonCoordinator.onMenuVisibilityChanged(true);

        verify(mFocusFunction).setFocus(false, LocationBar.OmniboxFocusReason.UNFOCUS);
        verify(mMenuButton).removeAppMenuUpdateBadge(true);
        verify(mUpdateMenuItemHelper).onMenuButtonClicked();

        mMenuButtonCoordinator.onMenuVisibilityChanged(false);
        verify(mControlsVisibilityDelegate).releasePersistentShowingToken(42);
    }

    @Test
    public void testAppMenuHighlightChange() {
        mAppMenuSupplier.set(mAppMenuCoordinator);

        doReturn(42)
                .when(mControlsVisibilityDelegate)
                .showControlsPersistentAndClearOldToken(TokenHolder.INVALID_TOKEN);

        mMenuButtonCoordinator.onMenuHighlightChanged(true);
        verify(mMenuButton).setMenuButtonHighlight(true);

        mMenuButtonCoordinator.onMenuHighlightChanged(false);
        verify(mMenuButton).setMenuButtonHighlight(false);
        verify(mControlsVisibilityDelegate).releasePersistentShowingToken(42);
    }

    @Test
    public void testAppMenuUpdateBadge() {
        mAppMenuSupplier.set(mAppMenuCoordinator);

        doReturn(true).when(mActivity).isDestroyed();
        mMenuButtonCoordinator.updateStateChanged();

        verify(mMenuButton, never()).showAppMenuUpdateBadgeIfAvailable(anyBoolean());
        verify(mRequestRenderRunnable, never()).run();
        verify(mMenuButton, never()).removeAppMenuUpdateBadge(false);

        doReturn(false).when(mActivity).isDestroyed();
        mMenuButtonCoordinator.updateStateChanged();

        verify(mMenuButton, never()).showAppMenuUpdateBadgeIfAvailable(anyBoolean());
        verify(mRequestRenderRunnable, never()).run();
        verify(mMenuButton, times(1)).removeAppMenuUpdateBadge(false);

        mMenuUiState.buttonState = new UpdateMenuItemHelper.MenuButtonState();
        mMenuButtonCoordinator.updateStateChanged();

        verify(mMenuButton).showAppMenuUpdateBadgeIfAvailable(true);
        verify(mRequestRenderRunnable).run();
        verify(mMenuButton, times(1)).removeAppMenuUpdateBadge(false);
    }

    @Test
    public void testAppMenuUpdateBadge_activityShouldNotShow() {
        MenuButtonCoordinator newCoordinator =
                new MenuButtonCoordinator(mAppMenuSupplier, mControlsVisibilityDelegate, mActivity,
                        mFocusFunction, mRequestRenderRunnable, false, () -> false, mMenuButton);

        doReturn(true).when(mActivity).isDestroyed();
        newCoordinator.updateStateChanged();

        verify(mMenuButton, never()).showAppMenuUpdateBadgeIfAvailable(anyBoolean());
        verify(mRequestRenderRunnable, never()).run();
        verify(mMenuButton, never()).removeAppMenuUpdateBadge(false);

        doReturn(false).when(mActivity).isDestroyed();
        newCoordinator.updateStateChanged();

        verify(mMenuButton, never()).showAppMenuUpdateBadgeIfAvailable(anyBoolean());
        verify(mRequestRenderRunnable, never()).run();
        verify(mMenuButton, never()).removeAppMenuUpdateBadge(false);

        mMenuUiState.buttonState = new UpdateMenuItemHelper.MenuButtonState();
        newCoordinator.updateStateChanged();

        verify(mMenuButton, never()).showAppMenuUpdateBadgeIfAvailable(anyBoolean());
        verify(mRequestRenderRunnable, never()).run();
        verify(mMenuButton, never()).removeAppMenuUpdateBadge(false);
    }

    @Test
    public void testDestroyIsSafe() {
        mMenuButtonCoordinator.destroy();
        // It should be crash-safe to call public methods, but the results aren't meaningful.
        mMenuButtonCoordinator.getMenuButtonHelperSupplier();
        mMenuButtonCoordinator.onMenuHighlightChanged(true);
        mMenuButtonCoordinator.onMenuVisibilityChanged(false);
        mMenuButtonCoordinator.onNativeInitialized();
        mMenuButtonCoordinator.setAppMenuUpdateBadgeSuppressed(true);
        mMenuButtonCoordinator.updateReloadingState(true);
        mMenuButtonCoordinator.updateStateChanged();
    }

    @Test
    public void testDisableDestroysButton() {
        mMenuButtonCoordinator.disableMenuButton();
        verify(mMenuButton).destroy();
    }
}
