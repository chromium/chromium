// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.preference.PreferenceFragmentCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;

/** Tests of {@link IncognitoReauthSettingSwitchPreference}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class IncognitoReauthSettingSwitchPreferenceTest {
    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "This is a summary.";

    @Rule
    public SettingsActivityTestRule<PlaceholderSettingsForTest> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PlaceholderSettingsForTest.class);

    private IncognitoReauthSettingSwitchPreference mPreference;
    private Context mContext;

    @Mock private Runnable mLinkClickDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mSettingsActivityTestRule.startSettingsActivity();
        PreferenceFragmentCompat fragment =
                (PreferenceFragmentCompat)
                        mSettingsActivityTestRule.getActivity().getMainFragment();
        mContext = fragment.getPreferenceManager().getContext();

        mPreference = new IncognitoReauthSettingSwitchPreference(mContext);
        mPreference.setTitle(TITLE);
        mPreference.setSummary(SUMMARY);

        mPreference.setLinkClickDelegate(mLinkClickDelegate);
        fragment.getPreferenceScreen().addPreference(mPreference);
    }

    @Test
    @SmallTest
    public void testChromeSwitchPreferenceWithClickableSummary_OnSummaryClick_Interactable() {
        mPreference.setPreferenceInteractable(true);

        onView(withId(android.R.id.summary)).perform(click());
        verify(mLinkClickDelegate, never()).run();
    }

    @Test
    @SmallTest
    public void testChromeSwitchPreferenceWithClickableSummary_OnSummaryClick_NonInteractable() {
        mPreference.setPreferenceInteractable(false);

        onView(withId(android.R.id.summary)).perform(click());
        verify(mLinkClickDelegate).run();
    }

    @Test
    @SmallTest
    public void testChromeSwitchPreferenceWithClickableSummary_OnPreferenceClick() {
        mPreference.setPreferenceInteractable(true);
        assertFalse(mPreference.isChecked());

        onView(withId(android.R.id.title)).perform(click());

        // Toggling the preference shouldn't invoke the click defined for summary text.
        verify(mLinkClickDelegate, never()).run();
        assertTrue(mPreference.isChecked());
    }
}
