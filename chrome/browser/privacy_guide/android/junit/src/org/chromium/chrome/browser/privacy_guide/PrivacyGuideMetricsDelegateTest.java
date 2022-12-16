// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
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
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Set;

/**
 * JUnit tests of the class {@link PrivacyGuideMetricsDelegate}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PrivacyGuideMetricsDelegateTest {
    private static final String SETTINGS_STATES_HISTOGRAM = "Settings.PrivacyGuide.SettingsStates";
    private static final String NEXT_NAVIGATION_HISTOGRAM = "Settings.PrivacyGuide.NextNavigation";

    @Rule
    public JniMocker mocker = new JniMocker();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Profile mProfile;
    @Mock
    private UnifiedConsentServiceBridge.Natives mNativeMock;
    @Mock
    private SyncService mSyncService;
    @Mock
    private Set<Integer> mSyncTypes;
    @Mock
    private SafeBrowsingBridge.Natives mSafeBrowsingNativeMock;
    @Mock
    private PrefService mPrefServiceMock;
    @Mock
    private UserPrefs.Natives mUserPrefsNativesMock;

    private final PrivacyGuideMetricsDelegate mPrivacyGuideMetricsDelegate =
            new PrivacyGuideMetricsDelegate();
    private final UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mProfile);
        mocker.mock(UnifiedConsentServiceBridgeJni.TEST_HOOKS, mNativeMock);
        SyncService.overrideForTests(mSyncService);
        when(mSyncService.getSelectedTypes()).thenReturn(mSyncTypes);
        mocker.mock(SafeBrowsingBridgeJni.TEST_HOOKS, mSafeBrowsingNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNativesMock);
        when(mUserPrefsNativesMock.get(mProfile)).thenReturn(mPrefServiceMock);
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    private void mockMSBBState(boolean initialMSBBState, boolean finalMSBBState) {
        when(mNativeMock.isUrlKeyedAnonymizedDataCollectionEnabled(mProfile))
                .thenReturn(initialMSBBState, finalMSBBState);
    }
    private void mockSafeBrowsingState(@SafeBrowsingState int initialSafeBrowsingState,
            @SafeBrowsingState int finalSafeBrowsingState) {
        when(mSafeBrowsingNativeMock.getSafeBrowsingState())
                .thenReturn(initialSafeBrowsingState, finalSafeBrowsingState);
    }

    private void mockHistorySyncState(
            boolean initialHistorySyncState, boolean finalHistorySyncState) {
        when(mSyncTypes.contains(UserSelectableType.HISTORY))
                .thenReturn(initialHistorySyncState, finalHistorySyncState);
    }

    private void mockCookieControlsMode(
            @CookieControlsMode int initialCookiesMode, @CookieControlsMode int finalCookiesMode) {
        when(mPrefServiceMock.getInteger(PrefNames.COOKIE_CONTROLS_MODE))
                .thenReturn(initialCookiesMode, finalCookiesMode);
    }

    private void triggerMetricsOnNext(@PrivacyGuideFragment.FragmentType int fragmentType) {
        mPrivacyGuideMetricsDelegate.setInitialStateForCard(fragmentType);
        mPrivacyGuideMetricsDelegate.recordMetricsOnNextForCard(fragmentType);
    }

    @Test
    @SmallTest
    public void testMSBB_offToOffSettingsStatesHistogram() {
        mockMSBBState(false, false);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
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
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
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
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
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
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_ON));
    }

    @Test
    @SmallTest
    public void testMSBB_nextClickUserAction() {
        mockMSBBState(false, false);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickMSBB"));
    }

    @Test
    @SmallTest
    public void testMSBB_nextNavigationHistogram() {
        mockMSBBState(false, false);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.MSBB_NEXT_BUTTON));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.MSBB_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    public void testHistorySync_offToOffSettingsStatesHistogram() {
        mockHistorySyncState(false, false);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF));
    }

    @Test
    @SmallTest
    public void testHistorySync_offToOnSettingsStatesHistogram() {
        mockHistorySyncState(false, true);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON));
    }

    @Test
    @SmallTest
    public void testHistorySync_onToOffSettingsStatesHistogram() {
        mockHistorySyncState(true, false);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF));
    }

    @Test
    @SmallTest
    public void testHistorySync_onToOnSettingsStatesHistogram() {
        mockHistorySyncState(true, true);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON));
    }

    @Test
    @SmallTest
    public void testHistorySync_nextClickUserAction() {
        mockHistorySyncState(false, false);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickHistorySync"));
    }

    @Test
    @SmallTest
    public void testHistorySync_nextNavigationHistogram() {
        mockHistorySyncState(false, false);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_enhanceToEnhanceSettingsStatesHistogram() {
        mockSafeBrowsingState(
                SafeBrowsingState.ENHANCED_PROTECTION, SafeBrowsingState.ENHANCED_PROTECTION);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED));
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_enhanceToStandardSettingsStatesHistogram() {
        mockSafeBrowsingState(
                SafeBrowsingState.ENHANCED_PROTECTION, SafeBrowsingState.STANDARD_PROTECTION);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD));
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_standardToEnhanceSettingsStatesHistogram() {
        mockSafeBrowsingState(
                SafeBrowsingState.ENHANCED_PROTECTION, SafeBrowsingState.STANDARD_PROTECTION);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD));
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_standardToStandardSettingsStatesHistogram() {
        mockSafeBrowsingState(
                SafeBrowsingState.STANDARD_PROTECTION, SafeBrowsingState.STANDARD_PROTECTION);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD));
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_nextClickUserAction() {
        mockSafeBrowsingState(
                SafeBrowsingState.STANDARD_PROTECTION, SafeBrowsingState.STANDARD_PROTECTION);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickSafeBrowsing"));
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_nextNavigationHistogram() {
        mockSafeBrowsingState(
                SafeBrowsingState.STANDARD_PROTECTION, SafeBrowsingState.STANDARD_PROTECTION);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON));
    }

    @Test
    @SmallTest
    public void testCookies_block3PIncognitoTo3PIncognitoSettingsStatesHistogram() {
        mockCookieControlsMode(
                CookieControlsMode.INCOGNITO_ONLY, CookieControlsMode.INCOGNITO_ONLY);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO));
    }

    @Test
    @SmallTest
    public void testCookies_block3PIncognitoTo3PSettingsStatesHistogram() {
        mockCookieControlsMode(
                CookieControlsMode.INCOGNITO_ONLY, CookieControlsMode.BLOCK_THIRD_PARTY);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P));
    }

    @Test
    @SmallTest
    public void testCookies_block3PTo3PIncognitoSettingsStatesHistogram() {
        mockCookieControlsMode(
                CookieControlsMode.BLOCK_THIRD_PARTY, CookieControlsMode.INCOGNITO_ONLY);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO));
    }

    @Test
    @SmallTest
    public void testCookies_block3PTo3PSettingsStatesHistogram() {
        mockCookieControlsMode(
                CookieControlsMode.BLOCK_THIRD_PARTY, CookieControlsMode.BLOCK_THIRD_PARTY);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_TO3P));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_TO3P));
    }

    @Test
    @SmallTest
    public void testCookies_nextClickUserAction() {
        mockCookieControlsMode(
                CookieControlsMode.INCOGNITO_ONLY, CookieControlsMode.INCOGNITO_ONLY);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickCookies"));
    }

    @Test
    @SmallTest
    public void testCookies_nextNavigationHistogram() {
        mockCookieControlsMode(
                CookieControlsMode.INCOGNITO_ONLY, CookieControlsMode.INCOGNITO_ONLY);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COOKIES_NEXT_BUTTON));
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COOKIES_NEXT_BUTTON));
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
    public void testHistorySync_changeHistorySyncOnUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnHistorySyncChange(true);
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeHistorySyncOn"));
    }

    @Test
    @SmallTest
    public void testHistorySync_changeHistorySyncOffUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnHistorySyncChange(false);
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
    public void testHistorySync_backClickUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickHistorySync"));
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_backClickUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickSafeBrowsing"));
    }

    @Test
    @SmallTest
    public void testCookies_backClickUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                PrivacyGuideFragment.FragmentType.COOKIES);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickCookies"));
    }
}
