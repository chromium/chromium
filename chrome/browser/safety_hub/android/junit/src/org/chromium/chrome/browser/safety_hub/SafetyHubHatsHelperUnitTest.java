// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.ui.hats.SurveyClient;
import org.chromium.chrome.browser.ui.hats.SurveyClientFactory;
import org.chromium.chrome.browser.ui.hats.TestSurveyUtils;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** UnitTests for {@link SafetyHubHatsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyHubHatsHelperUnitTest {
    private static final String EXAMPLE_URL = "http://example1.com";
    private static final PermissionsData PERMISSIONS_DATA =
            PermissionsData.create(
                    EXAMPLE_URL,
                    new int[] {
                        ContentSettingsType.MEDIASTREAM_CAMERA,
                    },
                    0,
                    0);
    private static final NotificationPermissions NOTIFICATION_PERMISSIONS =
            NotificationPermissions.create(EXAMPLE_URL, "*", 3);
    private static final String HATS_SURVEY_TRIGGER_ID = "safety_hub_android_organic_survey";

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SafetyHubHatsBridge.Natives mSafetyHubHatsBridgeNatives;
    @Mock private UserPrefs.Natives mUserPrefsNativeMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private SafetyHubFetchService mSafetyHubFetchService;
    @Mock private SafeBrowsingBridge.Natives mSafeBrowsingBridgeNativeMock;
    @Mock private UnusedSitePermissionsBridge.Natives mUnusedPermissionsNativeMock;

    @Mock
    private NotificationPermissionReviewBridge.Natives mNotificationPermissionReviewNativeMock;

    @Mock private SurveyClientFactory mSurveyFactory;
    @Mock private SurveyClient mSurveyClient;
    @Mock private Activity mActivity;

    @Mock private Profile mProfile;

    SafetyHubHatsHelper mSafetyHubHatsHelper;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNativeMock);
        doReturn(mPrefServiceMock).when(mUserPrefsNativeMock).get(mProfile);
        mockPasswordCounts(0, 0, 0);

        SafetyHubFetchServiceFactory.setSafetyHubFetchServiceForTesting(mSafetyHubFetchService);
        mockUpdateStatus(UpdateStatusProvider.UpdateState.NONE);

        mJniMocker.mock(UnusedSitePermissionsBridgeJni.TEST_HOOKS, mUnusedPermissionsNativeMock);
        mockUnusedSitePermissions(false);

        mJniMocker.mock(
                NotificationPermissionReviewBridgeJni.TEST_HOOKS,
                mNotificationPermissionReviewNativeMock);
        mockNotificationPermission(false);

        mJniMocker.mock(SafeBrowsingBridgeJni.TEST_HOOKS, mSafeBrowsingBridgeNativeMock);
        mockSafeBrowsing(SafeBrowsingState.STANDARD_PROTECTION);

        mSafetyHubHatsHelper = new SafetyHubHatsHelper(mProfile);

        TestSurveyUtils.setTestSurveyConfigForTrigger(
                HATS_SURVEY_TRIGGER_ID, new String[0], new String[0]);
        SurveyClientFactory.setInstanceForTesting(mSurveyFactory);
        doReturn(mSurveyClient).when(mSurveyFactory).createClient(any(), any(), any());
    }

    private void mockPasswordCounts(int compromised, int weak, int reused) {
        doReturn(compromised).when(mPrefServiceMock).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        doReturn(weak).when(mPrefServiceMock).getInteger(Pref.WEAK_CREDENTIALS_COUNT);
        doReturn(reused).when(mPrefServiceMock).getInteger(Pref.REUSED_CREDENTIALS_COUNT);
    }

    private void mockUpdateStatus(@UpdateState int updateState) {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = updateState;
        doReturn(updateStatus).when(mSafetyHubFetchService).getUpdateStatus();
    }

    private void mockUnusedSitePermissions(boolean hasUnusedSitePermissions) {
        if (hasUnusedSitePermissions) {
            doReturn(new PermissionsData[] {PERMISSIONS_DATA})
                    .when(mUnusedPermissionsNativeMock)
                    .getRevokedPermissions(mProfile);
        } else {
            doReturn(new PermissionsData[] {})
                    .when(mUnusedPermissionsNativeMock)
                    .getRevokedPermissions(mProfile);
        }
    }

    private void mockNotificationPermission(boolean hasNotificationPermissions) {
        if (hasNotificationPermissions) {
            doReturn(new NotificationPermissions[] {NOTIFICATION_PERMISSIONS})
                    .when(mNotificationPermissionReviewNativeMock)
                    .getNotificationPermissions(mProfile);
        } else {
            doReturn(new NotificationPermissions[] {})
                    .when(mNotificationPermissionReviewNativeMock)
                    .getNotificationPermissions(mProfile);
        }
    }

    private void mockSafeBrowsing(@SafeBrowsingState int safeBrowsingState) {
        doReturn(safeBrowsingState)
                .when(mSafeBrowsingBridgeNativeMock)
                .getSafeBrowsingState(mProfile);
    }

    @Test
    public void testOverallState_Safe() {
        String overallState = mSafetyHubHatsHelper.getOverallState();
        // Default mocked values return a safe state.
        assertEquals("Safe", overallState);
    }

    @Test
    public void testOverallState_HasCompromisedPasswords() {
        // Compromised password are higher priority than weak or reused.
        mockPasswordCounts(/* compromised= */ 1, /* weak= */ 1, /* reused= */ 1);
        String overallState = mSafetyHubHatsHelper.getOverallState();
        assertEquals("Warning", overallState);
    }

    @Test
    public void testOverallState_UnavailableCompromisedPasswords() {
        mockPasswordCounts(/* compromised= */ -1, 0, 0);
        String overallState = mSafetyHubHatsHelper.getOverallState();
        assertEquals("Unavailable", overallState);
    }

    @Test
    public void testOverallState_WeakAndReusedPasswords() {
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ 1, /* reused= */ 1);
        String overallState = mSafetyHubHatsHelper.getOverallState();
        assertEquals("Info", overallState);
    }

    @Test
    public void testOverallState_UpdateAvailable() {
        mockUpdateStatus(UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE);
        String overallState = mSafetyHubHatsHelper.getOverallState();
        assertEquals("Warning", overallState);
    }

    @Test
    public void testOverallState_UnsupportedOs() {
        mockUpdateStatus(UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION);
        String overallState = mSafetyHubHatsHelper.getOverallState();
        assertEquals("Unavailable", overallState);
    }

    @Test
    public void testOverallState_HasUnusedSitePermissions() {
        mockUnusedSitePermissions(/* hasUnusedSitePermissions= */ true);
        String overallState = mSafetyHubHatsHelper.getOverallState();
        assertEquals("Info", overallState);
    }

    @Test
    public void testOverallState_HasNotificationPermissions() {
        mockNotificationPermission(/* hasNotificationPermissions= */ true);
        String overallState = mSafetyHubHatsHelper.getOverallState();
        assertEquals("Info", overallState);
    }

    @Test
    public void testOverallState_NoSafeBrowsing() {
        mockSafeBrowsing(SafeBrowsingState.NO_SAFE_BROWSING);
        String overallState = mSafetyHubHatsHelper.getOverallState();
        assertEquals("Warning", overallState);
    }

    @Test
    public void testOverallState_ModulesWithDifferentStates() {
        // The notification module is in the information state.
        mockNotificationPermission(/* hasNotificationPermissions= */ true);
        // The passwords module is in the warning state.
        mockPasswordCounts(/* compromised= */ 1, 0, 0);

        String overallState = mSafetyHubHatsHelper.getOverallState();

        // The most severe state of the modules is the overall state.
        assertEquals("Warning", overallState);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB_ANDROID_ORGANIC_SURVEY)
    public void testTriggerOrganicHatsSurvey() {
        mSafetyHubHatsHelper.triggerOrganicHatsSurvey(mActivity);
        verify(mSurveyClient, times(1)).showSurvey(eq(mActivity), eq(null), any(), any());
    }
}
