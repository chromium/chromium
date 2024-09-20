// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthControllerImpl;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;

/** Robolectric tests for {@link IncognitoRestoreAppLaunchDrawBlocker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.LEGACY)
public class IncognitoRestoreAppLaunchDrawBlockerUnitTest {
    @Mock private Bundle mSavedInstanceStateMock;
    @Mock private Intent mIntentMock;
    @Mock private CipherFactory mCipherFactoryMock;
    @Mock private TabModelSelector mTabModelSelectorMock;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;
    @Mock private Runnable mUnblockDrawRunnableMock;
    @Captor private ArgumentCaptor<LifecycleObserver> mLifecycleObserverArgumentCaptor;

    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverArgumentCaptor;

    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorObservableSupplier =
            new ObservableSupplierImpl<>();
    private Supplier<Intent> mIntentSupplier =
            new Supplier<Intent>() {
                @Nullable
                @Override
                public Intent get() {
                    return mIntentMock;
                }
            };
    private Supplier<Boolean> mShouldIgnoreIntentSupplier =
            new Supplier<Boolean>() {
                @Nullable
                @Override
                public Boolean get() {
                    return mShouldIgnoreIntent;
                }
            };

    private boolean mShouldIgnoreIntent;
    private IncognitoRestoreAppLaunchDrawBlocker mIncognitoRestoreAppLaunchDrawBlocker;
    private NativeInitObserver mNativeInitObserver;
    private TabModelSelectorObserver mTabModelSelectorObserver;

    private Bundle getSavedInstanceStateMock() {
        return mSavedInstanceStateMock;
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ false);
        mTabModelSelectorObservableSupplier.set(mTabModelSelectorMock);
        mIncognitoRestoreAppLaunchDrawBlocker =
                new IncognitoRestoreAppLaunchDrawBlocker(
                        this::getSavedInstanceStateMock,
                        mTabModelSelectorObservableSupplier,
                        mIntentSupplier,
                        mShouldIgnoreIntentSupplier,
                        mActivityLifecycleDispatcherMock,
                        mUnblockDrawRunnableMock,
                        mCipherFactoryMock);

        // Check that the we added the native init observer.
        verify(mActivityLifecycleDispatcherMock, times(1))
                .register(mLifecycleObserverArgumentCaptor.capture());
        mNativeInitObserver = (NativeInitObserver) mLifecycleObserverArgumentCaptor.getValue();
        assertNotNull("Didn't register the NativeInitObserver", mNativeInitObserver);

        verify(mTabModelSelectorMock, times(1))
                .addObserver(mTabModelSelectorObserverArgumentCaptor.capture());
        // This is called by TabModelUtils.runOnTabStateInitialized.
        verify(mTabModelSelectorMock, times(1)).isTabStateInitialized();
        mTabModelSelectorObserver = mTabModelSelectorObserverArgumentCaptor.getValue();
        assertNotNull("Didn't add any observer.", mTabModelSelectorObserver);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(
                mSavedInstanceStateMock,
                mIntentMock,
                mCipherFactoryMock,
                mTabModelSelectorMock,
                mUnblockDrawRunnableMock,
                mActivityLifecycleDispatcherMock);
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenReauthFeatureNotAvailable() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ false);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({ChromeSwitches.NO_RESTORE_STATE})
    public void testShouldNotBlockDraw_WhenNoRestoreStateSwitchIsPresent() {
        // Premise conditions.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);

        // Test condition
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenNoCipherDataIsFound() {
        // Premise conditions.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);

        // Test condition
        doReturn(false).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenReauthIsNotPending() {
        // Premise conditions.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);

        // Test condition
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenIntentingToRegularTab_AndLastTabModelWasNotIncognito() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);

        // Test conditions
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        doReturn(false)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenBothTabStateIsInitialized_And_NativeIsInitialized() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        mIncognitoRestoreAppLaunchDrawBlocker.resetIsUnblockDrawRunnableInvokedForTesting();
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        doReturn(true)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);

        // Test condition
        doReturn(true).when(mTabModelSelectorMock).isTabStateInitialized();
        mNativeInitObserver.onFinishNativeInitialization();
        assertFalse(
                "Should not block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        // Verify all the mocks were called.
        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        // This is called again when we call mNativeInitObserver.onFinishNativeInitialization();
        verify(mTabModelSelectorMock, times(3)).isTabStateInitialized();
        // This is called when we call mNativeInitObserver.onFinishNativeInitialization() and since
        // tab state is initialized as well, we will invoke the unblock runnable.
        verify(mUnblockDrawRunnableMock, times(1)).run();
    }

    @Test
    @SmallTest
    public void testShouldBlockDraw_WhenTabStateIsNotInitialized_And_NativeIsInitialized() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        doReturn(true)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);

        // Test condition
        doReturn(false).when(mTabModelSelectorMock).isTabStateInitialized();
        mNativeInitObserver.onFinishNativeInitialization();
        assertTrue("Should block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        // Verify all the mocks were called.
        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        // This is called again when we call mNativeInitObserver.onFinishNativeInitialization();
        verify(mTabModelSelectorMock, times(3)).isTabStateInitialized();
    }

    @Test
    @SmallTest
    public void testShouldBlockDraw_WhenTabStateIsInitialized_And_WhenNativeIsNotInitialized() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        doReturn(true)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);

        // Test condition
        doReturn(true).when(mTabModelSelectorMock).isTabStateInitialized();
        assertTrue(
                "Should block draw as native has not finished initialization.",
                mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        // Verify all the mocks were called.
        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        verify(mTabModelSelectorMock, times(2)).isTabStateInitialized();
    }

    @Test
    @SmallTest
    public void testUnblockDrawRunnableNotInvoked_WhenTabStateNotInitialized() {
        doReturn(false).when(mTabModelSelectorMock).isTabStateInitialized();
        mNativeInitObserver.onFinishNativeInitialization();

        verify(mTabModelSelectorMock, times(2)).isTabStateInitialized();
        verifyNoMoreInteractions(mUnblockDrawRunnableMock);
    }

    @Test
    @SmallTest
    public void testUnblockDrawRunnableNotInvoked_WhenNativeNotInitialized() {
        doReturn(true).when(mTabModelSelectorMock).isTabStateInitialized();
        mTabModelSelectorObserver.onTabStateInitialized();

        verify(mTabModelSelectorMock, times(1)).removeObserver(mTabModelSelectorObserver);
        verify(mTabModelSelectorMock, times(2)).isTabStateInitialized();
        verifyNoMoreInteractions(mUnblockDrawRunnableMock);
    }

    @Test
    @SmallTest
    public void testUnblockDrawRunnableInvoked_WhenTabStateAndNativeIsInitialized() {
        // We need to reset the boolean so that we can verify mUnblockDrawRunnable is invoked.
        mIncognitoRestoreAppLaunchDrawBlocker.resetIsUnblockDrawRunnableInvokedForTesting();
        doReturn(true).when(mTabModelSelectorMock).isTabStateInitialized();
        mNativeInitObserver.onFinishNativeInitialization();

        verify(mTabModelSelectorMock, times(2)).isTabStateInitialized();
        verify(mUnblockDrawRunnableMock, times(1)).run();
    }
}
