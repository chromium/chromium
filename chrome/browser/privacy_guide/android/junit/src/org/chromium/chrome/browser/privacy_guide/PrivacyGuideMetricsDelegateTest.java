// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.eq;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Set;

/** JUnit tests of the class {@link PrivacyGuideMetricsDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
public class PrivacyGuideMetricsDelegateTest {
    private static final String SETTINGS_STATES_HISTOGRAM = "Settings.PrivacyGuide.SettingsStates";
    private static final String NEXT_NAVIGATION_HISTOGRAM = "Settings.PrivacyGuide.NextNavigation";

    @Rule public JniMocker mocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private UnifiedConsentServiceBridge.Natives mNativeMock;
    @Mock private SyncService mSyncService;
    @Mock private Set<Integer> mSyncTypes;
    @Mock private SafeBrowsingBridge.Natives mSafeBrowsingNativeMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private UserPrefs.Natives mUserPrefsNativesMock;

    private final UserActionTester mActionTester = new UserActionTester();

    private PrivacyGuideMetricsDelegate mPrivacyGuideMetricsDelegate;

    @Before
    public void setUp() {
        mPrivacyGuideMetricsDelegate = new PrivacyGuideMetricsDelegate(mProfile);

        mocker.mock(UnifiedConsentServiceBridgeJni.TEST_HOOKS, mNativeMock);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
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

    private void mockSafeBrowsingState(
            @SafeBrowsingState int initialSafeBrowsingState,
            @SafeBrowsingState int finalSafeBrowsingState) {
        when(mSafeBrowsingNativeMock.getSafeBrowsingState(eq(mProfile)))
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

    private void mockSearchSuggestionsState(
            boolean initialSearchSuggestionsState, boolean finalSearchSuggestionsState) {
        when(mPrefServiceMock.getBoolean(Pref.SEARCH_SUGGEST_ENABLED))
                .thenReturn(initialSearchSuggestionsState, finalSearchSuggestionsState);
    }

    private void mockAdTopicsState(boolean initialAdTopicsState, boolean finalAdTopicsState) {
        when(mPrefServiceMock.getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED))
                .thenReturn(initialAdTopicsState, finalAdTopicsState);
    }

    private void triggerMetricsOnNext(@PrivacyGuideFragment.FragmentType int fragmentType) {
        mPrivacyGuideMetricsDelegate.setInitialStateForCard(fragmentType);
        mPrivacyGuideMetricsDelegate.recordMetricsOnNextForCard(fragmentType);
    }

    @Test
    @SmallTest
    public void testMSBB_offToOffSettingsStatesHistogram() {
        mockMSBBState(false, false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testMSBB_offToOnSettingsStatesHistogram() {
        mockMSBBState(false, true);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_OFF_TO_ON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testMSBB_onToOffSettingsStatesHistogram() {
        mockMSBBState(true, false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_OFF);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testMSBB_onToOnSettingsStatesHistogram() {
        mockMSBBState(true, true);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.MSBB_ON_TO_ON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
        watcher.assertExpected();
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
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.MSBB_NEXT_BUTTON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.MSBB);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testHistorySync_offToOffSettingsStatesHistogram() {
        mockHistorySyncState(false, false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testHistorySync_offToOnSettingsStatesHistogram() {
        mockHistorySyncState(false, true);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testHistorySync_onToOffSettingsStatesHistogram() {
        mockHistorySyncState(true, false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testHistorySync_onToOnSettingsStatesHistogram() {
        mockHistorySyncState(true, true);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        watcher.assertExpected();
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
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_enhanceToEnhanceSettingsStatesHistogram() {
        mockSafeBrowsingState(
                SafeBrowsingState.ENHANCED_PROTECTION, SafeBrowsingState.ENHANCED_PROTECTION);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_enhanceToStandardSettingsStatesHistogram() {
        mockSafeBrowsingState(
                SafeBrowsingState.ENHANCED_PROTECTION, SafeBrowsingState.STANDARD_PROTECTION);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_standardToEnhanceSettingsStatesHistogram() {
        mockSafeBrowsingState(
                SafeBrowsingState.ENHANCED_PROTECTION, SafeBrowsingState.STANDARD_PROTECTION);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_standardToStandardSettingsStatesHistogram() {
        mockSafeBrowsingState(
                SafeBrowsingState.STANDARD_PROTECTION, SafeBrowsingState.STANDARD_PROTECTION);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        watcher.assertExpected();
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
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCookies_block3PIncognitoTo3PIncognitoSettingsStatesHistogram() {
        mockCookieControlsMode(
                CookieControlsMode.INCOGNITO_ONLY, CookieControlsMode.INCOGNITO_ONLY);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCookies_block3PIncognitoTo3PSettingsStatesHistogram() {
        mockCookieControlsMode(
                CookieControlsMode.INCOGNITO_ONLY, CookieControlsMode.BLOCK_THIRD_PARTY);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCookies_block3PTo3PIncognitoSettingsStatesHistogram() {
        mockCookieControlsMode(
                CookieControlsMode.BLOCK_THIRD_PARTY, CookieControlsMode.INCOGNITO_ONLY);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCookies_block3PTo3PSettingsStatesHistogram() {
        mockCookieControlsMode(
                CookieControlsMode.BLOCK_THIRD_PARTY, CookieControlsMode.BLOCK_THIRD_PARTY);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.BLOCK3P_TO3P);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        watcher.assertExpected();
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
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.COOKIES_NEXT_BUTTON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.COOKIES);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testSearchSuggestions_offToOffSettingsStatesHistogram() {
        mockSearchSuggestionsState(false, false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_OFF_TO_OFF);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SEARCH_SUGGESTIONS);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testSearchSuggestions_offToOnSettingsStatesHistogram() {
        mockSearchSuggestionsState(false, true);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_OFF_TO_ON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SEARCH_SUGGESTIONS);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testSearchSuggestions_onToOffSettingsStatesHistogram() {
        mockSearchSuggestionsState(true, false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_ON_TO_OFF);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SEARCH_SUGGESTIONS);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testSearchSuggestions_onToOnSettingsStatesHistogram() {
        mockSearchSuggestionsState(true, true);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM,
                        PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_ON_TO_ON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SEARCH_SUGGESTIONS);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testSearchSuggestions_nextClickUserAction() {
        mockSearchSuggestionsState(false, false);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SEARCH_SUGGESTIONS);
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.NextClickSearchSuggestions"));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testSearchSuggestions_nextNavigationHistogram() {
        mockSearchSuggestionsState(false, false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM,
                        PrivacyGuideInteractions.SEARCH_SUGGESTIONS_NEXT_BUTTON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.SEARCH_SUGGESTIONS);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testWelcome_nextClickUserAction() {
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.WELCOME);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickWelcome"));
    }

    @Test
    @SmallTest
    public void testWelcome_nextNavigationHistogram() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.WELCOME_NEXT_BUTTON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.WELCOME);
        watcher.assertExpected();
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
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testSearchSuggestions_changeSearchSuggestionsOnUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnSearchSuggestionsChange(true);
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.ChangeSearchSuggestionsOn"));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testSearchSuggestions_changeSearchSuggestionsOffUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnSearchSuggestionsChange(false);
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.ChangeSearchSuggestionsOff"));
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
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.ChangeSafeBrowsingEnhanced"));
    }

    @Test
    @SmallTest
    public void testSafeBrowsing_changeSafeBrowsingStandardUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnSafeBrowsingChange(
                SafeBrowsingState.STANDARD_PROTECTION);
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.ChangeSafeBrowsingStandard"));
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
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito"));
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

    @Test
    public void testDone_backClickUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                PrivacyGuideFragment.FragmentType.DONE);
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickCompletion"));
    }

    @Test
    public void testMSBB_backClickUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                PrivacyGuideFragment.FragmentType.MSBB);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickMSBB"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE_ANDROID_3)
    public void testSearchSuggestions_backClickUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                PrivacyGuideFragment.FragmentType.SEARCH_SUGGESTIONS);
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.BackClickSearchSuggestions"));
    }

    @Test(expected = AssertionError.class)
    public void testWelcome_backClickUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                PrivacyGuideFragment.FragmentType.WELCOME);
    }

    @Test
    @SmallTest
    public void testAdTopics_offToOffSettingsStatesHistogram() {
        mockAdTopicsState(false, false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.AD_TOPICS_OFF_TO_OFF);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.AD_TOPICS);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testAdTopics_offToOnSettingsStatesHistogram() {
        mockAdTopicsState(false, true);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.AD_TOPICS_OFF_TO_ON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.AD_TOPICS);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testAdTopics_onToOffSettingsStatesHistogram() {
        mockAdTopicsState(true, false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.AD_TOPICS_ON_TO_OFF);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.AD_TOPICS);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testAdTopics_onToOnSettingsStatesHistogram() {
        mockAdTopicsState(true, true);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SETTINGS_STATES_HISTOGRAM, PrivacyGuideSettingsStates.AD_TOPICS_ON_TO_ON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.AD_TOPICS);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testAdTopics_nextNavigationHistogram() {
        mockAdTopicsState(false, false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NEXT_NAVIGATION_HISTOGRAM, PrivacyGuideInteractions.AD_TOPICS_NEXT_BUTTON);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.AD_TOPICS);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testAdTopics_changeAdTopicsOnUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnAdTopicsChange(true);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeAdTopicsOn"));
    }

    @Test
    @SmallTest
    public void testAdTopics_changeAdTopicsOffUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnAdTopicsChange(false);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeAdTopicsOff"));
    }

    @Test
    @SmallTest
    public void testAdTopics_nextClickUserAction() {
        mockAdTopicsState(false, false);
        triggerMetricsOnNext(PrivacyGuideFragment.FragmentType.AD_TOPICS);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.NextClickAdTopics"));
    }

    @Test
    public void testAdTopics_backClickUserAction() {
        PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                PrivacyGuideFragment.FragmentType.AD_TOPICS);
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.BackClickAdTopics"));
    }
}
