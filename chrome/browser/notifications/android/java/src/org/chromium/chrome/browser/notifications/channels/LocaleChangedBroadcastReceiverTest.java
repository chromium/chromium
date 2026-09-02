// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.lifetime.ApplicationLifetime;

/** Unit tests for {@link LocaleChangedBroadcastReceiver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LocaleChangedBroadcastReceiverTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ApplicationLifetime.Observer mObserver;
    @Mock private BroadcastReceiver.PendingResult mPendingResult;

    private Context mContext;
    private LocaleChangedBroadcastReceiver mReceiver;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mReceiver = new LocaleChangedBroadcastReceiver();
        mReceiver.setPendingResult(mPendingResult);
        ApplicationLifetime.addObserver(mObserver);
    }

    @After
    public void tearDown() {
        ApplicationLifetime.setRestartForLocaleSwitch(false);
        ApplicationLifetime.removeObserver(mObserver);
    }

    @Test
    public void testOnReceive_RestartForLocaleSwitchSet_RestartsApplication() {
        ApplicationLifetime.setRestartForLocaleSwitch(true);
        Intent intent = new Intent(Intent.ACTION_LOCALE_CHANGED);
        mReceiver.onReceive(mContext, intent);
        RobolectricUtil.runAllBackgroundAndUiAllowBlocking();

        verify(mPendingResult).finish();
        verify(mObserver).onTerminate(/* restart= */ true);
    }

    @Test
    public void testOnReceive_RestartForLocaleSwitchNotSet_DoesNotRestartApplication() {
        ApplicationLifetime.setRestartForLocaleSwitch(false);
        Intent intent = new Intent(Intent.ACTION_LOCALE_CHANGED);
        mReceiver.onReceive(mContext, intent);
        RobolectricUtil.runAllBackgroundAndUiAllowBlocking();

        verify(mPendingResult).finish();
        verify(mObserver, never()).onTerminate(anyBoolean());
    }
}
