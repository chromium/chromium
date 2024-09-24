// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/**
 * JUnit tests of the class {@link StepDisplayHandlerImpl}. This test suite can be significantly
 * compressed if @ParameterizedTest from JUnit5 can be used.
 */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
public class StepDisplayHandlerImplTest {
    @Rule public JniMocker mMocker = new JniMocker();
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private SafeBrowsingBridge.Natives mSBNativesMock;
    @Mock private SyncService mSyncService;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PrefService mPrefServiceMock;
    @Mock private UserPrefs.Natives mUserPrefsNativesMock;
    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceNativesMock;
    @Mock private PrivacySandboxBridgeJni mPrivacySandboxBridgeJni;

    private StepDisplayHandler mStepDisplayHandler;

    @Before
    public void setUp() {
        mMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNativesMock);
        when(mUserPrefsNativesMock.get(mProfile)).thenReturn(mPrefServiceMock);

        mMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceNativesMock);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

        SyncServiceFactory.setInstanceForTesting(mSyncService);
        mMocker.mock(SafeBrowsingBridgeJni.TEST_HOOKS, mSBNativesMock);
        mMocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mPrivacySandboxBridgeJni);

        mStepDisplayHandler = new StepDisplayHandlerImpl(mProfile);
    }

    private void setSBState(@SafeBrowsingState int sbState) {
        when(mSBNativesMock.getSafeBrowsingState(eq(mProfile))).thenReturn(sbState);
    }

    private void setSyncState(boolean enabled) {
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(enabled);
    }

    private void setCookieState(@CookieControlsMode int cookieControlsMode, boolean allowCookies) {
        when(mPrefServiceMock.getInteger(PrefNames.COOKIE_CONTROLS_MODE))
                .thenReturn(cookieControlsMode);
        when(mWebsitePreferenceNativesMock.isContentSettingEnabled(
                        mProfile, ContentSettingsType.COOKIES))
                .thenReturn(allowCookies);
    }

    @Test
    public void testDisplaySBStepWhenSBEnhanced() {
        setSBState(SafeBrowsingState.ENHANCED_PROTECTION);
        assertTrue(mStepDisplayHandler.shouldDisplaySafeBrowsing());
    }

    @Test
    public void testDisplaySBWhenSBStandard() {
        setSBState(SafeBrowsingState.STANDARD_PROTECTION);
        assertTrue(mStepDisplayHandler.shouldDisplaySafeBrowsing());
    }

    @Test
    public void testDontDisplaySBWhenSBUnsafe() {
        setSBState(SafeBrowsingState.NO_SAFE_BROWSING);
        assertFalse(mStepDisplayHandler.shouldDisplaySafeBrowsing());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testDisplayHistorySyncWhenSyncOn() {
        setSyncState(true);
        assertTrue(mStepDisplayHandler.shouldDisplayHistorySync());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testDontDisplayHistorySyncWhenSyncOff() {
        setSyncState(false);
        assertFalse(mStepDisplayHandler.shouldDisplayHistorySync());
    }

    @Test
    public void testDontDisplayHistorySyncWhenNotSignedIn() {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        assertFalse(mStepDisplayHandler.shouldDisplayHistorySync());
    }

    @Test
    public void testDontDisplayHistorySyncWhenSyncDisabledByEnterprisePolicy() {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        when(mSyncService.isSyncDisabledByEnterprisePolicy()).thenReturn(true);
        assertFalse(mStepDisplayHandler.shouldDisplayHistorySync());
    }

    @Test
    public void testDontDisplayHistorySyncWhenHistoryAndTabsSyncIsManagedByPolicy() {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        when(mSyncService.isSyncDisabledByEnterprisePolicy()).thenReturn(false);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.HISTORY)).thenReturn(true);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.TABS)).thenReturn(true);
        assertFalse(mStepDisplayHandler.shouldDisplayHistorySync());
    }

    @Test
    public void testDisplayHistorySyncWhenOnlyHistorySyncIsManagedByPolicy() {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        when(mSyncService.isSyncDisabledByEnterprisePolicy()).thenReturn(false);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.HISTORY)).thenReturn(true);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.TABS)).thenReturn(false);
        when(mSyncService.isTypeManagedByCustodian(UserSelectableType.HISTORY)).thenReturn(false);
        when(mSyncService.isTypeManagedByCustodian(UserSelectableType.TABS)).thenReturn(false);
        assertTrue(mStepDisplayHandler.shouldDisplayHistorySync());
    }

    @Test
    public void testDontDisplayHistorySyncWhenHistoryAndTabsSyncIsManagedByCustodian() {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        when(mSyncService.isSyncDisabledByEnterprisePolicy()).thenReturn(false);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.HISTORY)).thenReturn(false);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.TABS)).thenReturn(false);
        when(mSyncService.isTypeManagedByCustodian(UserSelectableType.HISTORY)).thenReturn(true);
        when(mSyncService.isTypeManagedByCustodian(UserSelectableType.TABS)).thenReturn(true);
        assertFalse(mStepDisplayHandler.shouldDisplayHistorySync());
    }

    @Test
    public void testDisplayHistorySyncWhenOnlyHistorySyncIsManagedByCustodian() {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        when(mSyncService.isSyncDisabledByEnterprisePolicy()).thenReturn(false);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.HISTORY)).thenReturn(false);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.TABS)).thenReturn(false);
        when(mSyncService.isTypeManagedByCustodian(UserSelectableType.HISTORY)).thenReturn(true);
        when(mSyncService.isTypeManagedByCustodian(UserSelectableType.TABS)).thenReturn(false);
        assertTrue(mStepDisplayHandler.shouldDisplayHistorySync());
    }

    @Test
    public void testDontDisplayCookiesWhenCookiesAllAllowed() {
        setCookieState(CookieControlsMode.OFF, true);
        assertFalse(mStepDisplayHandler.shouldDisplayCookies());
    }

    @Test
    public void testDisplayCookiesWhenCookiesThirdPartyBlocked() {
        setCookieState(CookieControlsMode.BLOCK_THIRD_PARTY, true);
        assertTrue(mStepDisplayHandler.shouldDisplayCookies());
    }

    @Test
    public void testDisplayCookiesWhenCookiesIncognitoBlocked() {
        setCookieState(CookieControlsMode.INCOGNITO_ONLY, true);
        assertTrue(mStepDisplayHandler.shouldDisplayCookies());
    }

    @Test
    public void testDontDisplayCookiesWhenCookiesAllBlocked() {
        setCookieState(CookieControlsMode.BLOCK_THIRD_PARTY, false);
        assertFalse(mStepDisplayHandler.shouldDisplayCookies());
    }

    @Test
    public void testDisplayAdTopicsWhenShouldShowAdTopicsIsOn() {
        when(mPrivacySandboxBridgeJni.privacySandboxPrivacyGuideShouldShowAdTopicsCard(any()))
                .thenReturn(true);
        assertTrue(mStepDisplayHandler.shouldDisplayAdTopics());
    }

    @Test
    public void testDontDisplayAdTopicsWhenShouldShowAdTopicsIsOff() {
        when(mPrivacySandboxBridgeJni.privacySandboxPrivacyGuideShouldShowAdTopicsCard(any()))
                .thenReturn(false);
        assertFalse(mStepDisplayHandler.shouldDisplayAdTopics());
    }
}
