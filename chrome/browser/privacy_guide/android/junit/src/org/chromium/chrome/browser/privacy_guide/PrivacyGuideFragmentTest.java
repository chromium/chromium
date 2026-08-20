// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.MenuItem;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.widget.ButtonCompat;

/** JUnit tests for {@link PrivacyGuideFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PrivacyGuideFragmentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private SafeBrowsingBridge.Natives mSafeBrowsingNatives;
    @Mock private SyncService mSyncService;
    @Mock private HistorySyncHelper mHistorySyncHelper;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsNatives;
    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceNatives;

    private FragmentScenario<PrivacyGuideFragment> mScenario;
    private PrivacyGuideFragment mFragment;

    @Before
    public void setUp() {
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        UserPrefsJni.setInstanceForTesting(mUserPrefsNatives);
        Mockito.lenient().when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);

        WebsitePreferenceBridgeJni.setInstanceForTesting(mWebsitePreferenceNatives);
        Mockito.lenient()
                .when(
                        mWebsitePreferenceNatives.isContentSettingEnabled(
                                mProfile, ContentSettingsType.COOKIES))
                .thenReturn(true);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        Mockito.lenient()
                .when(mIdentityServicesProvider.getIdentityManager(mProfile))
                .thenReturn(mIdentityManager);

        SyncServiceFactory.setInstanceForTesting(mSyncService);
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);

        SafeBrowsingBridgeJni.setInstanceForTesting(mSafeBrowsingNatives);
        Mockito.lenient()
                .when(mSafeBrowsingNatives.getSafeBrowsingState(mProfile))
                .thenReturn(SafeBrowsingState.STANDARD_PROTECTION);

        var fragmentFactory =
                new FragmentFactory() {
                    @Override
                    public Fragment instantiate(ClassLoader classLoader, String className) {
                        Fragment fragment = super.instantiate(classLoader, className);
                        if (fragment instanceof PrivacyGuideFragment privacyGuide) {
                            privacyGuide.setProfile(mProfile);
                        }
                        return fragment;
                    }
                };
        mScenario =
                FragmentScenario.launchInContainer(
                        PrivacyGuideFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_MaterialComponents,
                        fragmentFactory);
        mScenario.onFragment(fragment -> mFragment = (PrivacyGuideFragment) fragment);
    }

    @After
    public void tearDown() {
        mScenario.close();
    }

    @Test
    public void testDoneButtonClicked_finishesCurrentSettings() {
        ButtonCompat doneButton = mFragment.requireView().findViewById(R.id.done_button);
        doneButton.performClick();

        verify(mSettingsNavigation).finishCurrentSettings(mFragment);
    }

    @Test
    public void testCloseMenuItemClicked_finishesCurrentSettings() {
        MenuItem closeItem = Mockito.mock(MenuItem.class);
        when(closeItem.getItemId()).thenReturn(R.id.close_menu_id);

        assertTrue(mFragment.onOptionsItemSelected(closeItem));
        verify(mSettingsNavigation).finishCurrentSettings(mFragment);
    }

    @Test
    public void testHomeMenuItemClicked_withMultiColumn_finishesCurrentSettings() {
        MenuItem homeItem = Mockito.mock(MenuItem.class);
        when(homeItem.getItemId()).thenReturn(android.R.id.home);

        assertTrue(mFragment.onOptionsItemSelected(homeItem));
        verify(mSettingsNavigation).finishCurrentSettings(mFragment);
    }
}
