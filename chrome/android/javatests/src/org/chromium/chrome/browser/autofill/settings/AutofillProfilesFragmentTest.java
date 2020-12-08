// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.view.KeyEvent;
import android.widget.EditText;

import androidx.preference.PreferenceFragmentCompat;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Unit test suite for AutofillProfilesFragment.
 */

@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillProfilesFragmentTest {
    @Rule
    public final AutofillTestRule rule = new AutofillTestRule();
    @Rule
    public final SettingsActivityTestRule<AutofillProfilesFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillProfilesFragment.class);
    @Rule
    public final TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "" /* honorific prefix */, "Seb Doe", "Google", "111 First St", "CA", "Los Angeles",
                "", "90291", "", "US", "650-253-0000", "first@gmail.com", "en-US"));
        helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "" /* honorific prefix */, "John Doe", "Google", "111 Second St", "CA",
                "Los Angeles", "", "90291", "", "US", "650-253-0000", "second@gmail.com", "en-US"));
        // Invalid state should not cause a crash on the state dropdown list.
        helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "" /* honorific prefix */, "Bill Doe", "Google", "111 Third St", "XXXYYY",
                "Los Angeles", "", "90291", "", "US", "650-253-0000", "third@gmail.com", "en-US"));
        // Full value for state should show up correctly on the dropdown list.
        helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "" /* honorific prefix */, "Bob Doe", "Google", "111 Fourth St", "California",
                "Los Angeles", "", "90291", "", "US", "650-253-0000", "fourth@gmail.com", "en-US"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.
    EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UI_FOR_HONORIFIC_PREFIXES_IN_SETTINGS})
    public void testAddProfile() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillProfilesFragment autofillProfileFragment =
                (AutofillProfilesFragment) activity.getMainFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        PreferenceFragmentCompat fragment = (PreferenceFragmentCompat) activity.getMainFragment();
        AutofillProfileEditorPreference addProfile =
                (AutofillProfileEditorPreference) fragment.findPreference(
                        AutofillProfilesFragment.PREF_NEW_PROFILE);
        Assert.assertNotNull(addProfile);

        // Add a profile.
        updatePreferencesAndWait(autofillProfileFragment, addProfile,
                new String[] {"Ms.", "Alice Doe", "Google", "111 Added St", "Los Angeles", "CA",
                        "90291", "650-253-0000", "add@profile.com"},
                R.id.editor_dialog_done_button, false);

        Assert.assertEquals(7 /* One toggle + one add button + five profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference addedProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("Alice Doe");
        Assert.assertNotNull(addedProfile);
        Assert.assertEquals("111 Added St, 90291", addedProfile.getSummary());
        activity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.
    EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UI_FOR_HONORIFIC_PREFIXES_IN_SETTINGS})
    public void testAddIncompletedProfile() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillProfilesFragment autofillProfileFragment =
                (AutofillProfilesFragment) activity.getMainFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        PreferenceFragmentCompat fragment = (PreferenceFragmentCompat) activity.getMainFragment();
        AutofillProfileEditorPreference addProfile =
                (AutofillProfileEditorPreference) fragment.findPreference(
                        AutofillProfilesFragment.PREF_NEW_PROFILE);
        Assert.assertNotNull(addProfile);

        // Add an incomplete profile.
        updatePreferencesAndWait(autofillProfileFragment, addProfile, new String[] {"", "Mike Doe"},
                R.id.editor_dialog_done_button, false);

        // Incomplete profile should still be added.
        Assert.assertEquals(7 /* One toggle + one add button + five profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference addedProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("Mike Doe");
        Assert.assertNotNull(addedProfile);
        activity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.
    EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UI_FOR_HONORIFIC_PREFIXES_IN_SETTINGS})
    public void testAddProfileWithInvalidPhone() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillProfilesFragment autofillProfileFragment =
                (AutofillProfilesFragment) activity.getMainFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        PreferenceFragmentCompat fragment = (PreferenceFragmentCompat) activity.getMainFragment();
        AutofillProfileEditorPreference addProfile =
                (AutofillProfileEditorPreference) fragment.findPreference(
                        AutofillProfilesFragment.PREF_NEW_PROFILE);
        Assert.assertNotNull(addProfile);

        // Try to add a profile with invalid phone.
        updatePreferencesAndWait(autofillProfileFragment, addProfile,
                new String[] {"", "", "", "", "", "", "", "123"}, R.id.editor_dialog_done_button,
                true);
        activity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDeleteProfile() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillProfilesFragment autofillProfileFragment =
                (AutofillProfilesFragment) activity.getMainFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        PreferenceFragmentCompat fragment = (PreferenceFragmentCompat) activity.getMainFragment();
        AutofillProfileEditorPreference sebProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("Seb Doe");
        Assert.assertNotNull(sebProfile);
        Assert.assertEquals("Seb Doe", sebProfile.getTitle());

        // Delete a profile.
        updatePreferencesAndWait(
                autofillProfileFragment, sebProfile, null, R.id.delete_menu_id, false);

        Assert.assertEquals(5 /* One toggle + one add button + three profile. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference remainedProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("John Doe");
        Assert.assertNotNull(remainedProfile);
        AutofillProfileEditorPreference deletedProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("Seb Doe");
        Assert.assertNull(deletedProfile);
        activity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.
    EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UI_FOR_HONORIFIC_PREFIXES_IN_SETTINGS})
    public void testEditProfile() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillProfilesFragment autofillProfileFragment =
                (AutofillProfilesFragment) activity.getMainFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        PreferenceFragmentCompat fragment = (PreferenceFragmentCompat) activity.getMainFragment();
        AutofillProfileEditorPreference johnProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("John Doe");
        Assert.assertNotNull(johnProfile);
        Assert.assertEquals("John Doe", johnProfile.getTitle());

        // Edit a profile.
        updatePreferencesAndWait(autofillProfileFragment, johnProfile,
                new String[] {"Dr.", "Emily Doe", "Google", "111 Edited St", "Los Angeles", "CA",
                        "90291", "650-253-0000", "edit@profile.com"},
                R.id.editor_dialog_done_button, false);
        // Check if the preferences are updated correctly.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference editedProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("Emily Doe");
        Assert.assertNotNull(editedProfile);
        Assert.assertEquals("111 Edited St, 90291", editedProfile.getSummary());
        AutofillProfileEditorPreference oldProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("John Doe");
        Assert.assertNull(oldProfile);
        activity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testOpenProfileWithCompleteState() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillProfilesFragment autofillProfileFragment =
                (AutofillProfilesFragment) activity.getMainFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        PreferenceFragmentCompat fragment = (PreferenceFragmentCompat) activity.getMainFragment();
        AutofillProfileEditorPreference bobProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("Bob Doe");
        Assert.assertNotNull(bobProfile);
        Assert.assertEquals("Bob Doe", bobProfile.getTitle());

        // Open the profile.
        updatePreferencesAndWait(
                autofillProfileFragment, bobProfile, null, R.id.editor_dialog_done_button, false);

        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        activity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testOpenProfileWithInvalidState() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        PreferenceFragmentCompat fragment = (PreferenceFragmentCompat) activity.getMainFragment();
        AutofillProfileEditorPreference billProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("Bill Doe");
        Assert.assertNotNull(billProfile);
        Assert.assertEquals("Bill Doe", billProfile.getTitle());

        // Open the profile.
        updatePreferencesAndWait(
                autofillProfileFragment, billProfile, null, R.id.editor_dialog_done_button, false);

        // Check if the preferences are updated correctly.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testKeyboardShownOnDpadCenter() throws TimeoutException {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        AutofillProfilesFragment fragment = (AutofillProfilesFragment) activity.getMainFragment();
        AutofillProfileEditorPreference addProfile =
                (AutofillProfileEditorPreference) fragment.findPreference(
                        AutofillProfilesFragment.PREF_NEW_PROFILE);
        Assert.assertNotNull(addProfile);

        // Open AutofillProfileEditorPreference.
        TestThreadUtils.runOnUiThreadBlocking(addProfile::performClick);
        rule.setEditorDialogAndWait(fragment.getEditorDialogForTest());
        // The keyboard is shown as soon as AutofillProfileEditorPreference comes into view.
        waitForKeyboardStatus(true, activity);

        final List<EditText> fields =
                fragment.getEditorDialogForTest().getEditableTextFieldsForTest();
        // Ensure the first text field is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> { fields.get(0).requestFocus(); });
        // Hide the keyboard.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            KeyboardVisibilityDelegate.getInstance().hideKeyboard(fields.get(0));
        });
        // Check that the keyboard is hidden.
        waitForKeyboardStatus(false, activity);

        // Send a d-pad key event to one of the text fields
        try {
            rule.sendKeycodeToTextFieldInEditorAndWait(KeyEvent.KEYCODE_DPAD_CENTER, 0);
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        // Check that the keyboard was shown.
        waitForKeyboardStatus(true, activity);
        activity.finish();
    }

    private void waitForKeyboardStatus(
            final boolean keyboardVisible, final SettingsActivity activity) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                                       activity, activity.findViewById(android.R.id.content)),
                    Matchers.is(keyboardVisible));
        });
    }

    private void updatePreferencesAndWait(AutofillProfilesFragment profileFragment,
            AutofillProfileEditorPreference profile, String[] values, int buttonId,
            boolean waitForError) throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(profile::performClick);

        rule.setEditorDialogAndWait(profileFragment.getEditorDialogForTest());
        if (values != null) rule.setTextInEditorAndWait(values);
        if (waitForError) {
            rule.clickInEditorAndWaitForValidationError(buttonId);
        } else {
            rule.clickInEditorAndWait(buttonId);
            rule.waitForThePreferenceUpdate();
        }
    }
}
