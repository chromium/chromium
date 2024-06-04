// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.messages.MessageDispatcher;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrackingProtectionOnboardingViewTest {
    @Mock private MessageDispatcher mMessageDispatcher;
    @Mock private SettingsLauncher mSettingsLauncher;
    @Mock private Callback<Boolean> mNoticeShownCallback;
    @Mock private Callback<Integer> mNoticeDismissedCallback;
    @Mock private Supplier<Integer> mNoticePrimaryActionCallback;
    @Mock private Resources mResources;
    @Mock private Context mContext;
    private TrackingProtectionOnboardingView mView;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getString(any(Integer.class))).thenReturn("Test String"); // Simplified
        mView =
                new TrackingProtectionOnboardingView(
                        mContext, mMessageDispatcher, mSettingsLauncher);
    }

    @Test
    public void testShowNotice_Onboarding() {
        mView.showNotice(
                mNoticeShownCallback, mNoticeDismissedCallback, mNoticePrimaryActionCallback);
        verify(mMessageDispatcher).enqueueWindowScopedMessage(any(), eq(true));
    }

    @Test
    public void testIsNoticeShowing_AfterShow() {
        assertFalse(mView.wasNoticeRequested());
        mView.showNotice(
                mNoticeShownCallback, mNoticeDismissedCallback, mNoticePrimaryActionCallback);
        assertTrue(mView.wasNoticeRequested());
    }
}
