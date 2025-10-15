// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.os.Looper;
import android.view.ViewGroup;
import android.view.WindowManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDisplay;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import java.util.Collections;

/** Unit tests for TabItemPickerCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabItemPickerCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabWindowManager mTabWindowManager;
    @Mock private Profile mProfile;
    @Mock private TabModelSelectorImpl mTabModelSelector;
    @Mock private Callback<TabModelSelector> mCallback;
    @Mock private Activity mActivity;
    @Mock private TabListEditorCoordinator mTabListEditorCoordinator;
    @Mock private ViewGroup mRootView;
    @Mock private ViewGroup mContainerView;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TabListEditorCoordinator.TabListEditorController mTabListEditorController;
    @Mock private android.view.Window mWindow;
    @Mock private android.view.ViewGroup mDecorView;
    @Mock private android.content.res.Resources mResources;
    @Mock private WindowManager mWindowManager;
    @Mock private TabModel mRegularTabModel;

    private OneshotSupplierImpl<Profile> mProfileSupplierImpl;
    private TabItemPickerCoordinator mItemPickerCoordinator;
    private final int mWindowId = 5;

    @Before
    public void setUp() {
        mProfileSupplierImpl = new OneshotSupplierImpl<>();
        TabItemPickerCoordinator realCoordinator =
                new TabItemPickerCoordinator(
                        mProfileSupplierImpl,
                        mWindowId,
                        mActivity,
                        mSnackbarManager,
                        mRootView,
                        mContainerView);
        mItemPickerCoordinator = Mockito.spy(realCoordinator);

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mWindow.getDecorView()).thenReturn(mDecorView);

        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getInteger(anyInt())).thenReturn(1);

        when(mActivity.getSystemService(Context.WINDOW_SERVICE)).thenReturn(mWindowManager);
        when(mWindowManager.getDefaultDisplay()).thenReturn(ShadowDisplay.getDefaultDisplay());

        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mRegularTabModel.index()).thenReturn(0);

        doReturn(Collections.emptyList().iterator()).when(mRegularTabModel).iterator();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabListEditorCoordinator.getController()).thenReturn(mTabListEditorController);
        doReturn(mTabListEditorCoordinator)
                .when(mItemPickerCoordinator)
                .createTabListEditorCoordinator(any(TabModelSelector.class));
    }

    @After
    public void tearDown() {
        TabWindowManagerSingleton.setTabWindowManagerForTesting(null);
    }

    @Test
    public void testShowTabItemPicker_SuccessPath() {
        // Mock the window manager to return a valid selector upon request
        when(TabWindowManagerSingleton.getInstance()
                        .requestSelectorWithoutActivity(anyInt(), any(Profile.class)))
                .thenReturn(mTabModelSelector);

        mItemPickerCoordinator.showTabItemPicker(mCallback);
        mProfileSupplierImpl.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();

        // Verification 1: The window manager was called correctly
        verify(TabWindowManagerSingleton.getInstance())
                .requestSelectorWithoutActivity(eq(mWindowId), eq(mProfile));

        // Verification 2: The final callback is called with the initialized selector
        verify(mCallback).onResult(mTabModelSelector);
    }

    @Test
    public void testShowTabItemPicker_InvalidWindowIdFailsEarly() {
        // Mock coordinator with an invalid window ID
        TabItemPickerCoordinator coordinatorWithInvalidId =
                new TabItemPickerCoordinator(
                        mProfileSupplierImpl,
                        TabWindowManager.INVALID_WINDOW_ID,
                        mActivity,
                        mSnackbarManager,
                        mRootView,
                        mContainerView);

        coordinatorWithInvalidId.showTabItemPicker(mCallback);
        mProfileSupplierImpl.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();

        // Verification 1: The window manager should never have been called.
        verify(mTabWindowManager, never())
                .requestSelectorWithoutActivity(anyInt(), any(Profile.class));

        // Verification 2: The final callback is called with null.
        verify(mCallback).onResult(null);
    }

    @Test
    public void testShowTabItemPicker_AcquisitionFailsReturnsNull() {
        // Mock the window manager to explicitly return NULL
        Mockito.when(mTabWindowManager.requestSelectorWithoutActivity(anyInt(), any(Profile.class)))
                .thenReturn(null);

        mItemPickerCoordinator.showTabItemPicker(mCallback);
        mProfileSupplierImpl.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();

        // Verification 1: The window manager was called (it ran the core logic)
        verify(mTabWindowManager).requestSelectorWithoutActivity(eq(mWindowId), eq(mProfile));

        // Verification 2: The final callback is called with null.
        verify(mCallback).onResult(null);
    }
}
