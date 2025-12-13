// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.preference.PreferenceScreen;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;
import org.chromium.components.browser_ui.site_settings.TwoActionSwitchPreference;

/** Tests for {@link TwoActionSwitchPreference}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TwoActionSwitchPreferenceTest {
    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "Preference Summary.";

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private TwoActionSwitchPreference mPreference;

    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        PreferenceScreen preferenceScreen = mSettingsRule.getPreferenceScreen();

        mPreference = new TwoActionSwitchPreference(mSettingsRule.getActivity());
        mPreference.setTitle(TITLE);
        mPreference.setSummary(SUMMARY);
        mPreference.setChecked(false);
        preferenceScreen.addPreference(mPreference);
    }

    @Test
    @SmallTest
    public void testPrimaryButtonClick() {
        CallbackHelper callback = new CallbackHelper();
        mPreference.setPrimaryButtonClickListener((v) -> callback.notifyCalled());

        assertFalse(mPreference.isChecked());
        onView(withText(TITLE)).perform(click());
        assertEquals(1, callback.getCallCount());

        // Verify that clicking on the primary button area does not flip the switch.
        assertFalse(mPreference.isChecked());
    }

    @Test
    @SmallTest
    public void testSwitchClick() {
        CallbackHelper callback = new CallbackHelper();
        mPreference.setPrimaryButtonClickListener((v) -> callback.notifyCalled());

        assertFalse(mPreference.isChecked());
        onView(withId(R.id.switch_container)).perform(click());
        assertTrue(mPreference.isChecked());

        // Verify that clicking on the switch does not trigger the primary button click listener.
        assertEquals(0, callback.getCallCount());
    }
}
