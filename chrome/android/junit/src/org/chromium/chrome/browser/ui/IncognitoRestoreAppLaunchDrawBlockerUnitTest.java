// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.CommandLine;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Robolectric tests for {@link IncognitoRestoreAppLaunchDrawBlocker}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
public class IncognitoRestoreAppLaunchDrawBlockerUnitTest {
    @Mock
    private Bundle mSavedInstanceStateMock;
    @Mock
    private Intent mIntentMock;
    @Mock
    private CipherFactory mCipherFactoryMock;
    @Mock
    private CommandLine mCommandLineMock;
    @Mock
    private TabModelSelector mTabModelSelectorMock;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;

    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorObservableSupplier =
            new ObservableSupplierImpl<>();
    private Supplier<Intent> mIntentSupplier = new Supplier<Intent>() {
        @Nullable
        @Override
        public Intent get() {
            return mIntentMock;
        }
    };

    private IncognitoRestoreAppLaunchDrawBlocker mIncognitoRestoreAppLaunchDrawBlocker;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(/*isAvailable=*/true);
        CipherFactory.resetInstanceForTesting(mCipherFactoryMock);
        CommandLine.setInstanceForTesting(mCommandLineMock);

        mTabModelSelectorObservableSupplier.set(mTabModelSelectorMock);
        doNothing().when(mActivityLifecycleDispatcherMock).register(any());
        mIncognitoRestoreAppLaunchDrawBlocker = new IncognitoRestoreAppLaunchDrawBlocker(
                mSavedInstanceStateMock, mTabModelSelectorObservableSupplier, mIntentSupplier,
                mActivityLifecycleDispatcherMock);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(mSavedInstanceStateMock, mIntentMock, mCipherFactoryMock,
                mCommandLineMock, mTabModelSelectorMock);
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenReauthFeatureNotAvailable() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /*isAvailable=*/false);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenNoRestoreStateSwitchIsPresent() {
        // Premise conditions.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(/*isAvailable=*/true);

        // Test condition
        doReturn(true).when(mCommandLineMock).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCommandLineMock, times(1)).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenNoCipherDataIsFound() {
        // Premise conditions.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(/*isAvailable=*/true);
        doReturn(false).when(mCommandLineMock).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);

        // Test condition
        doReturn(false).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCommandLineMock, times(1)).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenReauthIsNotPending() {
        // Premise conditions.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(/*isAvailable=*/true);
        doReturn(false).when(mCommandLineMock).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);

        // Test condition
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthController.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCommandLineMock, times(1)).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthController.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenIntentingToRegularTab_AndLastTabModelWasNotIncognito() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(/*isAvailable=*/true);
        doReturn(false).when(mCommandLineMock).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthController.KEY_IS_INCOGNITO_REAUTH_PENDING, false);

        // Test conditions
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        doReturn(false)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCommandLineMock, times(1)).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthController.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenBothTabStateIsInitialized_And_NativeIsInitialized() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(/*isAvailable=*/true);
        doReturn(false).when(mCommandLineMock).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthController.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        doReturn(true)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);

        // Test condition
        doReturn(true).when(mTabModelSelectorMock).isTabStateInitialized();
        mIncognitoRestoreAppLaunchDrawBlocker.setIsNativeInitializationFinished(
                /*initialized=*/true);
        assertFalse(
                "Should not block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        // Verify all the mocks were called.
        verify(mCommandLineMock, times(1)).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthController.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        verify(mTabModelSelectorMock, times(1)).isTabStateInitialized();
    }

    @Test
    @SmallTest
    public void testShouldBlockDraw_WhenTabStateIsNotInitialized_And_NativeIsInitialized() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(/*isAvailable=*/true);
        doReturn(false).when(mCommandLineMock).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthController.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        doReturn(true)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);

        // Test condition
        doReturn(false).when(mTabModelSelectorMock).isTabStateInitialized();
        mIncognitoRestoreAppLaunchDrawBlocker.setIsNativeInitializationFinished(
                /*initialized=*/true);
        assertTrue("Should block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        // Verify all the mocks were called.
        verify(mCommandLineMock, times(1)).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthController.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        verify(mTabModelSelectorMock, times(1)).isTabStateInitialized();
    }

    @Test
    @SmallTest
    public void testShouldBlockDraw_WhenTabStateIsInitialized_And_WhenNativeIsNotInitialized() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(/*isAvailable=*/true);
        doReturn(false).when(mCommandLineMock).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthController.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        doReturn(true)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);

        // Test condition
        doReturn(true).when(mTabModelSelectorMock).isTabStateInitialized();
        mIncognitoRestoreAppLaunchDrawBlocker.setIsNativeInitializationFinished(
                /*initialized=*/false);
        assertTrue("Should block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        // Verify all the mocks were called.
        verify(mCommandLineMock, times(1)).hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthController.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        verify(mTabModelSelectorMock, times(1)).isTabStateInitialized();
    }
}
