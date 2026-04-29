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

import org.chromium.base.ScreenStateReceiver.ScreenStateObserver;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

/** Unit tests for {@link ScreenStateReceiver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ScreenStateReceiverTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        // Initialize the receiver to ensure it is registered.
        ScreenStateReceiver.getInstance();
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @After
    public void tearDown() {
        // Reset to ensure our observers are removed and statics are reset.
        ScreenStateReceiver.resetForTesting();
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testScreenOffReceiver() {
        ScreenStateObserver observer = mock(ScreenStateObserver.class);
        ScreenStateReceiver.addObserver(observer);

        // Send screen off broadcast.
        Intent intent = new Intent(Intent.ACTION_SCREEN_OFF);
        sendIntent(intent);
        verify(observer).onScreenOff(any(Context.class), eq(intent));

        ScreenStateReceiver.removeObserver(observer);

        // Verify not called a second time.
        sendIntent(intent);
        verify(observer).onScreenOff(any(Context.class), eq(intent));
    }

    @Test
    public void testScreenOnReceiver() {
        ScreenStateObserver observer = mock(ScreenStateObserver.class);
        ScreenStateReceiver.addObserver(observer);

        // Send screen on broadcast.
        Intent intent = new Intent(Intent.ACTION_SCREEN_ON);
        sendIntent(intent);
        verify(observer).onScreenOn(any(Context.class), eq(intent));

        ScreenStateReceiver.removeObserver(observer);
    }

    @Test
    public void testOtherBroadcastsIgnored() {
        ScreenStateObserver observer = mock(ScreenStateObserver.class);
        ScreenStateReceiver.addObserver(observer);

        // Send a different broadcast.
        Intent intent = new Intent(Intent.ACTION_USER_PRESENT);
        sendIntent(intent);

        // Verify observer was NOT called.
        verify(observer, never()).onScreenOff(any(Context.class), eq(intent));
        verify(observer, never()).onScreenOn(any(Context.class), eq(intent));
    }

    private void sendIntent(Intent intent) {
        ContextUtils.getApplicationContext().sendBroadcast(intent);
        Shadows.shadowOf(Looper.getMainLooper()).runToEndOfTasks();
    }
}
