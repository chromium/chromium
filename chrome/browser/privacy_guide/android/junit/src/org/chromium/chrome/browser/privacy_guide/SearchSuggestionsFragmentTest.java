// Copyright 2023 The Chromium Authors
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

/** JUnit tests of the class {@link SearchSuggestionsFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchSuggestionsFragmentTest {
    @Rule public JniMocker mocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefService;

    private FragmentScenario mScenario;
    private MaterialSwitchWithText mSearchSuggestionsButton;
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

    private void initFragmentWithSearchSuggestionsState(boolean isSearchSuggestionsOn) {
        when(mPrefService.getBoolean(Pref.SEARCH_SUGGEST_ENABLED))
                .thenReturn(isSearchSuggestionsOn);
        mScenario =
                FragmentScenario.launchInContainer(
                        SearchSuggestionsFragment.class,
                        Bundle.EMPTY,
                        org.chromium.chrome.R.style.Theme_BrowserUI_DayNight,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof SearchSuggestionsFragment) {
                                    ((SearchSuggestionsFragment) fragment).setProfile(mProfile);
                                }
                                return fragment;
                            }
                        });
        mScenario.onFragment(
                fragment ->
                        mSearchSuggestionsButton =
                                fragment.getView().findViewById(R.id.search_suggestions_switch));
    }

    @Test
    public void testIsSwitchOffWhenSearchSuggestionsOff() {
        initFragmentWithSearchSuggestionsState(false);
        assertFalse(mSearchSuggestionsButton.isChecked());
    }

    @Test
    public void testIsSwitchOnWhenSearchSuggestionsOn() {
        initFragmentWithSearchSuggestionsState(true);
        assertTrue(mSearchSuggestionsButton.isChecked());
    }

    @Test
    public void testTurnSearchSuggestionsOn() {
        initFragmentWithSearchSuggestionsState(false);
        mSearchSuggestionsButton.performClick();
        verify(mPrefService).setBoolean(Pref.SEARCH_SUGGEST_ENABLED, true);
    }

    @Test
    public void testTurnSearchSuggestionsOff() {
        initFragmentWithSearchSuggestionsState(true);
        mSearchSuggestionsButton.performClick();
        verify(mPrefService).setBoolean(Pref.SEARCH_SUGGEST_ENABLED, false);
    }
}
