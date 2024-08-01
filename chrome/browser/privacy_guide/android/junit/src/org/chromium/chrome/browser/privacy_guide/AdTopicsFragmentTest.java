// Copyright 2024 The Chromium Authors
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
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** JUnit tests of the class {@link AdTopicsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AdTopicsFragmentTest {
    @Rule public JniMocker mocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefService;

    private FragmentScenario mScenario;
    private MaterialSwitchWithText mAdTopicsButton;
    private final UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setUp() {
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
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
                                }
                                return fragment;
                            }
                        });
        mScenario.onFragment(
                fragment ->
                        mAdTopicsButton = fragment.getView().findViewById(R.id.ad_topics_switch));
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
}
