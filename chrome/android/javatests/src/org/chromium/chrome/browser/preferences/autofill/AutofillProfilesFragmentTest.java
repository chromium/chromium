// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.view.KeyEvent;
import android.widget.EditText;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesTest;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Unit test suite for AutofillProfilesFragment.
 */

@RunWith(BaseJUnit4ClassRunner.class)
public class AutofillProfilesFragmentTest {
    @Rule
    public final AutofillTestRule rule = new AutofillTestRule();

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Seb Doe", "Google",
                "111 First St", "CA", "Los Angeles", "", "90291", "", "US", "650-253-0000",
                "first@gmail.com", "en-US"));
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "John Doe", "Google",
                "111 Second St", "CA", "Los Angeles", "", "90291", "", "US", "650-253-0000",
                "second@gmail.com", "en-US"));
        // Invalid state should not cause a crash on the state dropdown list.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Bill Doe", "Google",
                "111 Third St", "XXXYYY", "Los Angeles", "", "90291", "", "US", "650-253-0000",
                "third@gmail.com", "en-US"));
        // Full value for state should show up correctly on the dropdown list.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Bob Doe", "Google",
                "111 Fourth St", "California", "Los Angeles", "", "90291", "", "US", "650-253-0000",
                "fourth@gmail.com", "en-US"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAddProfile() throws Exception {
        Preferences activity =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        AutofillProfilesFragment.class.getName());
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
        // TODO(jeffreycohen): Change this test into a parameterized test that exercises
        // both branches of this if statement.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_COMPANY_NAME)) {
            updatePreferencesAndWait(autofillProfileFragment, addProfile,
                    new String[] {"Alice Doe", "Google", "111 Added St", "Los Angeles", "CA",
                            "90291", "650-253-0000", "add@profile.com"},
                    R.id.editor_dialog_done_button, false);
        } else {
            updatePreferencesAndWait(autofillProfileFragment, addProfile,
                    new String[] {"Alice Doe", "111 Added St", "Los Angeles", "CA", "90291",
                            "650-253-0000", "add@profile.com"},
                    R.id.editor_dialog_done_button, false);
        }

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
    public void testAddIncompletedProfile() throws Exception {
        Preferences activity =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        AutofillProfilesFragment.class.getName());
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

        // Try to add an incomplete profile.
        updatePreferencesAndWait(autofillProfileFragment, addProfile,
                new String[] {"Mike Doe"}, R.id.editor_dialog_done_button, true);
        activity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDeleteProfile() throws Exception {
        Preferences activity =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        AutofillProfilesFragment.class.getName());
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
        updatePreferencesAndWait(autofillProfileFragment, sebProfile, null,
                R.id.delete_menu_id, false);

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
    public void testEditProfile() throws Exception {
        Preferences activity =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        AutofillProfilesFragment.class.getName());
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
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_COMPANY_NAME)) {
            updatePreferencesAndWait(autofillProfileFragment, johnProfile,
                    new String[] {"Emily Doe", "Google", "111 Edited St", "Los Angeles", "CA",
                            "90291", "650-253-0000", "edit@profile.com"},
                    R.id.editor_dialog_done_button, false);
        } else {
            updatePreferencesAndWait(autofillProfileFragment, johnProfile,
                    new String[] {"Emily Doe", "111 Edited St", "Los Angeles", "CA", "90291",
                            "650-253-0000", "edit@profile.com"},
                    R.id.editor_dialog_done_button, false);
        }
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
        Preferences activity =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        AutofillProfilesFragment.class.getName());
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
        updatePreferencesAndWait(autofillProfileFragment, bobProfile, null,
                R.id.editor_dialog_done_button, false);

        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        activity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testOpenProfileWithInvalidState() throws Exception {
        Preferences activity =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        AutofillProfilesFragment.class.getName());
        AutofillProfilesFragment autofillProfileFragment =
                (AutofillProfilesFragment) activity.getMainFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        PreferenceFragmentCompat fragment = (PreferenceFragmentCompat) activity.getMainFragment();
        AutofillProfileEditorPreference billProfile =
                (AutofillProfileEditorPreference) fragment.findPreference("Bill Doe");
        Assert.assertNotNull(billProfile);
        Assert.assertEquals("Bill Doe", billProfile.getTitle());

        // Open the profile.
        updatePreferencesAndWait(autofillProfileFragment, billProfile, null,
                R.id.editor_dialog_done_button, false);

        // Check if the preferences are updated correctly.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testKeyboardShownOnDpadCenter() throws TimeoutException {
        Preferences activity =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        AutofillProfilesFragment.class.getName());

        PreferenceFragmentCompat fragment = (PreferenceFragmentCompat) activity.getMainFragment();
        AutofillProfileEditorPreference addProfile =
                (AutofillProfileEditorPreference) fragment.findPreference(
                        AutofillProfilesFragment.PREF_NEW_PROFILE);
        Assert.assertNotNull(addProfile);

        // Open AutofillProfileEditorPreference.
        TestThreadUtils.runOnUiThreadBlocking(addProfile::performClick);
        rule.setEditorDialogAndWait(addProfile.getEditorDialog());
        // The keyboard is shown as soon as AutofillProfileEditorPreference comes into view.
        waitForKeyboardStatus(true, activity);

        // Hide the keyboard.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<EditText> fields = addProfile.getEditorDialog().getEditableTextFieldsForTest();
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

    private void waitForKeyboardStatus(final boolean keyboardVisible, final Preferences activity) {
        CriteriaHelper.pollUiThread(
                new Criteria("Keyboard was not " + (keyboardVisible ? "shown." : "hidden.")) {
                    @Override
                    public boolean isSatisfied() {
                        return keyboardVisible
                                == KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                                           activity, activity.findViewById(android.R.id.content));
                    }
                });
    }

    private void updatePreferencesAndWait(AutofillProfilesFragment profileFragment,
            AutofillProfileEditorPreference profile, String[] values, int buttonId,
            boolean waitForError) throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(profile::performClick);

        rule.setEditorDialogAndWait(profile.getEditorDialog());
        if (values != null) rule.setTextInEditorAndWait(values);
        if (waitForError) {
            rule.clickInEditorAndWaitForValidationError(buttonId);
        } else {
            rule.clickInEditorAndWait(buttonId);
            rule.waitForThePreferenceUpdate();
        }
    }
}
