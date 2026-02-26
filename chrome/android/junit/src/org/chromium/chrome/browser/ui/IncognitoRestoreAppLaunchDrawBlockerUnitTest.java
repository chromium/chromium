// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.chrome.browser.incognito.reauth.IncognitoReauthControllerImpl.PREVIOUS_VERSION_CODE;

import android.content.Intent;
import android.os.Bundle;
import android.os.PersistableBundle;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.build.BuildConfig;
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

import java.util.function.Supplier;

/** Robolectric tests for {@link IncognitoRestoreAppLaunchDrawBlocker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IncognitoRestoreAppLaunchDrawBlockerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Bundle mSavedInstanceStateMock;
    @Mock private PersistableBundle mPersistentStateMock;
    @Mock private Intent mIntentMock;
    @Mock private CipherFactory mCipherFactoryMock;
    @Mock private TabModelSelector mTabModelSelectorMock;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;
    @Mock private Runnable mUnblockDrawRunnableMock;
    @Captor private ArgumentCaptor<LifecycleObserver> mLifecycleObserverArgumentCaptor;

    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverArgumentCaptor;

    private final SettableMonotonicObservableSupplier<TabModelSelector>
            mTabModelSelectorObservableSupplier = ObservableSuppliers.createMonotonic();
    private final Supplier<Intent> mIntentSupplier =
            new Supplier<>() {
                @Nullable
                @Override
                public Intent get() {
                    return mIntentMock;
                }
            };
    private final Supplier<Boolean> mShouldIgnoreIntentSupplier =
            new Supplier<>() {
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

    private PersistableBundle getPersistentStateMock() {
        return mPersistentStateMock;
    }

    @Before
    public void setUp() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ false);
        mTabModelSelectorObservableSupplier.set(mTabModelSelectorMock);
        mIncognitoRestoreAppLaunchDrawBlocker =
                new IncognitoRestoreAppLaunchDrawBlocker(
                        this::getSavedInstanceStateMock,
                        this::getPersistentStateMock,
                        mTabModelSelectorObservableSupplier,
                        mIntentSupplier,
                        mShouldIgnoreIntentSupplier,
                        mActivityLifecycleDispatcherMock,
                        mUnblockDrawRunnableMock,
                        mCipherFactoryMock);
        RobolectricUtil.runAllBackgroundAndUi();

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

        doReturn(BuildConfig.VERSION_CODE - 1)
                .when(mPersistentStateMock)
                .getLong(eq(PREVIOUS_VERSION_CODE), anyLong());
    }

    @After
    public void tearDown() {
        if (mSavedInstanceStateMock != null) {
            verifyNoMoreInteractions(mSavedInstanceStateMock);
        }
        if (mPersistentStateMock != null) {
            verifyNoMoreInteractions(mPersistentStateMock);
        }
        verifyNoMoreInteractions(
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
        doReturn(false).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenReauthIsNotPending() {
        // Premise conditions.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(true).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);

        // Test condition
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
    }

    @Test
    @SmallTest
    public void
            testShouldNotBlockDraw_WhenIntentingToRegularTab_AndLastTabModelWasNotIncognito_ForSavedPendingReauth() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        doReturn(true).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(true)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);

        doReturn(false).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        doReturn(false)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);

        // Test conditions
        doReturn(false)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
    }

    @Test
    @SmallTest
    public void
            testShouldNotBlockDraw_WhenIntentingToRegularTab_AndLastTabModelWasNotIncognito_ForPersistentPendingReauth() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        doReturn(false).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);

        doReturn(true).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        doReturn(true)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);

        // Test conditions
        doReturn(false)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        assertFalse(
                "Shouldn't block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
    }

    @Test
    @SmallTest
    public void
            testShouldNotBlockDraw_WhenBothTabStateIsInitialized_And_NativeIsInitialized_ForSavedPendingReauth() {
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

        doReturn(false).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        doReturn(false)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mPersistentStateMock)
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
        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());
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
    public void
            testShouldNotBlockDraw_WhenBothTabStateIsInitialized_And_NativeIsInitialized_ForPersistentPendingReauth() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        mIncognitoRestoreAppLaunchDrawBlocker.resetIsUnblockDrawRunnableInvokedForTesting();
        doReturn(false).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);

        doReturn(true).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        doReturn(true)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(true)
                .when(mPersistentStateMock)
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

        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());

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
    public void
            testShouldBlockDraw_WhenTabStateIsNotInitialized_And_NativeIsInitialized_ForSavedPendingReauth() {
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

        doReturn(false).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        doReturn(false)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
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
        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        // This is called again when we call mNativeInitObserver.onFinishNativeInitialization();
        verify(mTabModelSelectorMock, times(3)).isTabStateInitialized();
    }

    @Test
    @SmallTest
    public void
            testShouldBlockDraw_WhenTabStateIsNotInitialized_And_NativeIsInitialized_ForPersistedPendingReauth() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        doReturn(false).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);

        doReturn(true).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        doReturn(true)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mPersistentStateMock)
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
        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        // This is called again when we call mNativeInitObserver.onFinishNativeInitialization();
        verify(mTabModelSelectorMock, times(3)).isTabStateInitialized();
    }

    @Test
    @SmallTest
    public void
            testShouldBlockDraw_WhenTabStateIsInitialized_And_WhenNativeIsNotInitialized_ForSavedPendingReauth() {
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

        doReturn(false).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        doReturn(false)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
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
        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mSavedInstanceStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        verify(mTabModelSelectorMock, times(2)).isTabStateInitialized();
    }

    @Test
    @SmallTest
    public void
            testShouldBlockDraw_WhenTabStateIsInitialized_And_WhenNativeIsNotInitialized_ForPersistentPendingReauth() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        doReturn(false).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);

        doReturn(true).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        doReturn(true)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(true)
                .when(mPersistentStateMock)
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

        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());

        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        verify(mTabModelSelectorMock, times(2)).isTabStateInitialized();
    }

    @Test
    @SmallTest
    public void
            testShouldBlockDraw_WhenTabStateIsInitialized_And_WhenNativeIsNotInitialized_NullPersistentState() {
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

        mPersistentStateMock = null;

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
        verify(mCipherFactoryMock, never()).restoreFromPersistableBundle(any());
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
    public void
            testShouldBlockDraw_WhenTabStateIsInitialized_And_WhenNativeIsNotInitialized_NullSavedState() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        mSavedInstanceStateMock = null;

        doReturn(true).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        doReturn(true)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(true)
                .when(mPersistentStateMock)
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
        verify(mCipherFactoryMock, never()).restoreFromBundle(any());
        verify(mCipherFactoryMock, times(1)).restoreFromPersistableBundle(mPersistentStateMock);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        verify(mPersistentStateMock, times(1))
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);
        verify(mPersistentStateMock, times(1))
                .getLong(PREVIOUS_VERSION_CODE, BuildConfig.VERSION_CODE);

        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        verify(mTabModelSelectorMock, times(2)).isTabStateInitialized();
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDraw_WhenNotUpdatingApp_ForPersistedPendingReauth() {
        // Premise conditions
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        doReturn(false).when(mCipherFactoryMock).restoreFromBundle(mSavedInstanceStateMock);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(false)
                .when(mSavedInstanceStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);

        doReturn(BuildConfig.VERSION_CODE)
                .when(mPersistentStateMock)
                .getLong(eq(PREVIOUS_VERSION_CODE), anyLong());

        doReturn(true).when(mCipherFactoryMock).restoreFromPersistableBundle(mPersistentStateMock);
        doReturn(true)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoReauthControllerImpl.KEY_IS_INCOGNITO_REAUTH_PENDING, false);
        doReturn(true)
                .when(mPersistentStateMock)
                .getBoolean(IncognitoRestoreAppLaunchDrawBlocker.IS_INCOGNITO_SELECTED, false);

        doReturn(true)
                .when(mIntentMock)
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);

        // Test condition
        doReturn(true).when(mTabModelSelectorMock).isTabStateInitialized();

        // mNativeInitObserver.onFinishNativeInitialization();
        assertFalse("Should block draw.", mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw());

        // Verify all the mocks were called.
        verify(mCipherFactoryMock, times(1)).restoreFromBundle(mSavedInstanceStateMock);
        verify(mPersistentStateMock, times(1)).getLong(eq(PREVIOUS_VERSION_CODE), anyLong());
        verify(mIntentMock, times(1))
                .getBooleanExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
        verify(mTabModelSelectorMock, times(1)).isTabStateInitialized();
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
