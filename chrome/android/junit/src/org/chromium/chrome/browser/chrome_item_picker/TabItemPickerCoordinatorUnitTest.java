// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.os.Looper;

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

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

/** Unit tests for TabItemPickerCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabItemPickerCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabWindowManager mTabWindowManager;
    @Mock private Profile mProfile;
    @Mock private TabModelSelectorImpl mTabModelSelector;
    @Mock private Callback<TabModelSelector> mCallback;

    private TabItemPickerCoordinator mItemPickerCoordinator;
    private OneshotSupplierImpl<Profile> mProfileSupplierImpl;
    private CallbackController mCallbackController;
    private final int mWindowId = 5;

    @Before
    public void setUp() {
        mCallbackController = new CallbackController();
        mProfileSupplierImpl = new OneshotSupplierImpl<>();

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        mItemPickerCoordinator =
                new TabItemPickerCoordinator(mCallbackController, mProfileSupplierImpl, mWindowId);
    }

    @After
    public void tearDown() {
        TabWindowManagerSingleton.setTabWindowManagerForTesting(null);
        mCallbackController.destroy();
    }

    @Test
    public void testRequestTabModel_SuccessPath() {
        // Mock the window manager to return a valid selector upon request
        when(TabWindowManagerSingleton.getInstance()
                        .requestSelectorWithoutActivity(anyInt(), any(Profile.class)))
                .thenReturn(mTabModelSelector);

        mItemPickerCoordinator.requestTabModel(mCallback);
        mProfileSupplierImpl.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();

        // Verification 1: The window manager was called correctly
        verify(TabWindowManagerSingleton.getInstance())
                .requestSelectorWithoutActivity(eq(mWindowId), eq(mProfile));

        // Verification 2: The final callback is called with the initialized selector
        verify(mCallback).onResult(mTabModelSelector);
    }

    @Test
    public void testRequestTabModel_InvalidWindowIdFailsEarly() {
        // Mock coordinator with an invalid window ID
        TabItemPickerCoordinator coordinatorWithInvalidId =
                new TabItemPickerCoordinator(
                        mCallbackController,
                        mProfileSupplierImpl,
                        TabWindowManager.INVALID_WINDOW_ID);

        coordinatorWithInvalidId.requestTabModel(mCallback);
        mProfileSupplierImpl.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();

        // Verification 1: The window manager should never have been called.
        verify(mTabWindowManager, never())
                .requestSelectorWithoutActivity(anyInt(), any(Profile.class));

        // Verification 2: The final callback is called with null.
        verify(mCallback).onResult(null);
    }

    @Test
    public void testRequestTabModel_AcquisitionFailsReturnsNull() {
        // Mock the window manager to explicitly return NULL
        Mockito.when(mTabWindowManager.requestSelectorWithoutActivity(anyInt(), any(Profile.class)))
                .thenReturn(null);

        mItemPickerCoordinator.requestTabModel(mCallback);
        mProfileSupplierImpl.set(mProfile);
        shadowOf(Looper.getMainLooper()).idle();

        // Verification 1: The window manager was called (it ran the core logic)
        verify(mTabWindowManager).requestSelectorWithoutActivity(eq(mWindowId), eq(mProfile));

        // Verification 2: The final callback is called with null.
        verify(mCallback).onResult(null);
    }
}
