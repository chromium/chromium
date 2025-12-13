// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

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
import android.view.KeyEvent;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.editors.EditorDialogView;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
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
import org.chromium.components.browser_ui.settings.CardWithButtonPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
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
@DoNotBatch(
        reason =
                "TODO(crbug.com/437074185): The tests are leaking state. Fix and re-enable"
                        + " batching.")
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
    private static final AutofillProfile sAccountNameEmailProfile =
            AutofillProfile.builder()
                    .setRecordType(RecordType.ACCOUNT_NAME_EMAIL)
                    .setFullName("Elisa Beckett")
                    .setEmailAddress("elisa.beckett@gmail.com")
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);
                });
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

    /**
     * Verifies that clicking the edit link for specific profile types launches an external URL.
     *
     * <p>This helper is for testing profiles that are not editable within the app, such as Home and
     * Work addresses, which are managed in the user's Google Account. It checks that the preference
     * displays the correct icon and that clicking the "Edit" link correctly fires an ACTION_VIEW
     * intent for the specified URL.
     *
     * @param profile The profile to add and test.
     * @param expectedWidgetLayout The expected resource ID for the preference's widget layout.
     * @param expectedUrl The external URL that is expected to be launched for editing.
     */
    private void testExternalEditEntryPoint(
            AutofillProfile profile, @LayoutRes int expectedWidgetLayout, String expectedUrl)
            throws Exception {
        mHelper.setProfile(profile);
        AutofillProfilesFragment fragment = sSettingsActivityTestRule.getFragment();

        AutofillProfileEditorPreference profilePreference =
                fragment.findPreference(profile.getInfo(FieldType.NAME_FULL));
        assertNotNull(profilePreference);
        assertEquals(expectedWidgetLayout, profilePreference.getWidgetLayoutResource());

        // Edit a profile.
        ThreadUtils.runOnUiThreadBlocking(profilePreference::performClick);
        rule.setEditorDialogAndWait(fragment.getEditorDialogForTest());

        // Mock the intent that should be fired.
        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, null);
        var intentMatcher = allOf(hasAction(Intent.ACTION_VIEW), hasData(Uri.parse(expectedUrl)));
        intending(intentMatcher).respondWith(result);

        // Click the "Edit" link and verify the correct intent was sent.
        Context context = fragment.getContext();
        onView(withText(context.getString(R.string.autofill_edit_address_label))).perform(click());
        intended(intentMatcher);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testHomeEntry() throws Exception {
        testExternalEditEntryPoint(
                sHomeProfile,
                R.layout.autofill_settings_home_profile_icon,
                AutofillProfilesFragment.GOOGLE_ACCOUNT_HOME_ADDRESS_EDIT_URL);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testWorkEntry() throws Exception {
        testExternalEditEntryPoint(
                sWorkProfile,
                R.layout.autofill_settings_work_profile_icon,
                AutofillProfilesFragment.GOOGLE_ACCOUNT_WORK_ADDRESS_EDIT_URL);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAccountNameEmailEntry() throws Exception {
        testExternalEditEntryPoint(
                sAccountNameEmailProfile,
                0, // No widget layout resource
                AutofillProfilesFragment.GOOGLE_ACCOUNT_NAME_EMAIL_ADDRESS_EDIT_URL);
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

        // Try to add a profile with invalid phone.
        updatePreferencesAndWait(
                autofillProfileFragment,
                addProfile,
                new String[] {"", "", "", "", "", "", "123"},
                R.id.editor_dialog_done_button,
                true);
    }

    /**
     * A helper method for testing the deletion of an Autofill profile. Clicks the deletion button
     * but cancels the flow. Clicks the deletion button again and confirms the deletion dialog by
     * clicking the corresponding positive button.
     *
     * @param profileNameToDelete The name of the profile to be deleted.
     * @param initialCount The number of preferences expected before the deletion.
     * @param expectedConfirmationMessage The confirmation message expected to be shown to the user.
     */
    private void testDeleteProfile(
            String profileNameToDelete, int initialCount, String expectedConfirmationMessage)
            throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(initialCount);
        AutofillProfileEditorPreference profileToDelete =
                autofillProfileFragment.findPreference(profileNameToDelete);
        assertNotNull("Profile to delete not found", profileToDelete);
        assertEquals(profileNameToDelete, profileToDelete.getTitle());

        // --- Part 1: Attempt to delete the profile, but cancel on confirmation. ---
        HistogramWatcher deletionCanceledHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                EditorDialogView.PROFILE_DELETED_HISTOGRAM,
                                /* value= */ false,
                                /* times= */ 1)
                        .expectBooleanRecordTimes(
                                EditorDialogView.PROFILE_DELETED_SETTINGS_HISTOGRAM,
                                /* value= */ false,
                                /* times= */ 1)
                        .build();
        ThreadUtils.runOnUiThreadBlocking(profileToDelete::performClick);
        EditorDialogView editorDialog = autofillProfileFragment.getEditorDialogForTest();
        rule.setEditorDialogAndWait(editorDialog);
        rule.clickInEditorAndWaitForConfirmationDialog(R.id.delete_menu_id);

        // Verify the confirmation message is correct.
        AlertDialog confirmationDialog = editorDialog.getConfirmationDialogForTest();
        assertNotNull(confirmationDialog);
        TextView messageView = confirmationDialog.findViewById(R.id.confirmation_dialog_message);
        assertEquals(expectedConfirmationMessage, messageView.getText().toString());

        // Click cancel and ensure we return to the editor, then the main list.
        rule.clickInConfirmationDialogAndWait(
                DialogInterface.BUTTON_NEGATIVE, /* waitForPreferenceUpdate= */ false);
        rule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, /* waitForPreferenceUpdate= */ false);

        // Verify that the profile was NOT deleted.
        checkPreferenceCount(initialCount);
        assertNotNull(findPreference(profileNameToDelete));
        deletionCanceledHistogramWatcher.assertExpected();

        // --- Part 2: Delete the profile and confirm it. ---
        HistogramWatcher deletionConfirmedHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                EditorDialogView.PROFILE_DELETED_HISTOGRAM,
                                /* value= */ true,
                                /* times= */ 1)
                        .expectBooleanRecordTimes(
                                EditorDialogView.PROFILE_DELETED_SETTINGS_HISTOGRAM,
                                /* value= */ true,
                                /* times= */ 1)
                        .build();
        ThreadUtils.runOnUiThreadBlocking(profileToDelete::performClick);
        rule.setEditorDialogAndWait(autofillProfileFragment.getEditorDialogForTest());
        rule.clickInEditorAndWaitForConfirmationDialog(R.id.delete_menu_id);
        rule.clickInConfirmationDialogAndWait(
                DialogInterface.BUTTON_POSITIVE, /* waitForPreferenceUpdate= */ true);

        // Verify that the profile IS deleted and the preference count has decreased.
        checkPreferenceCount(initialCount - 1);
        assertNull("Profile should have been deleted", findPreference(profileNameToDelete));
        deletionConfirmedHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDeleteLocalProfile() throws Exception {
        Context context = sSettingsActivityTestRule.getFragment().getContext();
        setUpMockSyncService(false, new HashSet());
        testDeleteProfile(
                "Seb Doe",
                7 /* toggle + add button + 4 profiles + plus address entry */,
                context.getString(R.string.autofill_delete_local_address_record_type_notice));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDeleteSyncableProfile() throws Exception {
        Context context = sSettingsActivityTestRule.getFragment().getContext();
        setUpMockSyncService(true, Collections.singleton(UserSelectableType.AUTOFILL));
        testDeleteProfile(
                "Seb Doe",
                7 /* toggle + add button + 4 profiles + plus address entry */,
                context.getString(R.string.autofill_delete_sync_address_record_type_notice));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDeleteAccountProfile() throws Exception {
        // Setup specific to this test case.
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        mHelper.setProfile(sAccountProfile);
        Context context = sSettingsActivityTestRule.getFragment().getContext();

        // Prepare the expected confirmation message with the account email.
        String expectedMessage =
                context.getString(R.string.autofill_delete_account_address_record_type_notice)
                        .replace("$1", TestAccounts.ACCOUNT1.getEmail());

        // Call the reusable helper method with parameters for this test.
        testDeleteProfile(
                "Artik Doe",
                8 /* toggle + add button + 5 profiles + plus address entry */,
                expectedMessage);
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

        // Make sure that the icon is visible for non-HW profiles too.
        AutofillProfileEditorPreference billProfile =
                autofillProfileFragment.findPreference("Bill Doe");
        assertNotNull(billProfile);

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
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);

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
        String expectedMessage =
                context.getString(
                                R.string
                                        .autofill_address_already_saved_in_account_record_type_notice)
                        .replace("$1", TestAccounts.ACCOUNT1.getEmail());
        onView(withText(expectedMessage))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));

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
    @DisabledTest(message = "https://crbug.com/381982174")
    public void testKeyboardShownOnDpadCenter() throws TimeoutException {
        AutofillProfilesFragment fragment = sSettingsActivityTestRule.getFragment();
        AutofillProfileEditorPreference addProfile =
                fragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        assertNotNull(addProfile);

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

    /**
     * Verifies that profile icons are shown correctly for the currently established sign-in and
     * sync state.
     *
     * @param expectedLocalIconLayout The expected widget layout for the local/sync profile.
     */
    private void verifyAddressProfileIcons(@LayoutRes int expectedLocalIconLayout)
            throws Exception {
        // Trigger the address profile list to be rebuilt with the new state.
        mHelper.setProfile(sAccountProfile);

        // The account profile icon is always hidden.
        AutofillProfileEditorPreference accountProfilePreference =
                findPreference(sAccountProfile.getInfo(FieldType.NAME_FULL));
        assertEquals(0, accountProfilePreference.getWidgetLayoutResource());

        // The local/sync profile icon visibility depends on the sync state.
        AutofillProfileEditorPreference localOrSyncProfilePreference =
                findPreference(sLocalOrSyncProfile.getInfo(FieldType.NAME_FULL));
        assertEquals(
                expectedLocalIconLayout, localOrSyncProfilePreference.getWidgetLayoutResource());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalProfiles_UserNotSignedIn() throws Exception {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        setUpMockSyncService(false, new HashSet<>());

        verifyAddressProfileIcons(/* expectedLocalIconLayout= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalProfiles_NoSync() throws Exception {
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        setUpMockSyncService(false, new HashSet<>());

        verifyAddressProfileIcons(R.layout.autofill_settings_local_profile_icon);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDisplayedProfileIcons_AddressesNotSynced() throws Exception {
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        setUpMockSyncService(true, new HashSet<>());

        verifyAddressProfileIcons(R.layout.autofill_settings_local_profile_icon);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDisplayedProfileIcons_AddressesSynced() throws Exception {
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        setUpMockSyncService(true, Collections.singleton(UserSelectableType.AUTOFILL));

        verifyAddressProfileIcons(/* expectedLocalIconLayout= */ 0);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testSettingsState_thirdPartyMode() throws Exception {
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        setUpMockSyncService(true, Collections.singleton(UserSelectableType.AUTOFILL));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });

        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);
        AutofillProfileEditorPreference accountProfilePreference =
                findPreference(sAccountProfile.getInfo(FieldType.NAME_FULL));
        assertNotNull(accountProfilePreference);

        // Save and fill addresses toggle should be disabled.
        ChromeSwitchPreference saveAndFillToggle =
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.SAVE_AND_FILL_ADDRESSES);
        assertFalse(saveAndFillToggle.isEnabled());

        // Address list should be shown.
        AutofillProfileEditorPreference localOrSyncProfilePreference =
                findPreference(sLocalOrSyncProfile.getInfo(FieldType.NAME_FULL));
        assertNotNull(localOrSyncProfilePreference);

        // Add address button should be hidden.
        AutofillProfileEditorPreference addProfile =
                autofillProfileFragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        assertNull(addProfile);

        // Plus address entry should be shown.
        AutofillProfileEditorPreference plusAddressEntry =
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.MANAGE_PLUS_ADDRESSES);
        assertNotNull(plusAddressEntry);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testDisabledSettingsText_shownInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);

        assertNotNull(
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.DISABLED_SETTINGS_INFO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testDisabledSettingsText_linksToAutofillOptionsPage() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();
        Context context = autofillProfileFragment.getContext();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);

        CardWithButtonPreference disabled_settings_info_pref =
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.DISABLED_SETTINGS_INFO);
        assertNotNull(disabled_settings_info_pref);
        onView(allOf(withId(R.id.icon), isDescendantOfA(withId(R.id.card_layout))))
                .check(matches(isDisplayed()));
        String title = disabled_settings_info_pref.getTitle().toString();
        assertThat(title)
                .isEqualTo(context.getString(R.string.autofill_disable_settings_explanation_title));
        String summary = disabled_settings_info_pref.getSummary().toString();
        assertThat(summary)
                .isEqualTo(context.getString(R.string.autofill_disable_settings_explanation));

        onView(withId(R.id.card_button))
                .check(matches(withText(R.string.autofill_disable_settings_button_label)))
                .perform(scrollTo(), click());

        // Verify that the Autofill options fragment is opened.
        assertTrue(rule.getLastestShownFragment() instanceof AutofillOptionsFragment);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sSettingsActivityTestRule.getActivity().onBackPressed();
                });
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
                                    .isKeyboardShowing(activity.findViewById(android.R.id.content)),
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

    private void setUpMockPrimaryAccount(AccountInfo accountInfo) {
        rule.addAccount(accountInfo);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(accountInfo);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
    }

    private void setUpMockSyncService(boolean enabled, Set<Integer> selectedTypes) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> SyncServiceFactory.setInstanceForTesting(mSyncService));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(enabled);
        when(mSyncService.getSelectedTypes()).thenReturn(selectedTypes);
    }
}
