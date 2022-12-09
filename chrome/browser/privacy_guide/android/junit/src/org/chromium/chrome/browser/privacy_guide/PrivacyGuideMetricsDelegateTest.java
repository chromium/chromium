// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;
import org.chromium.components.content_settings.CookieControlsMode;

/**
 * JUnit tests of the class {@link PrivacyGuideMetricsDelegate}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PrivacyGuideMetricsDelegateTest {
    private static final String SETTINGS_STATES_HISTOGRAM = "Settings.PrivacyGuide.SettingsStates";

    @Rule
    public JniMocker mocker = new JniMocker();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Profile mProfile;
    @Mock
    private UnifiedConsentServiceBridge.Natives mNativeMock;

    private final PrivacyGuideMetricsDelegate mPrivacyGuideMetricsDelegate =
            new PrivacyGuideMetricsDelegate();
    private final UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mProfile);
        mocker.mock(UnifiedConsentServiceBridgeJni.TEST_HOOKS, mNativeMock);
    }

    private void mockMSBBState(boolean initialMSBBState, boolean finalMSBBState) {
        when(mNativeMock.isUrlKeyedAnonymizedDataCollectionEnabled(mProfile))
                .thenReturn(initialMSBBState, finalMSBBState);
    }

    private void triggerMSBBMetricsOnNext() {
        mPrivacyGuideMetricsDelegate.setInitialStateForCard(PrivacyGuideFragment.FragmentType.MSBB);
        mPrivacyGuideMetricsDelegate.recordMetricsOnNextForCard(
                PrivacyGuideFragment.FragmentType.MSBB);
    }

    @Test
    @SmallTest
    public void testMSBB_offToOffSettingsStatesHistogram() {
        mockMSBBState(false, false);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF));
        triggerMSBBMetricsOnNext();
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF));
    }

    @Test
    @SmallTest
    public void testMSBB_offToOnSettingsStatesHistogram() {
        mockMSBBState(false, true);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON));
        triggerMSBBMetricsOnNext();
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON));
    }

    @Test
    @SmallTest
    public void testMSBB_onToOffSettingsStatesHistogram() {
        mockMSBBState(true, false);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_OFF));
        triggerMSBBMetricsOnNext();
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_OFF));
    }

    @Test
    @SmallTest
    public void testMSBB_onToOnSettingsStatesHistogram() {
        mockMSBBState(true, true);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_ON));
        triggerMSBBMetricsOnNext();
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_ON));
    }

    @Test
    @SmallTest
    public void testMSBB_nextClickUserAction() {
        mockMSBBState(false, false);
        triggerMSBBMetricsOnNext();
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickMSBB"));
    }

    @Test
    @SmallTest
    public void testMSBB_changeMSBBOnUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnMSBBChange(true);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeMSBBOn"));
    }

    @Test
    @SmallTest
    public void testMSBB_changeMSBBOffUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnMSBBChange(false);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeMSBBOff"));
    }

    @Test
    @SmallTest
    public void testSync_changeHistorySyncOnUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnSyncChange(true);
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeHistorySyncOn"));
    }

    @Test
    @SmallTest
    public void testSync_changeHistorySyncOffUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnSyncChange(false);
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeHistorySyncOff"));
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_changeSafeBrowsingEnhancedUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnSafeBrowsingChange(
                SafeBrowsingState.ENHANCED_PROTECTION);
        assertTrue(mActionTester.getActions().contains(
                "Settings.PrivacyGuide.ChangeSafeBrowsingEnhanced"));
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_changeSafeBrowsingStandardUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnSafeBrowsingChange(
                SafeBrowsingState.STANDARD_PROTECTION);
        assertTrue(mActionTester.getActions().contains(
                "Settings.PrivacyGuide.ChangeSafeBrowsingStandard"));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testSafeBrowsing_changeSafeBrowsingOff() {
        PrivacyGuideMetricsDelegate.recordMetricsOnSafeBrowsingChange(
                SafeBrowsingState.NO_SAFE_BROWSING);
    }

    @Test
    @SmallTest
    public void testCookies_changeCookiesBlock3PIncognitoUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnCookieControlsChange(
                CookieControlsMode.INCOGNITO_ONLY);
        assertTrue(mActionTester.getActions().contains(
                "Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito"));
    }

    @Test
    @SmallTest
    public void testCookies_changeCookiesBlock3PUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnCookieControlsChange(
                CookieControlsMode.BLOCK_THIRD_PARTY);
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeCookiesBlock3P"));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testCookies_changeCookiesOff() {
        PrivacyGuideMetricsDelegate.recordMetricsOnCookieControlsChange(CookieControlsMode.OFF);
    }

    @Test
    @SmallTest
    public void testSync_backClickUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                PrivacyGuideFragment.FragmentType.SYNC);
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickHistorySync"));
    }
}
