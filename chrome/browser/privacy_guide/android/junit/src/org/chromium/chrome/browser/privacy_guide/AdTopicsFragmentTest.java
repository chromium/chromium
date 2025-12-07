// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;

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
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.FakePrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** JUnit tests of the class {@link AdTopicsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AdTopicsFragmentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefService;
    @Mock private SettingsCustomTabLauncher mCustomTabLauncher;

    private FragmentScenario mScenario;
    private Fragment mFragment;
    private MaterialSwitchWithText mAdTopicsButton;
    private final UserActionTester mActionTester = new UserActionTester();
    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;

    @Before
    public void setUp() {
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        PrivacySandboxBridgeJni.setInstanceForTesting(mFakePrivacySandboxBridge);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
        mActionTester.tearDown();
    }

    private void initFragmentWithAdTopicsState(boolean isAdTopicsOn) {
        when(mPrefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED))
                .thenReturn(isAdTopicsOn);
        mScenario =
                FragmentScenario.launchInContainer(
                        AdTopicsFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_MaterialComponents,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof AdTopicsFragment) {
                                    ((AdTopicsFragment) fragment).setProfile(mProfile);
                                    ((AdTopicsFragment) fragment)
                                            .setCustomTabLauncher(mCustomTabLauncher);
                                }
                                mFragment = fragment;
                                return fragment;
                            }
                        });
        mAdTopicsButton = mFragment.getView().findViewById(R.id.ad_topics_switch);
    }

    @Test
    public void testIsSwitchOffWhenAdTopicsOff() {
        initFragmentWithAdTopicsState(false);
        assertFalse(mAdTopicsButton.isChecked());
    }

    @Test
    public void testIsSwitchOffWhenAdTopicsOn() {
        initFragmentWithAdTopicsState(true);
        assertTrue(mAdTopicsButton.isChecked());
    }

    @Test
    public void testTurnAdTopicsOn() {
        initFragmentWithAdTopicsState(false);
        mAdTopicsButton.performClick();
        verify(mPrefService).setBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED, true);
    }

    @Test
    public void testTurnAdTopicsOff() {
        initFragmentWithAdTopicsState(true);
        mAdTopicsButton.performClick();
        verify(mPrefService).setBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED, false);
    }

    @Test
    public void testTurnAdTopicsOff_changeAdTopicsOffUserAction() {
        initFragmentWithAdTopicsState(true);
        mAdTopicsButton.performClick();
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeAdTopicsOff"));
    }

    @Test
    public void testTurnAdTopicsOn_changeAdTopicsOnUserAction() {
        initFragmentWithAdTopicsState(false);
        mAdTopicsButton.performClick();
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeAdTopicsOn"));
    }

    @Test
    public void testPrivacyPolicyLink() {
        initFragmentWithAdTopicsState(false);
        String disclaimerText =
                mFragment
                        .getResources()
                        .getString(
                                R.string
                                        .settings_privacy_guide_ad_topics_things_to_consider_bullet3_clank);
        String matcherText = disclaimerText.replaceAll("<link>|</link>", "");

        onView(withText(matcherText)).perform(scrollTo()).check(matches(isDisplayed()));
        onView(withText(matcherText)).perform(clickOnClickableSpan(0));
        assertEquals(
                1,
                mActionTester.getActionCount(
                        "Settings.PrivacyGuide.AdTopicsPrivacyPolicyLinkClicked"));
    }
}
