// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import androidx.preference.PreferenceFragmentCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;

import java.util.concurrent.TimeoutException;

/** Tests of {@link IncognitoReauthSettingSwitchPreference}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
public class IncognitoReauthSettingSwitchPreferenceTest {
    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "This is a summary.";

    @Rule
    public final SettingsTestRule<PlaceholderSettingsForTest> mSettingsTestRule =
            new SettingsTestRule<>(PlaceholderSettingsForTest.class);

    private final CallbackHelper mLinkClickDelegateHelper = new CallbackHelper();

    private IncognitoReauthSettingSwitchPreference mPreference;
    private Context mContext;

    @Before
    public void setUp() {
        mSettingsTestRule.startSettingsActivity();
        PreferenceFragmentCompat fragment = mSettingsTestRule.getFragment();
        mContext = fragment.getPreferenceManager().getContext();

        mPreference = new IncognitoReauthSettingSwitchPreference(mContext);
        mPreference.setTitle(TITLE);
        mPreference.setSummary(SUMMARY);

        mPreference.setLinkClickDelegate(mLinkClickDelegateHelper::notifyCalled);
        fragment.getPreferenceScreen().addPreference(mPreference);
    }

    @Test
    @SmallTest
    public void testChromeSwitchPreferenceWithClickableSummary_OnSummaryClick_Interactable() {
        mPreference.setPreferenceInteractable(true);

        onView(withId(android.R.id.summary)).perform(click());
        assertEquals(0, mLinkClickDelegateHelper.getCallCount());
    }

    @Test
    @SmallTest
    public void testChromeSwitchPreferenceWithClickableSummary_OnSummaryClick_NonInteractable()
            throws TimeoutException {
        mPreference.setPreferenceInteractable(false);

        onView(withId(android.R.id.summary)).perform(click());
        mLinkClickDelegateHelper.waitForOnly();
    }

    @Test
    @SmallTest
    public void testChromeSwitchPreferenceWithClickableSummary_OnPreferenceClick() {
        mPreference.setPreferenceInteractable(true);
        assertFalse(mPreference.isChecked());

        onView(withId(android.R.id.title)).perform(click());

        // Toggling the preference shouldn't invoke the click defined for summary text.
        assertEquals(0, mLinkClickDelegateHelper.getCallCount());
        assertTrue(mPreference.isChecked());
    }
}
