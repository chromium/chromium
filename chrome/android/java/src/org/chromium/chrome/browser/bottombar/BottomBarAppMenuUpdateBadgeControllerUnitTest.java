// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.menu_button.MenuUiState;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.appmenu.AppMenuActionProperties;
import org.chromium.chrome.browser.ui.actions.appmenu.MenuButtonState;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BottomBarAppMenuUpdateBadgeController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarAppMenuUpdateBadgeControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActionRegistry mActionRegistry;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock private AppMenuCoordinator mAppMenuCoordinator;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Captor private ArgumentCaptor<AppMenuObserver> mAppMenuObserverCaptor;
    @Captor private ArgumentCaptor<Runnable> mMenuStateObserverCaptor;

    private final SettableNullableObservableSupplier<PropertyModel> mActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createNullable();
    private final OneshotSupplierImpl<AppMenuCoordinator> mAppMenuCoordinatorSupplier =
            new OneshotSupplierImpl<>();

    private PropertyModel mPropertyModel;
    private BottomBarAppMenuUpdateBadgeController mController;

    @Before
    public void setUp() {
        when(mAppMenuCoordinator.getAppMenuHandler()).thenReturn(mAppMenuHandler);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mIncognitoProfile.getOriginalProfile()).thenReturn(mProfile);
        mPropertyModel =
                new PropertyModel.Builder(AppMenuActionProperties.ALL_KEYS)
                        .with(AppMenuActionProperties.SHOW_UPDATE_BADGE, false)
                        .with(AppMenuActionProperties.UPDATE_BADGE_BUTTON_STATE, null)
                        .with(ActionProperties.CONTENT_DESCRIPTION_RESOLVER, null)
                        .build();
        mActionSupplier.set(mPropertyModel);
        when(mActionRegistry.get(ActionId.APP_MENU)).thenReturn(mActionSupplier);
        when(mUpdateMenuItemHelper.getUiState()).thenReturn(new MenuUiState());
        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
    }

    @Test
    public void testInitialization() {
        mController =
                new BottomBarAppMenuUpdateBadgeController(
                        mActionRegistry, mProfileSupplier, mAppMenuCoordinatorSupplier);
        mProfileSupplier.set(mProfile);

        verify(mUpdateMenuItemHelper).registerObserver(any());
    }

    @Test
    public void testUpdateStateChanged_showBadge() {
        MenuUiState state = new MenuUiState();
        state.buttonState = new MenuButtonState();
        state.buttonState.menuContentDescription = 12345;
        when(mUpdateMenuItemHelper.getUiState()).thenReturn(state);

        mController =
                new BottomBarAppMenuUpdateBadgeController(
                        mActionRegistry, mProfileSupplier, mAppMenuCoordinatorSupplier);
        mProfileSupplier.set(mProfile);

        assertTrue(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));
        assertEquals(
                state.buttonState,
                mPropertyModel.get(AppMenuActionProperties.UPDATE_BADGE_BUTTON_STATE));
        assertNotNull(mPropertyModel.get(ActionProperties.CONTENT_DESCRIPTION_RESOLVER));
    }

    @Test
    public void testUpdateStateChanged_noBadge() {
        MenuUiState state = new MenuUiState();
        state.buttonState = null;
        when(mUpdateMenuItemHelper.getUiState()).thenReturn(state);

        mController =
                new BottomBarAppMenuUpdateBadgeController(
                        mActionRegistry, mProfileSupplier, mAppMenuCoordinatorSupplier);
        mProfileSupplier.set(mProfile);

        assertFalse(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));
        assertNull(mPropertyModel.get(AppMenuActionProperties.UPDATE_BADGE_BUTTON_STATE));
    }

    @Test
    public void testDestroy() {
        mController =
                new BottomBarAppMenuUpdateBadgeController(
                        mActionRegistry, mProfileSupplier, mAppMenuCoordinatorSupplier);
        mProfileSupplier.set(mProfile);

        mController.destroy();
        verify(mUpdateMenuItemHelper).unregisterObserver(any());
    }

    @Test
    public void testMenuVisible_dismissBadge() {
        mController =
                new BottomBarAppMenuUpdateBadgeController(
                        mActionRegistry, mProfileSupplier, mAppMenuCoordinatorSupplier);
        mProfileSupplier.set(mProfile);
        mAppMenuCoordinatorSupplier.set(mAppMenuCoordinator);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mAppMenuHandler).addObserver(mAppMenuObserverCaptor.capture());
        AppMenuObserver observer = mAppMenuObserverCaptor.getValue();

        // Initially shown.
        MenuUiState state = new MenuUiState();
        state.buttonState = new MenuButtonState();
        state.buttonState.menuContentDescription = 12345;
        when(mUpdateMenuItemHelper.getUiState()).thenReturn(state);
        mPropertyModel.set(AppMenuActionProperties.SHOW_UPDATE_BADGE, true);

        doAnswer(
                        invocation -> {
                            assertFalse(
                                    mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));
                            assertTrue(mController.isBadgeDismissedForTesting());
                            return null;
                        })
                .when(mUpdateMenuItemHelper)
                .onMenuButtonClicked();

        // When menu becomes visible, badge should be dismissed before onMenuButtonClicked is
        // called.
        observer.onMenuVisibilityChanged(true);

        assertFalse(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));
        assertTrue(mController.isBadgeDismissedForTesting());
        verify(mUpdateMenuItemHelper).onMenuButtonClicked();
    }

    @Test
    public void testMenuVisible_dismissBadge_persistsAcrossProfileSwitch() {
        MenuUiState state = new MenuUiState();
        state.buttonState = new MenuButtonState();
        state.buttonState.menuContentDescription = R.string.accessibility_toolbar_btn_menu_update;
        when(mUpdateMenuItemHelper.getUiState()).thenReturn(state);

        mController =
                new BottomBarAppMenuUpdateBadgeController(
                        mActionRegistry, mProfileSupplier, mAppMenuCoordinatorSupplier);
        mProfileSupplier.set(mProfile);
        mAppMenuCoordinatorSupplier.set(mAppMenuCoordinator);
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));

        verify(mAppMenuHandler).addObserver(mAppMenuObserverCaptor.capture());
        AppMenuObserver observer = mAppMenuObserverCaptor.getValue();

        // Dismiss the badge by opening the app menu.
        observer.onMenuVisibilityChanged(true);
        assertFalse(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));

        // Switch to incognito profile and back to regular profile.
        mProfileSupplier.set(mIncognitoProfile);
        assertFalse(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.accessibility_toolbar_btn_menu),
                mPropertyModel
                        .get(ActionProperties.CONTENT_DESCRIPTION_RESOLVER)
                        .resolve(ContextUtils.getApplicationContext()));

        mProfileSupplier.set(mProfile);
        assertFalse(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.accessibility_toolbar_btn_menu),
                mPropertyModel
                        .get(ActionProperties.CONTENT_DESCRIPTION_RESOLVER)
                        .resolve(ContextUtils.getApplicationContext()));

        verify(mProfile, times(2)).getOriginalProfile();
        verify(mIncognitoProfile).getOriginalProfile();
        verify(mUpdateMenuItemHelper, times(1))
                .registerObserver(mMenuStateObserverCaptor.capture());
        verify(mUpdateMenuItemHelper, never()).unregisterObserver(any());

        // Triggering updateBadgeState() without a menu state-change notification should keep the
        // badge dismissed.
        mActionSupplier.set(null);
        mActionSupplier.set(mPropertyModel);
        assertFalse(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));

        // A menu state-change notification resets the dismissed flag and shows the badge again.
        mMenuStateObserverCaptor.getValue().run();
        assertTrue(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));
    }

    @Test
    public void testMenuVisible_noBadgeShowing_doesNotPreventFutureBadge() {
        mController =
                new BottomBarAppMenuUpdateBadgeController(
                        mActionRegistry, mProfileSupplier, mAppMenuCoordinatorSupplier);
        mProfileSupplier.set(mProfile);
        mAppMenuCoordinatorSupplier.set(mAppMenuCoordinator);
        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));

        verify(mAppMenuHandler).addObserver(mAppMenuObserverCaptor.capture());
        AppMenuObserver observer = mAppMenuObserverCaptor.getValue();

        // Opening the menu when no badge is showing should not mark a future badge as dismissed.
        observer.onMenuVisibilityChanged(true);
        assertFalse(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));
        assertFalse(mController.isBadgeDismissedForTesting());

        MenuUiState state = new MenuUiState();
        state.buttonState = new MenuButtonState();
        state.buttonState.menuContentDescription = R.string.accessibility_toolbar_btn_menu_update;
        when(mUpdateMenuItemHelper.getUiState()).thenReturn(state);

        mActionSupplier.set(null);
        mActionSupplier.set(mPropertyModel);
        assertTrue(mPropertyModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE));
        assertFalse(mController.isBadgeDismissedForTesting());
    }
}
