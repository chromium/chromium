// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;
import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.ScreenOffBroadcastReceiver.ScreenOffListener;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ScreenOffBroadcastReceiver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ScreenOffBroadcastReceiverTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        // Initialize the receiver to ensure it is registered.
        ScreenOffBroadcastReceiver.getInstance();
        BaseRobolectricTestRule.runAllBackgroundAndUi();
    }

    @After
    public void tearDown() {
        // Reset to ensure our listeners are removed and statics are reset.
        ScreenOffBroadcastReceiver.resetForTesting();
        BaseRobolectricTestRule.runAllBackgroundAndUi();
    }

    @Test
    public void testScreenOffReceiver() {
        ScreenOffListener listener = mock(ScreenOffListener.class);
        ScreenOffBroadcastReceiver.addListener(listener);

        // Send screen off broadcast.
        Intent intent = new Intent(Intent.ACTION_SCREEN_OFF);
        sendIntent(intent);
        verify(listener).onScreenOff(any(Context.class), any(Intent.class));

        ScreenOffBroadcastReceiver.removeListener(listener);

        // Verify not called a second time.
        sendIntent(intent);
        verify(listener).onScreenOff(any(Context.class), any(Intent.class));
    }

    @Test
    public void testOtherBroadcastsIgnored() {
        ScreenOffListener listener = mock(ScreenOffListener.class);
        ScreenOffBroadcastReceiver.addListener(listener);

        // Send a different broadcast.
        Intent intent = new Intent(Intent.ACTION_SCREEN_ON);
        sendIntent(intent);

        // Verify listener was NOT called.
        verify(listener, never()).onScreenOff(any(Context.class), eq(intent));
    }

    private void sendIntent(Intent intent) {
        ContextUtils.getApplicationContext().sendBroadcast(intent);
        Shadows.shadowOf(Looper.getMainLooper()).runToEndOfTasks();
    }
}
