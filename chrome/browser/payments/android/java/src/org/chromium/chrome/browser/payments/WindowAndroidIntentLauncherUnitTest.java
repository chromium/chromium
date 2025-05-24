// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

/** Tests for the Android intent-based payment app launcher. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class WindowAndroidIntentLauncherUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final Intent mIntent = new Intent();

    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Callback<String> mErrorCallback;
    @Mock private IntentCallback mIntentCallback;

    @SmallTest
    @Test
    public void testCannotLaunchPaymentAppForDestroyedWebContents() throws Exception {
        var launcher = new WindowAndroidIntentLauncher(mWebContents);
        when(mWebContents.isDestroyed()).thenReturn(true);

        launcher.launchPaymentApp(mIntent, mErrorCallback, mIntentCallback);

        verify(mWindowAndroid, never()).showIntent(any(Intent.class), any(), any());
        verify(mErrorCallback).onResult("Unable to invoke the payment app.");
    }

    @SmallTest
    @Test
    public void testCannotLaunchPaymentAppForNullTopLevelNativeWindow() throws Exception {
        var launcher = new WindowAndroidIntentLauncher(mWebContents);
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(null);

        launcher.launchPaymentApp(mIntent, mErrorCallback, mIntentCallback);

        verify(mWindowAndroid, never()).showIntent(any(Intent.class), any(), any());
        verify(mErrorCallback).onResult("Unable to invoke the payment app.");
    }

    @SmallTest
    @Test
    public void testErrorCallbackWhenCannotLaunchIntent() throws Exception {
        var launcher = new WindowAndroidIntentLauncher(mWebContents);
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.showIntent(any(Intent.class), any(), any())).thenReturn(false);

        launcher.launchPaymentApp(mIntent, mErrorCallback, mIntentCallback);

        verify(mErrorCallback).onResult("Unable to invoke the payment app.");
    }

    @SmallTest
    @Test
    public void testErrorCallbackWhenPrivateActivity() throws Exception {
        var launcher = new WindowAndroidIntentLauncher(mWebContents);
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.showIntent(any(Intent.class), any(), any()))
                .thenThrow(new SecurityException());

        launcher.launchPaymentApp(mIntent, mErrorCallback, mIntentCallback);

        verify(mErrorCallback)
                .onResult(
                        "Payment app does not have android:exported=\"true\" on the PAY activity.");
    }

    @SmallTest
    @Test
    public void testNoErrorCallbackOnSuccessfulIntentLaunch() throws Exception {
        var launcher = new WindowAndroidIntentLauncher(mWebContents);
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.showIntent(any(Intent.class), any(), any())).thenReturn(true);

        launcher.launchPaymentApp(mIntent, mErrorCallback, mIntentCallback);

        verify(mErrorCallback, never()).onResult(any());
        verify(mWindowAndroid)
                .showIntent(
                        eq(mIntent), eq(mIntentCallback), eq(R.string.payments_android_app_error));
    }
}
