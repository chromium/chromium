// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.device_lock;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for the {@link DeviceLockActivity}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Uses static launcher.")
public class DeviceLockActivityLauncherImplTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Context mContext;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private WindowAndroid.IntentCallback mIntentCallback;
    @Mock
    private KeyguardManager mKeyguardManager;
    @Mock
    private ReauthenticatorBridge mReauthenticatorBridge;

    private final AtomicBoolean mCallbackCalled = new AtomicBoolean();

    @Before
    public void setup() {
        mContext = Mockito.mock(Context.class);
        mKeyguardManager = Mockito.mock(KeyguardManager.class);
        mReauthenticatorBridge = Mockito.mock(ReauthenticatorBridge.class);

        mCallbackCalled.set(false);
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.DEVICE_LOCK_PAGE_HAS_BEEN_PASSED);
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorBridge);

        doReturn(mKeyguardManager).when(mContext).getSystemService(eq(Context.KEYGUARD_SERVICE));
        doReturn(true).when(mKeyguardManager).isDeviceSecure();
    }

    @Test
    @MediumTest
    public void testLaunchDeviceLockActivity_launchesIntent() {
        DeviceLockActivityLauncherImpl.get().launchDeviceLockActivity(
                mContext, "testSelectedAccount", mWindowAndroid, mIntentCallback);
        verify(mWindowAndroid, times(1))
                .showIntent(any(Intent.class), eq(mIntentCallback), isNull());
    }

    @Test
    @MediumTest
    public void testLaunchDeviceLockActivity_launchesDeviceLockActivity() {
        doAnswer((invocation) -> {
            Intent intent = invocation.getArgument(0);
            assertEquals(DeviceLockActivity.class.getName(), intent.getComponent().getClassName());
            return null;
        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());
        DeviceLockActivityLauncherImpl.get().launchDeviceLockActivity(
                mContext, "testSelectedAccount", mWindowAndroid, mIntentCallback);
    }

    @Test
    @MediumTest
    public void
    testEnsureDeviceLockSecureForSignedOutFlow_launchesDeviceLockActivity_callbackCalled() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_PAGE_HAS_BEEN_PASSED, false);

        doAnswer((invocation) -> {
            Intent intent = invocation.getArgument(0);
            assertEquals(DeviceLockActivity.class.getName(), intent.getComponent().getClassName());

            WindowAndroid.IntentCallback callback = invocation.getArgument(1);
            callback.onIntentCompleted(Activity.RESULT_OK, null);
            return null;
        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());

        DeviceLockActivityLauncherImpl.get().presentDeviceLockChallenge(
                mContext, mWindowAndroid, () -> mCallbackCalled.set(true));
        verify(mWindowAndroid, times(1)).showIntent(any(Intent.class), any(), any());
        verify(mReauthenticatorBridge, never()).reauthenticate(any(), anyBoolean());
        assertTrue("The callback should have been called if the activity ends with RESULT_OK.",
                mCallbackCalled.get());
    }

    @Test
    @MediumTest
    public void
    testEnsureDeviceLockSecureForSignedOutFlow_launchesDeviceLockActivity_callbackNotCalled() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_PAGE_HAS_BEEN_PASSED, false);

        doAnswer((invocation) -> {
            Intent intent = invocation.getArgument(0);
            assertEquals(DeviceLockActivity.class.getName(), intent.getComponent().getClassName());

            WindowAndroid.IntentCallback callback = invocation.getArgument(1);
            callback.onIntentCompleted(Activity.RESULT_CANCELED, null);
            return null;
        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());

        DeviceLockActivityLauncherImpl.get().presentDeviceLockChallenge(
                mContext, mWindowAndroid, () -> mCallbackCalled.set(true));
        verify(mWindowAndroid, times(1)).showIntent(any(Intent.class), any(), any());
        verify(mReauthenticatorBridge, never()).reauthenticate(any(), anyBoolean());
        assertFalse("The callback should not have been called if the activity ends with "
                        + "RESULT_CANCELED.",
                mCallbackCalled.get());
    }

    @Test
    @MediumTest
    public void
    testEnsureDeviceLockSecureForSignedOutFlow_reauthenticationChallenge_authSucceeded() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_PAGE_HAS_BEEN_PASSED, true);
        doReturn(mKeyguardManager).when(mContext).getSystemService(eq(Context.KEYGUARD_SERVICE));
        doReturn(true).when(mKeyguardManager).isDeviceSecure();

        doAnswer((invocation) -> {
            Callback<Boolean> callback = invocation.getArgument(0);
            callback.onResult(true);
            return null;
        })
                .when(mReauthenticatorBridge)
                .reauthenticate(any(), anyBoolean());

        DeviceLockActivityLauncherImpl.get().presentDeviceLockChallenge(
                mContext, mWindowAndroid, () -> mCallbackCalled.set(true));
        verify(mWindowAndroid, never()).showIntent(any(Intent.class), any(), any());
        verify(mReauthenticatorBridge, times(1)).reauthenticate(any(), anyBoolean());
        assertTrue("The callback should be called after a successful reauthentication.",
                mCallbackCalled.get());
    }

    @Test
    @MediumTest
    public void testEnsureDeviceLockSecureForSignedOutFlow_reauthenticationChallenge_authFailed() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_PAGE_HAS_BEEN_PASSED, true);
        doReturn(mKeyguardManager).when(mContext).getSystemService(eq(Context.KEYGUARD_SERVICE));
        doReturn(true).when(mKeyguardManager).isDeviceSecure();

        doAnswer((invocation) -> {
            Callback<Boolean> callback = invocation.getArgument(0);
            callback.onResult(false);
            return null;
        })
                .when(mReauthenticatorBridge)
                .reauthenticate(any(), anyBoolean());

        DeviceLockActivityLauncherImpl.get().presentDeviceLockChallenge(
                mContext, mWindowAndroid, () -> mCallbackCalled.set(true));
        verify(mWindowAndroid, never()).showIntent(any(Intent.class), any(), any());
        verify(mReauthenticatorBridge, times(1)).reauthenticate(any(), anyBoolean());
        assertFalse("The callback should be not called after a failed reauthentication.",
                mCallbackCalled.get());
    }
}
