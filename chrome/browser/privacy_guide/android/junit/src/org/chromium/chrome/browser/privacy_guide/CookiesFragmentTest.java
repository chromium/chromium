// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** Robolectric tests of the class {@link CookiesFragment} */
@RunWith(BaseRobolectricTestRunner.class)
public class CookiesFragmentTest {
    // TODO(crbug.com/40860773): Use Espresso for view interactions
    @Rule public JniMocker mMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private PrefService mPrefServiceMock;
    @Mock private UserPrefs.Natives mUserPrefsNativesMock;
    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceNativesMock;

    private FragmentScenario mScenario;
    private RadioButtonWithDescription mBlockThirdPartyIncognito;
    private RadioButtonWithDescription mBlockThirdParty;
    private final UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setUp() {
        mMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNativesMock);
        when(mUserPrefsNativesMock.get(mProfile)).thenReturn(mPrefServiceMock);

        mMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceNativesMock);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
        mActionTester.tearDown();
    }

    public void initFragmentWithCookiesState(
            @CookieControlsMode int cookieControlsMode, boolean allowCookies) {
        when(mPrefServiceMock.getInteger(PrefNames.COOKIE_CONTROLS_MODE))
                .thenReturn(cookieControlsMode);
        when(mWebsitePreferenceNativesMock.isContentSettingEnabled(
                        mProfile, ContentSettingsType.COOKIES))
                .thenReturn(allowCookies);

        mScenario =
                FragmentScenario.launchInContainer(
                        CookiesFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_MaterialComponents,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof CookiesFragment) {
                                    ((CookiesFragment) fragment).setProfile(mProfile);
                                }
                                return fragment;
                            }
                        });
        mScenario.onFragment(
                fragment -> {
                    mBlockThirdPartyIncognito =
                            fragment.getView().findViewById(R.id.block_third_party_incognito);
                    mBlockThirdParty = fragment.getView().findViewById(R.id.block_third_party);
                });
    }

    @Test(expected = AssertionError.class)
    public void testInitWhenCookiesAllowed() {
        initFragmentWithCookiesState(CookieControlsMode.OFF, true);
    }

    @Test
    public void testInitWhenBlockThirdPartyIncognito() {
        initFragmentWithCookiesState(CookieControlsMode.INCOGNITO_ONLY, true);
        assertTrue(mBlockThirdPartyIncognito.isChecked());
        assertFalse(mBlockThirdParty.isChecked());
    }

    @Test
    public void testInitWhenBlockThirdPartyAlways() {
        initFragmentWithCookiesState(CookieControlsMode.BLOCK_THIRD_PARTY, true);
        assertFalse(mBlockThirdPartyIncognito.isChecked());
        assertTrue(mBlockThirdParty.isChecked());
    }

    @Test(expected = AssertionError.class)
    public void testInitWhenBlockAllCookies() {
        initFragmentWithCookiesState(CookieControlsMode.BLOCK_THIRD_PARTY, false);
    }

    @Test
    public void testSelectBlockThirdPartyIncognito() {
        initFragmentWithCookiesState(CookieControlsMode.BLOCK_THIRD_PARTY, true);
        mBlockThirdPartyIncognito.performClick();
        verify(mPrefServiceMock)
                .setInteger(PrefNames.COOKIE_CONTROLS_MODE, CookieControlsMode.INCOGNITO_ONLY);
        verify(mWebsitePreferenceNativesMock)
                .setContentSettingEnabled(mProfile, ContentSettingsType.COOKIES, true);
    }

    @Test
    public void testSelectBlockThirdPartyAlways() {
        initFragmentWithCookiesState(CookieControlsMode.INCOGNITO_ONLY, true);
        mBlockThirdParty.performClick();
        verify(mPrefServiceMock)
                .setInteger(PrefNames.COOKIE_CONTROLS_MODE, CookieControlsMode.BLOCK_THIRD_PARTY);
        verify(mWebsitePreferenceNativesMock)
                .setContentSettingEnabled(mProfile, ContentSettingsType.COOKIES, true);
    }

    @Test
    public void testSelectBlockThirdPartyIncognito_changeCookiesBlock3PIncognitoUserAction() {
        initFragmentWithCookiesState(CookieControlsMode.BLOCK_THIRD_PARTY, true);
        mBlockThirdPartyIncognito.performClick();
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito"));
    }

    @Test
    public void testSelectBlockThirdPartyAlways_changeCookiesBlock3PUserAction() {
        initFragmentWithCookiesState(CookieControlsMode.INCOGNITO_ONLY, true);
        mBlockThirdParty.performClick();
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeCookiesBlock3P"));
    }
}
