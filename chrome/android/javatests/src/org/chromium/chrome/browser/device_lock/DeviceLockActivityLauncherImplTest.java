// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.device_lock;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.ui.base.WindowAndroid;

/** Tests for the {@link DeviceLockActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DeviceLockActivityLauncherImplTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WindowAndroid.IntentCallback mIntentCallback;

    @Test
    @MediumTest
    public void testLaunchDeviceLockActivity_launchesIntent() {
        DeviceLockActivityLauncherImpl.get()
                .launchDeviceLockActivity(
                        mContext,
                        "testSelectedAccount",
                        true,
                        mWindowAndroid,
                        mIntentCallback,
                        DeviceLockActivityLauncher.Source.SYNC_CONSENT);
        verify(mWindowAndroid, times(1))
                .showIntent(any(Intent.class), eq(mIntentCallback), isNull());
    }

    @Test
    @MediumTest
    public void testLaunchDeviceLockActivity_launchesDeviceLockActivity() {
        doAnswer(
                        (invocation) -> {
                            Intent intent = invocation.getArgument(0);
                            assertEquals(
                                    DeviceLockActivity.class.getName(),
                                    intent.getComponent().getClassName());
                            return null;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());
        DeviceLockActivityLauncherImpl.get()
                .launchDeviceLockActivity(
                        mContext,
                        "testSelectedAccount",
                        true,
                        mWindowAndroid,
                        mIntentCallback,
                        DeviceLockActivityLauncher.Source.SYNC_CONSENT);
    }
}
