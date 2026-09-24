// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.MenuItem;

import androidx.activity.OnBackPressedDispatcher;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.viewpager2.widget.ViewPager2;

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
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;
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
    @Mock private UnifiedConsentServiceBridge.Natives mUnifiedConsentNatives;
    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceNatives;

    private FragmentScenario<PrivacyGuideFragment> mScenario;
    private PrivacyGuideFragment mFragment;

    @Before
    public void setUp() {
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        UserPrefsJni.setInstanceForTesting(mUserPrefsNatives);
        Mockito.lenient().when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);

        // Advancing a step records MSBB state, which reads through this bridge.
        UnifiedConsentServiceBridgeJni.setInstanceForTesting(mUnifiedConsentNatives);

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

    @Test
    public void testGetPageTitle_isPopulatedForEmbeddedHost() {
        // As an EmbeddableSettingsPage the fragment must publish its title through the supplier
        // rather than by mutating the activity title.
        assertEquals(
                mFragment.getString(R.string.privacy_guide_fragment_title),
                mFragment.getPageTitle().get());
    }

    @Test
    public void testBackPress_onFirstStep_isNotConsumed() {
        ViewPager2 viewPager = mFragment.requireView().findViewById(R.id.review_viewpager);
        assertEquals(0, viewPager.getCurrentItem());

        // On the first step the host owns back so that it can close the guide.
        assertFalse(mFragment.requireActivity().getOnBackPressedDispatcher().hasEnabledCallbacks());
    }

    @Test
    public void testBackPress_afterAdvancing_returnsToPreviousStep() {
        ViewPager2 viewPager = mFragment.requireView().findViewById(R.id.review_viewpager);
        mFragment.requireView().findViewById(R.id.start_button).performClick();

        int advancedIdx = viewPager.getCurrentItem();
        assertTrue("Expected to advance past the welcome step.", advancedIdx > 0);

        OnBackPressedDispatcher dispatcher =
                mFragment.requireActivity().getOnBackPressedDispatcher();
        assertTrue(dispatcher.hasEnabledCallbacks());
        dispatcher.onBackPressed();

        assertEquals(advancedIdx - 1, viewPager.getCurrentItem());
    }

    @Test
    public void testBackPress_backToFirstStep_releasesBackToHost() {
        ViewPager2 viewPager = mFragment.requireView().findViewById(R.id.review_viewpager);
        mFragment.requireView().findViewById(R.id.start_button).performClick();

        OnBackPressedDispatcher dispatcher =
                mFragment.requireActivity().getOnBackPressedDispatcher();
        dispatcher.onBackPressed();

        // Back on the first step, the fragment must stop intercepting so the host can close it.
        assertEquals(0, viewPager.getCurrentItem());
        assertFalse(dispatcher.hasEnabledCallbacks());
    }
}
