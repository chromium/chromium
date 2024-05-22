// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.mockito.ArgumentMatchers.any;
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
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

    @Mock TrackingProtectionModeBOnboardingView mTrackingProtectionModeBOnboardingView;
    @Mock private Context mContext;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private SecurityStateModel.Natives mSecurityStateModelNatives;

    @Mock private WebContents mWebContents;

    @Before
    public void setUp() {
        when(mTab.isIncognito()).thenReturn(false); // Assume non-incognito tab
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mTrackingProtectionBridge.getRequiredNotice()).thenReturn(NoticeType.ONBOARDING);
        mController =
                TrackingProtectionOnboardingController.create(
                        mContext,
                        mTrackingProtectionBridge,
                        mActivityTabProvider,
                        mMessageDispatcher,
                        mSettingsLauncher,
                        TrackingProtectionOnboardingType.MODE_B);
        mController.setTrackingProtectionModeBOnboardingView(
                mTrackingProtectionModeBOnboardingView);

        mJniMocker.mock(SecurityStateModelJni.TEST_HOOKS, mSecurityStateModelNatives);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_ONBOARDING_SKIP_SECURE_PAGE_CHECK)
    @EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_NOTICE_REQUEST_TRACKING)
    public void testMaybeOnboard_ShowsNotice() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenReturn(ConnectionSecurityLevel.SECURE);
        mController.maybeOnboard(mTab, TrackingProtectionOnboardingType.MODE_B);
        verify(mTrackingProtectionModeBOnboardingView).showNotice(any(), any(), any());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.TRACKING_PROTECTION_ONBOARDING_SKIP_SECURE_PAGE_CHECK,
        ChromeFeatureList.TRACKING_PROTECTION_NOTICE_REQUEST_TRACKING
    })
    public void testMaybeOnboard_SecureConnection_ShowsNotice() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenReturn(ConnectionSecurityLevel.SECURE);
        mController.maybeOnboard(mTab, TrackingProtectionOnboardingType.MODE_B);
        verify(mTrackingProtectionModeBOnboardingView).showNotice(any(), any(), any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_ONBOARDING_SKIP_SECURE_PAGE_CHECK)
    @EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_NOTICE_REQUEST_TRACKING)
    public void testMaybeOnboard_NotSecureConnection_DoesNotShowNotice() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenReturn(ConnectionSecurityLevel.NONE);
        mController.maybeOnboard(mTab, TrackingProtectionOnboardingType.MODE_B);
        verify(mTrackingProtectionModeBOnboardingView, never()).showNotice(any(), any(), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TRACKING_PROTECTION_ONBOARDING_SKIP_SECURE_PAGE_CHECK)
    public void testMaybeOnboard_SilentOnboarding_NoNotice() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(any()))
                .thenReturn(ConnectionSecurityLevel.SECURE);
        when(mTrackingProtectionBridge.getRequiredNotice())
                .thenReturn(NoticeType.SILENT_ONBOARDING);
        mController.maybeOnboard(mTab, TrackingProtectionOnboardingType.MODE_B);
        verify(mTrackingProtectionBridge).noticeShown(NoticeType.SILENT_ONBOARDING);
        verify(mTrackingProtectionModeBOnboardingView, never()).showNotice(any(), any(), any());
    }
}
