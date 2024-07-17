// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.WebContents;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrackingProtectionOnboardingControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TrackingProtectionBridge mTrackingProtectionBridge;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private MessageDispatcher mMessageDispatcher;
    @Mock private SettingsLauncher mSettingsLauncher;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;

    private TrackingProtectionOnboardingController mController;
    @Mock TrackingProtectionOnboardingView mTrackingProtectionOnboardingView;
    @Mock private Context mContext;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private SecurityStateModel.Natives mSecurityStateModelNatives;

    @Mock private WebContents mWebContents;

    @Before
    public void setUp() {
        when(mTab.isIncognito()).thenReturn(false); // Assume non-incognito tab
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mTrackingProtectionBridge.shouldRunUILogic(anyInt())).thenReturn(true);
        mController =
                new TrackingProtectionOnboardingController(
                        mContext,
                        mTrackingProtectionBridge,
                        mActivityTabProvider,
                        mMessageDispatcher,
                        mSettingsLauncher,
                        SurfaceType.BR_APP);
        mController.setTrackingProtectionOnboardingView(mTrackingProtectionOnboardingView);

        mJniMocker.mock(SecurityStateModelJni.TEST_HOOKS, mSecurityStateModelNatives);
    }

    @Test
    public void testMaybeOnboardFullOnboardingType_ShowsNotice() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenReturn(ConnectionSecurityLevel.SECURE);
        when(mTrackingProtectionBridge.getRequiredNotice(anyInt()))
                .thenReturn(NoticeType.FULL3PCD_ONBOARDING);

        mController.maybeOnboard(mTab);
        verify(mTrackingProtectionOnboardingView)
                .showNotice(any(), any(), any(), eq(NoticeType.FULL3PCD_ONBOARDING));
    }

    @Test
    public void testMaybeOnboard_ShowsNotice() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenReturn(ConnectionSecurityLevel.SECURE);
        when(mTrackingProtectionBridge.getRequiredNotice(anyInt()))
                .thenReturn(NoticeType.MODE_B_ONBOARDING);
        mController.maybeOnboard(mTab);
        verify(mTrackingProtectionOnboardingView)
                .showNotice(any(), any(), any(), eq(NoticeType.MODE_B_ONBOARDING));
    }

    @Test
    public void testMaybeOnboard_NotSecureConnection_DoesNotShowNotice() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenReturn(ConnectionSecurityLevel.NONE);
        when(mTrackingProtectionBridge.getRequiredNotice(anyInt()))
                .thenReturn(NoticeType.MODE_B_ONBOARDING);
        mController.maybeOnboard(mTab);
        verify(mTrackingProtectionOnboardingView, never())
                .showNotice(any(), any(), any(), eq(NoticeType.MODE_B_ONBOARDING));
    }

    @Test
    public void testMaybeOnboard_SilentOnboarding_NoNotice() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenReturn(ConnectionSecurityLevel.SECURE);
        when(mTrackingProtectionBridge.getRequiredNotice(anyInt()))
                .thenReturn(NoticeType.MODE_B_SILENT_ONBOARDING);
        mController.maybeOnboard(mTab);
        verify(mTrackingProtectionBridge)
                .noticeShown(SurfaceType.BR_APP, NoticeType.MODE_B_SILENT_ONBOARDING);
        verify(mTrackingProtectionOnboardingView, never())
                .showNotice(any(), any(), any(), anyInt());
    }

    @Test
    public void testMaybeOnboardFullLaunchOnboardinType_NoNoticeWithoutApproval() {
        when(mTrackingProtectionBridge.shouldRunUILogic(anyInt())).thenReturn(false);
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenReturn(ConnectionSecurityLevel.SECURE);
        when(mTrackingProtectionBridge.getRequiredNotice(anyInt()))
                .thenReturn(NoticeType.FULL3PCD_ONBOARDING);

        assertFalse(mController.shouldShowNotice(mTrackingProtectionBridge));
        mController.maybeOnboard(mTab);
        verify(mTrackingProtectionOnboardingView, never())
                .showNotice(any(), any(), any(), anyInt());
    }

    @Test
    public void testMaybeOnboardModeBLaunchOnboardinType_NoNoticeWithoutApproval() {
        when(mTrackingProtectionBridge.shouldRunUILogic(anyInt())).thenReturn(false);
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenReturn(ConnectionSecurityLevel.SECURE);
        when(mTrackingProtectionBridge.getRequiredNotice(anyInt()))
                .thenReturn(NoticeType.MODE_B_ONBOARDING);

        assertFalse(mController.shouldShowNotice(mTrackingProtectionBridge));
        mController.maybeOnboard(mTab);
        verify(mTrackingProtectionOnboardingView, never())
                .showNotice(any(), any(), any(), anyInt());
    }
}
