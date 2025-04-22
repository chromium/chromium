// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.view.KeyEvent;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.editors.EditorDialogView;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/** Unit test suite for AutofillProfilesFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({
    ChromeFeatureList.PLUS_ADDRESSES_ENABLED,
    ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HOME_AND_WORK
})
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillProfilesFragmentTest {
    private static final AutofillProfile sLocalOrSyncProfile =
            AutofillProfile.builder()
                    .setRecordType(RecordType.LOCAL_OR_SYNCABLE)
                    .setFullName("Seb Doe")
                    .setCompanyName("Google")
                    .setStreetAddress("111 First St")
                    .setRegion("CA")
                    .setLocality("Los Angeles")
                    .setPostalCode("90291")
                    .setCountryCode("US")
                    .setPhoneNumber("650-253-0000")
                    .setEmailAddress("first@gmail.com")
                    .setLanguageCode("en-US")
                    .build();
    private static final AutofillProfile sAccountProfile =
            AutofillProfile.builder()
                    .setRecordType(RecordType.ACCOUNT)
                    .setFullName("Artik Doe")
                    .setCompanyName("Google")
                    .setStreetAddress("999 Fourth St")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90291")
                    .setCountryCode("US")
                    .setPhoneNumber("650-253-0000")
                    .setEmailAddress("artik@gmail.com")
                    .setLanguageCode("en-US")
                    .build();

    private static final AutofillProfile sHomeProfile =
            AutofillProfile.builder()
                    .setRecordType(RecordType.ACCOUNT_HOME)
                    .setFullName("Home Doe")
                    .setCompanyName("Google")
                    .setStreetAddress("242 Fourth St")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90291")
                    .setCountryCode("US")
                    .setPhoneNumber("650-253-0000")
                    .setEmailAddress("home@gmail.com")
                    .setLanguageCode("en-US")
                    .build();
    private static final AutofillProfile sWorkProfile =
            AutofillProfile.builder()
                    .setRecordType(RecordType.ACCOUNT_WORK)
                    .setFullName("Work Doe")
                    .setCompanyName("Google")
                    .setStreetAddress("242 Fourth St")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90291")
                    .setCountryCode("US")
                    .setPhoneNumber("650-253-0000")
                    .setEmailAddress("work@gmail.com")
                    .setLanguageCode("en-US")
                    .build();

    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @ClassRule
    public static final SettingsActivityTestRule<AutofillProfilesFragment>
            sSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillProfilesFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private SyncService mSyncService;

    private final AutofillTestHelper mHelper = new AutofillTestHelper();

    @BeforeClass
    public static void setUpClass() {
        sSettingsActivityTestRule.startSettingsActivity();
    }

    @Before
    public void setUp() throws TimeoutException {
        Intents.init();
        mHelper.setProfile(sLocalOrSyncProfile);
        mHelper.setProfile(
                AutofillProfile.builder()
                        .setFullName("John Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("111 Second St")
                        .setRegion("CA")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("650-253-0000")
                        .setEmailAddress("second@gmail.com")
                        .setLanguageCode("en-US")
                        .build());
        // Invalid state should not cause a crash on the state dropdown list.
        mHelper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Bill Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("111 Third St")
                        .setRegion("XXXYYY")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("650-253-0000")
                        .setEmailAddress("third@gmail.com")
                        .setLanguageCode("en-US")
                        .build());
        // Full value for state should show up correctly on the dropdown list.
        mHelper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Bob Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("111 Fourth St")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("650-253-0000")
                        .setEmailAddress("fourth@gmail.com")
                        .setLanguageCode("en-US")
                        .build());
    }

    @After
    public void tearDown() throws TimeoutException {
        Intents.release();
        mHelper.clearAllDataForTesting();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAddProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
        AutofillProfileEditorPreference addProfile =
                autofillProfileFragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        assertNotNull(addProfile);
        assertFalse(addProfile.getRecordType().isPresent());

        // Add a profile.
        updatePreferencesAndWait(
                autofillProfileFragment,
                addProfile,
                new String[] {
                    "Alice Doe",
                    "Google",
                    "111 Added St",
                    "Los Angeles",
                    "CA",
                    "90291",
                    "650-253-0000",
                    "add@profile.com"
                },
                R.id.editor_dialog_done_button,
                false);

        checkPreferenceCount(
                8 /* One toggle + one add button + five profiles + plus address entry. */);
        AutofillProfileEditorPreference addedProfile = findPreference("Alice Doe");
        assertNotNull(addedProfile);
        assertEquals("111 Added St, 90291", addedProfile.getSummary());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testPlusAddressEntry() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
        AutofillProfileEditorPreference plusAddressEntry =
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.MANAGE_PLUS_ADDRESSES);
        assertNotNull(plusAddressEntry);

        assertEquals(
                sSettingsActivityTestRule
                        .getFragment()
                        .getContext()
                        .getString(R.string.plus_address_settings_entry_title),
                plusAddressEntry.getTitle());
        assertEquals(
                sSettingsActivityTestRule
                        .getFragment()
                        .getContext()
                        .getString(R.string.plus_address_settings_entry_summary),
                plusAddressEntry.getSummary());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testHomeEntry() throws Exception {
        mHelper.setProfile(sHomeProfile);

        AutofillProfileEditorPreference homeProfilePreference =
                findPreference(sHomeProfile.getInfo(FieldType.NAME_FULL));
        assertNotNull(homeProfilePreference);
        assertFalse(homeProfilePreference.shouldShowLocalProfileIcon());
        assertTrue(homeProfilePreference.getRecordType().isPresent());
        assertEquals(RecordType.ACCOUNT_HOME, homeProfilePreference.getRecordType().getAsInt());
        assertTrue(homeProfilePreference.getIcon().isVisible());

        // Define a fake result to return immediately when the intent is caught.
        // This prevents the actual Custom Tab from launching.
        Instrumentation.ActivityResult ok_result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, null);
        var homeIntentMatcher =
                allOf(
                        hasAction(Intent.ACTION_VIEW),
                        hasData(
                                Uri.parse(
                                        AutofillProfilesFragment
                                                .GOOGLE_ACCOUNT_HOME_ADDRESS_EDIT_URL)));
        intending(homeIntentMatcher).respondWith(ok_result);

        // Test that Custom Tab with the correct url is launched when Work address clicked.
        ThreadUtils.runOnUiThreadBlocking(homeProfilePreference::performClick);
        intended(homeIntentMatcher);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testWorkEntry() throws Exception {
        mHelper.setProfile(sWorkProfile);

        // Test work profile
        AutofillProfileEditorPreference workProfilePreference =
                findPreference(sWorkProfile.getInfo(FieldType.NAME_FULL));
        assertNotNull(workProfilePreference);
        assertFalse(workProfilePreference.shouldShowLocalProfileIcon());
        assertTrue(workProfilePreference.getRecordType().isPresent());
        assertEquals(RecordType.ACCOUNT_WORK, workProfilePreference.getRecordType().getAsInt());
        assertTrue(workProfilePreference.getIcon().isVisible());

        // Define a fake result to return immediately when the intent is caught.
        // This prevents the actual Custom Tab from launching.
        Instrumentation.ActivityResult ok_result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, null);
        var workIntentMatcher =
                allOf(
                        hasAction(Intent.ACTION_VIEW),
                        hasData(
                                Uri.parse(
                                        AutofillProfilesFragment
                                                .GOOGLE_ACCOUNT_WORK_ADDRESS_EDIT_URL)));
        intending(workIntentMatcher).respondWith(ok_result);

        // Test that Custom Tab with the correct url is launched when Home address clicked.
        ThreadUtils.runOnUiThreadBlocking(workProfilePreference::performClick);
        intended(workIntentMatcher);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAddIncompletedProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
        AutofillProfileEditorPreference addProfile =
                findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        assertNotNull(addProfile);
        assertFalse(addProfile.getRecordType().isPresent());

        // Add an incomplete profile.
        updatePreferencesAndWait(
                autofillProfileFragment,
                addProfile,
                new String[] {"Mike Doe"},
                R.id.editor_dialog_done_button,
                false);

        // Incomplete profile should still be added.
        checkPreferenceCount(
                8 /* One toggle + one add button + five profiles + plus address entry. */);
        assertNotNull(findPreference("Mike Doe"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAddProfileWithInvalidPhone() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
        AutofillProfileEditorPreference addProfile =
                autofillProfileFragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        assertNotNull(addProfile);
        assertFalse(addProfile.getRecordType().isPresent());

        // Try to add a profile with invalid phone.
        updatePreferencesAndWait(
                autofillProfileFragment,
                addProfile,
                new String[] {"", "", "", "", "", "", "123"},
                R.id.editor_dialog_done_button,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDeleteLocalProfile() throws Exception {
        Context context = sSettingsActivityTestRule.getFragment().getContext();
        setUpMockSyncService(false, new HashSet());
        testDeleteProfile(
                context.getString(R.string.autofill_delete_local_address_record_type_notice));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDeleteSyncableProfile() throws Exception {
        Context context = sSettingsActivityTestRule.getFragment().getContext();
        setUpMockSyncService(true, Collections.singleton(UserSelectableType.AUTOFILL));
        testDeleteProfile(
                context.getString(R.string.autofill_delete_sync_address_record_type_notice));
    }

    public void testDeleteProfile(String expectedConfirmationMessage) throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
        AutofillProfileEditorPreference sebProfile =
                autofillProfileFragment.findPreference("Seb Doe");
        assertNotNull(sebProfile);
        assertEquals("Seb Doe", sebProfile.getTitle());

        // Delete the profile, but cancel on confirmation.
        ThreadUtils.runOnUiThreadBlocking(sebProfile::performClick);
        EditorDialogView editorDialog = autofillProfileFragment.getEditorDialogForTest();
        rule.setEditorDialogAndWait(editorDialog);
        rule.clickInEditorAndWaitForConfirmationDialog(R.id.delete_menu_id);

        // Verify the confirmation message for non-account profile.
        AlertDialog confirmationDialog = editorDialog.getConfirmationDialogForTest();
        assertNotNull(confirmationDialog);
        TextView messageView = confirmationDialog.findViewById(R.id.confirmation_dialog_message);
        assertEquals(expectedConfirmationMessage, messageView.getText());

        // Get back to the profile list.
        rule.clickInConfirmationDialogAndWait(
                DialogInterface.BUTTON_NEGATIVE, /* waitForPreferenceUpdate= */ false);
        rule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, /* waitForPreferenceUpdate= */ false);

        // Make sure the profile is not deleted and the number of profiles didn't change.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);

        // Delete a profile and confirm it.
        ThreadUtils.runOnUiThreadBlocking(sebProfile::performClick);
        rule.setEditorDialogAndWait(autofillProfileFragment.getEditorDialogForTest());
        rule.clickInEditorAndWaitForConfirmationDialog(R.id.delete_menu_id);
        rule.clickInConfirmationDialogAndWait(
                DialogInterface.BUTTON_POSITIVE, /* waitForPreferenceUpdate= */ true);

        // Make sure the profile is deleted.
        checkPreferenceCount(
                6 /* One toggle + one add button + three profiles + plus address entry. */);
        assertNotNull(findPreference("John Doe"));
        assertNull(findPreference("Seb Doe"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDeleteAccountProfile() throws Exception {
        String email = "test@account";
        setUpMockPrimaryAccount(email);

        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();
        Context context = autofillProfileFragment.getContext();

        mHelper.setProfile(sAccountProfile);

        // Check the preferences on the initial screen.
        checkPreferenceCount(
                8 /* One toggle + one add button + five profiles + plus address entry. */);
        AutofillProfileEditorPreference artikProfile = findPreference("Artik Doe");
        assertNotNull(artikProfile);

        // Delete Artik's account profile.
        ThreadUtils.runOnUiThreadBlocking(artikProfile::performClick);
        EditorDialogView editorDialog = autofillProfileFragment.getEditorDialogForTest();
        rule.setEditorDialogAndWait(editorDialog);
        rule.clickInEditorAndWaitForConfirmationDialog(R.id.delete_menu_id);

        // Verify the message.
        AlertDialog confirmationDialog = editorDialog.getConfirmationDialogForTest();
        assertNotNull(confirmationDialog);
        TextView messageView = confirmationDialog.findViewById(R.id.confirmation_dialog_message);
        String expectedMessage =
                context.getString(R.string.autofill_delete_account_address_record_type_notice)
                        .replace("$1", email);
        assertEquals(expectedMessage, messageView.getText());

        rule.clickInConfirmationDialogAndWait(
                DialogInterface.BUTTON_POSITIVE, /* waitForPreferenceUpdate= */ true);

        // Make sure the profile is deleted.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
        assertNull(findPreference("Artik Doe"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testEditProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
        AutofillProfileEditorPreference johnProfile =
                autofillProfileFragment.findPreference("John Doe");
        assertNotNull(johnProfile);
        assertEquals("John Doe", johnProfile.getTitle());
        assertTrue(johnProfile.getIcon().isVisible());

        // Make sure that the icon is visible for non-HW profiles too.
        AutofillProfileEditorPreference billProfile =
                autofillProfileFragment.findPreference("Bill Doe");
        assertNotNull(billProfile);
        assertTrue(billProfile.getIcon().isVisible());

        // Edit a profile.
        ThreadUtils.runOnUiThreadBlocking(johnProfile::performClick);
        EditorDialogView editorDialog = autofillProfileFragment.getEditorDialogForTest();
        rule.setEditorDialogAndWait(editorDialog);
        rule.setTextInEditorAndWait(
                new String[] {
                    "Emily Doe",
                    "Google",
                    "111 Edited St",
                    "Los Angeles",
                    "CA",
                    "90291",
                    "650-253-0000",
                    "edit@profile.com"
                });

        // Verify the absence of the profile source notice.
        TextView footerMessage = editorDialog.findViewById(R.id.footer_message);
        assertEquals(View.GONE, footerMessage.getVisibility());

        rule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, /* waitForPreferenceUpdate= */ true);

        // Check if the preferences are updated correctly.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
        AutofillProfileEditorPreference editedProfile = findPreference("Emily Doe");
        assertNotNull(editedProfile);
        assertEquals("111 Edited St, 90291", editedProfile.getSummary());
        assertNull(findPreference("John Doe"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testEditAccountProfile() throws Exception {
        String email = "test@account";
        setUpMockPrimaryAccount(email);

        mHelper.setProfile(
                AutofillProfile.builder()
                        .setRecordType(RecordType.ACCOUNT)
                        .setFullName("Account Updated #0")
                        .setCompanyName("Google")
                        .setStreetAddress("111 Fourth St")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("650-253-0000")
                        .setEmailAddress("fourth@gmail.com")
                        .setLanguageCode("en-US")
                        .build());

        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();
        Context context = autofillProfileFragment.getContext();

        // Check the preferences on the initial screen.
        checkPreferenceCount(
                8 /* One toggle + one add button + 5 profiles + plus address entry. */);
        AutofillProfileEditorPreference johnProfile = findPreference("Account Updated #0");
        assertNotNull(johnProfile);

        ThreadUtils.runOnUiThreadBlocking(johnProfile::performClick);
        EditorDialogView editorDialog = autofillProfileFragment.getEditorDialogForTest();
        rule.setEditorDialogAndWait(editorDialog);

        // Verify the profile source notice.
        TextView footerMessage = editorDialog.findViewById(R.id.footer_message);
        assertEquals(View.VISIBLE, footerMessage.getVisibility());
        String expectedMessage =
                context.getString(
                                R.string
                                        .autofill_address_already_saved_in_account_record_type_notice)
                        .replace("$1", email);
        assertEquals(expectedMessage, footerMessage.getText());

        // Invalid input.
        rule.setTextInEditorAndWait(
                new String[] {
                    "Account Updated #1",
                    "Google",
                    "" /* Street address is required. */,
                    "Los Angeles",
                    "CA",
                    "90291",
                    "650-253-0000",
                    "edit@profile.com"
                });
        rule.clickInEditorAndWaitForValidationError(R.id.editor_dialog_done_button);

        // Fix invalid input.
        rule.setTextInEditorAndWait(
                new String[] {
                    "Account Updated #2",
                    "Google",
                    "222 Fourth St" /* Enter street address. */,
                    "Los Angeles",
                    "CA",
                    "90291",
                    "650-253-0000",
                    "edit@profile.com"
                });
        rule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, /* waitForPreferenceUpdate= */ true);

        // Check if the preferences are updated correctly.
        checkPreferenceCount(
                8 /* One toggle + one add button + five profiles + plus address entry. */);
        assertNotNull(findPreference("Account Updated #2"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testEditInvalidAccountProfile() throws Exception {
        mHelper.setProfile(
                AutofillProfile.builder()
                        .setRecordType(RecordType.ACCOUNT)
                        .setFullName("Account Updated #0")
                        .setCompanyName("Google")
                        .setStreetAddress("")
                        /* Street address is required in US but already missing. */
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("650-253-0000")
                        .setEmailAddress("fourth@gmail.com")
                        .setLanguageCode("en-US")
                        .build());

        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(
                8 /* One toggle + one add button + 5 profiles + plus address entry. */);
        AutofillProfileEditorPreference johnProfile = findPreference("Account Updated #0");
        assertNotNull(johnProfile);

        // Edit profile.
        updatePreferencesAndWait(
                autofillProfileFragment,
                johnProfile,
                new String[] {
                    "Account Updated #1",
                    "Google",
                    "" /* Dont fix missing Street address. */,
                    "Los Angeles",
                    "CA",
                    "90291",
                    "650-253-0000",
                    "edit@profile.com"
                },
                R.id.editor_dialog_done_button,
                false);

        // Check if the preferences are updated correctly.
        checkPreferenceCount(
                8 /* One toggle + one add button + five profiles + plus address entry. */);
        assertNotNull(findPreference("Account Updated #1"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testOpenProfileWithCompleteState() throws Exception {
        // Check the preferences on the initial screen.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
        AutofillProfileEditorPreference bobProfile = findPreference("Bob Doe");
        assertNotNull(bobProfile);
        assertEquals("Bob Doe", bobProfile.getTitle());

        // Open the profile.
        ThreadUtils.runOnUiThreadBlocking(bobProfile::performClick);
        rule.setEditorDialogAndWait(
                sSettingsActivityTestRule.getFragment().getEditorDialogForTest());
        rule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, /* waitForPreferenceUpdate= */ true);

        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testOpenProfileWithInvalidState() throws Exception {
        // Check the preferences on the initial screen.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
        AutofillProfileEditorPreference billProfile = findPreference("Bill Doe");
        assertNotNull(billProfile);
        assertEquals("Bill Doe", billProfile.getTitle());

        // Open the profile.
        ThreadUtils.runOnUiThreadBlocking(billProfile::performClick);
        rule.setEditorDialogAndWait(
                sSettingsActivityTestRule.getFragment().getEditorDialogForTest());
        rule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, /* waitForPreferenceUpdate= */ true);

        // Check if the preferences are updated correctly.
        checkPreferenceCount(
                7 /* One toggle + one add button + four profiles + plus address entry. */);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableIf.Build(
            sdk_is_less_than = Build.VERSION_CODES.TIRAMISU,
            message = "https://crbug.com/381982174")
    public void testKeyboardShownOnDpadCenter() throws TimeoutException {
        AutofillProfilesFragment fragment = sSettingsActivityTestRule.getFragment();
        AutofillProfileEditorPreference addProfile =
                fragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        assertNotNull(addProfile);
        assertFalse(addProfile.getRecordType().isPresent());

        // Open AutofillProfileEditorPreference.
        ThreadUtils.runOnUiThreadBlocking(addProfile::performClick);
        rule.setEditorDialogAndWait(fragment.getEditorDialogForTest());
        // The keyboard is shown as soon as AutofillProfileEditorPreference comes into view.
        waitForKeyboardStatus(true, sSettingsActivityTestRule.getActivity());

        final List<EditText> fields =
                fragment.getEditorDialogForTest().getEditableTextFieldsForTest();
        // Ensure the first text field is focused.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    fields.get(0).requestFocus();
                });
        // Hide the keyboard.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    KeyboardVisibilityDelegate.getInstance().hideKeyboard(fields.get(0));
                });
        // Check that the keyboard is hidden.
        waitForKeyboardStatus(false, sSettingsActivityTestRule.getActivity());

        // Send a d-pad key event to one of the text fields
        try {
            rule.sendKeycodeToTextFieldInEditorAndWait(KeyEvent.KEYCODE_DPAD_CENTER, 0);
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        // Check that the keyboard was shown.
        waitForKeyboardStatus(true, sSettingsActivityTestRule.getActivity());

        // Close the dialog.
        rule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, /* waitForPreferenceUpdate= */ false);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalProfiles_UserNotSignedIn() throws Exception {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        setUpMockSyncService(false, new HashSet());

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);
        AutofillProfileEditorPreference accountProfilePreference =
                findPreference(sAccountProfile.getInfo(FieldType.NAME_FULL));
        assertFalse(accountProfilePreference.shouldShowLocalProfileIcon());
        assertTrue(accountProfilePreference.getRecordType().isPresent());
        assertEquals(RecordType.ACCOUNT, accountProfilePreference.getRecordType().getAsInt());

        AutofillProfileEditorPreference localOrSyncProfilePreference =
                findPreference(sLocalOrSyncProfile.getInfo(FieldType.NAME_FULL));
        assertFalse(localOrSyncProfilePreference.shouldShowLocalProfileIcon());
        assertTrue(localOrSyncProfilePreference.getRecordType().isPresent());
        assertEquals(
                RecordType.LOCAL_OR_SYNCABLE,
                localOrSyncProfilePreference.getRecordType().getAsInt());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalProfiles_NoSync() throws Exception {
        setUpMockPrimaryAccount("test@account.com");
        setUpMockSyncService(false, new HashSet());

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);
        AutofillProfileEditorPreference accountProfilePreference =
                findPreference(sAccountProfile.getInfo(FieldType.NAME_FULL));
        assertFalse(accountProfilePreference.shouldShowLocalProfileIcon());
        assertTrue(accountProfilePreference.getRecordType().isPresent());
        assertEquals(RecordType.ACCOUNT, accountProfilePreference.getRecordType().getAsInt());

        AutofillProfileEditorPreference localOrSyncProfilePreference =
                findPreference(sLocalOrSyncProfile.getInfo(FieldType.NAME_FULL));
        assertTrue(localOrSyncProfilePreference.shouldShowLocalProfileIcon());
        assertTrue(localOrSyncProfilePreference.getRecordType().isPresent());
        assertEquals(
                RecordType.LOCAL_OR_SYNCABLE,
                localOrSyncProfilePreference.getRecordType().getAsInt());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalProfiles_AddressesNotSynced() throws Exception {
        setUpMockPrimaryAccount("test@account.com");
        setUpMockSyncService(true, new HashSet());

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);
        AutofillProfileEditorPreference accountProfilePreference =
                findPreference(sAccountProfile.getInfo(FieldType.NAME_FULL));
        assertFalse(accountProfilePreference.shouldShowLocalProfileIcon());
        assertTrue(accountProfilePreference.getRecordType().isPresent());
        assertEquals(RecordType.ACCOUNT, accountProfilePreference.getRecordType().getAsInt());

        AutofillProfileEditorPreference localOrSyncProfilePreference =
                findPreference(sLocalOrSyncProfile.getInfo(FieldType.NAME_FULL));
        assertTrue(localOrSyncProfilePreference.shouldShowLocalProfileIcon());
        assertTrue(localOrSyncProfilePreference.getRecordType().isPresent());
        assertEquals(
                RecordType.LOCAL_OR_SYNCABLE,
                localOrSyncProfilePreference.getRecordType().getAsInt());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalProfiles_AddressesSynced() throws Exception {
        setUpMockPrimaryAccount("test@account.com");
        setUpMockSyncService(true, Collections.singleton(UserSelectableType.AUTOFILL));

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);
        AutofillProfileEditorPreference accountProfilePreference =
                findPreference(sAccountProfile.getInfo(FieldType.NAME_FULL));
        assertFalse(accountProfilePreference.shouldShowLocalProfileIcon());
        assertTrue(accountProfilePreference.getRecordType().isPresent());
        assertEquals(RecordType.ACCOUNT, accountProfilePreference.getRecordType().getAsInt());

        AutofillProfileEditorPreference localOrSyncProfilePreference =
                findPreference(sLocalOrSyncProfile.getInfo(FieldType.NAME_FULL));
        assertFalse(localOrSyncProfilePreference.shouldShowLocalProfileIcon());
        assertTrue(localOrSyncProfilePreference.getRecordType().isPresent());
        assertEquals(
                RecordType.LOCAL_OR_SYNCABLE,
                localOrSyncProfilePreference.getRecordType().getAsInt());
    }

    private void checkPreferenceCount(int expectedPreferenceCount) {
        int preferenceCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                sSettingsActivityTestRule
                                        .getFragment()
                                        .getPreferenceScreen()
                                        .getPreferenceCount());
        assertEquals(expectedPreferenceCount, preferenceCount);
    }

    @Nullable
    private AutofillProfileEditorPreference findPreference(String title) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> sSettingsActivityTestRule.getFragment().findPreference(title));
    }

    private void waitForKeyboardStatus(
            final boolean keyboardVisible, final SettingsActivity activity) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            KeyboardVisibilityDelegate.getInstance()
                                    .isKeyboardShowing(
                                            activity, activity.findViewById(android.R.id.content)),
                            Matchers.is(keyboardVisible));
                });
    }

    private void updatePreferencesAndWait(
            AutofillProfilesFragment profileFragment,
            AutofillProfileEditorPreference profile,
            String[] values,
            int buttonId,
            boolean waitForError)
            throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(profile::performClick);

        rule.setEditorDialogAndWait(profileFragment.getEditorDialogForTest());
        rule.setTextInEditorAndWait(values);
        if (waitForError) {
            rule.clickInEditorAndWaitForValidationError(buttonId);
            rule.clickInEditorAndWait(
                    R.id.payments_edit_cancel_button, /* waitForPreferenceUpdate= */ false);
        } else {
            rule.clickInEditorAndWait(buttonId, /* waitForPreferenceUpdate= */ true);
        }
    }

    private void setUpMockPrimaryAccount(String email) {
        CoreAccountInfo coreAccountInfo = rule.addAccount(email);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(coreAccountInfo);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
    }

    private void setUpMockSyncService(boolean enabled, Set<Integer> selectedTypes) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> SyncServiceFactory.setInstanceForTesting(mSyncService));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(enabled);
        when(mSyncService.getSelectedTypes()).thenReturn(selectedTypes);
    }
}
