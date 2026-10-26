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
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.editors.address.AddressEditorMediator;
import org.chromium.chrome.browser.autofill.editors.address.EditorDialogView;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsInTab;
import org.chromium.chrome.browser.settings.SettingsTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.browser_ui.settings.CardWithButtonPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/** Unit test suite for AutofillProfilesFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DisableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
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

    @Rule
    public final SettingsTestRule<AutofillProfilesFragment> mSettingsTestRule =
            new SettingsTestRule<>(AutofillProfilesFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private SyncService mSyncService;

    private AutofillTestHelper mHelper;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() throws TimeoutException {
        Intents.init();
        ThreadUtils.runOnUiThreadBlocking(
                () -> SyncServiceFactory.setInstanceForTesting(mSyncService));
        when(mSyncService.getSelectedTypes()).thenReturn(Collections.emptySet());
        mSettingsTestRule.startSettingsActivity();
        mHelper = new AutofillTestHelper();
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
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Use ProfileManager instead of mSettingsTestRule.getFragment().getProfile()
                    // because tests might have navigated to another fragment (e.g.
                    // AutofillOptionsFragment), causing mSettingsTestRule.getFragment() to throw a
                    // ClassCastException.
                    assertTrue(ProfileManager.isInitialized());
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .clearPref(Pref.AUTOFILL_EMAIL_VERIFICATION_ENABLED);
                });
        mUserActionTester.tearDown();
        Intents.release();
        mHelper.clearAllDataForTesting();
        IdentityServicesProvider.setIdentityManagerForTesting(null);
        PersonalDataManagerFactory.setInstanceForTesting(null);
    }

    private PersonalDataManager setUpSpiedPersonalDataManager() {
        PersonalDataManager realPdm =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                PersonalDataManagerFactory.getForProfile(
                                        mSettingsTestRule.getFragment().getProfile()));
        PersonalDataManager spyPdm = spy(realPdm);
        PersonalDataManagerFactory.setInstanceForTesting(spyPdm);
        return spyPdm;
    }

    /** Opens the "More" menu of a verified email and selects "Remove permission". */
    private void clickRemovePermission(String email) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsTestRule.getFragment().scrollToPreference(email));
        onView(
                        allOf(
                                withId(R.id.verified_email_more_button),
                                withContentDescription(Matchers.containsString(email))))
                .perform(click());
        onView(withText(R.string.autofill_settings_remove_verified_email_permission))
                .perform(click());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAddProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(6 /* One toggle + one add button + four profiles. */);
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

        checkPreferenceCount(7 /* One toggle + one add button + five profiles. */);
        AutofillProfileEditorPreference addedProfile = findPreference("Alice Doe");
        assertNotNull(addedProfile);
        assertEquals("111 Added St, 90291", addedProfile.getSummary());
        assertTrue(mUserActionTester.getActions().contains("AutofillAddressesAdded"));
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
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();

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
        onView(withText(context.getString(R.string.autofill_edit_address_label)))
                .inRoot(isDialog())
                .perform(click());
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
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAddIncompletedProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(6 /* One toggle + one add button + four profiles. */);
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
        checkPreferenceCount(7 /* One toggle + one add button + five profiles. */);
        assertNotNull(findPreference("Mike Doe"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAddProfileWithInvalidPhone() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(6 /* One toggle + one add button + four profiles. */);
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
        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();

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
                                AddressEditorMediator.PROFILE_DELETED_HISTOGRAM,
                                /* value= */ false,
                                /* times= */ 1)
                        .expectBooleanRecordTimes(
                                AddressEditorMediator.PROFILE_DELETED_SETTINGS_HISTOGRAM,
                                /* value= */ false,
                                /* times= */ 1)
                        .build();
        ThreadUtils.runOnUiThreadBlocking(profileToDelete::performClick);
        EditorDialogView editorDialog = autofillProfileFragment.getEditorDialogForTest();
        rule.setEditorDialogAndWait(editorDialog);
        rule.clickInEditorAndWaitForConfirmationDialog(R.id.delete_menu_id);

        // Verify the confirmation message is correct.
        ModalDialogManager dialogManager = editorDialog.getModalDialogManagerForTest();
        assertNotNull(dialogManager);
        PropertyModel propertyModel = dialogManager.getCurrentPresenterForTest().getDialogModel();
        View dialogView = propertyModel.get(ModalDialogProperties.CUSTOM_VIEW);
        TextView messageView = dialogView.findViewById(R.id.description_text_view);
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
                                AddressEditorMediator.PROFILE_DELETED_HISTOGRAM,
                                /* value= */ true,
                                /* times= */ 1)
                        .expectBooleanRecordTimes(
                                AddressEditorMediator.PROFILE_DELETED_SETTINGS_HISTOGRAM,
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
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testDeleteLocalProfile() throws Exception {
        Context context = mSettingsTestRule.getFragment().getContext();
        setUpMockSyncService(new HashSet<>());
        testDeleteProfile(
                "Seb Doe",
                6 /* toggle + add button + 4 profiles */,
                context.getString(R.string.autofill_delete_local_address_record_type_notice));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testDeleteSyncableProfile() throws Exception {
        Context context = mSettingsTestRule.getFragment().getContext();
        setUpMockSyncService(Collections.singleton(UserSelectableType.AUTOFILL));
        testDeleteProfile(
                "Seb Doe",
                6 /* toggle + add button + 4 profiles */,
                context.getString(R.string.autofill_delete_sync_address_record_type_notice));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testDeleteAccountProfile() throws Exception {
        // Setup specific to this test case.
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        mHelper.setProfile(sAccountProfile);
        Context context = mSettingsTestRule.getFragment().getContext();

        // Prepare the expected confirmation message with the account email.
        String expectedMessage =
                context.getString(R.string.autofill_delete_account_address_record_type_notice)
                        .replace("$1", TestAccounts.ACCOUNT1.getEmail());

        // Call the reusable helper method with parameters for this test.
        testDeleteProfile("Artik Doe", 7 /* toggle + add button + 5 profiles */, expectedMessage);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testEditProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(6 /* One toggle + one add button + four profiles. */);
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
        checkPreferenceCount(6 /* One toggle + one add button + four profiles. */);
        AutofillProfileEditorPreference editedProfile = findPreference("Emily Doe");
        assertNotNull(editedProfile);
        assertEquals("111 Edited St, 90291", editedProfile.getSummary());
        assertNull(findPreference("John Doe"));
        assertTrue(mUserActionTester.getActions().contains("AutofillAddressesEdited"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
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

        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();
        Context context = autofillProfileFragment.getContext();

        // Check the preferences on the initial screen.
        checkPreferenceCount(7 /* One toggle + one add button + 5 profiles. */);
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
                .inRoot(isDialog())
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
        checkPreferenceCount(7 /* One toggle + one add button + five profiles. */);
        assertNotNull(findPreference("Account Updated #2"));
        assertTrue(mUserActionTester.getActions().contains("AutofillAddressesEdited"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
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

        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();

        // Check the preferences on the initial screen.
        checkPreferenceCount(7 /* One toggle + one add button + 5 profiles. */);
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
        checkPreferenceCount(7 /* One toggle + one add button + five profiles. */);
        assertNotNull(findPreference("Account Updated #1"));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testOpenProfileWithCompleteState() throws Exception {
        // Check the preferences on the initial screen.
        checkPreferenceCount(6 /* One toggle + one add button + four profiles. */);
        AutofillProfileEditorPreference bobProfile = findPreference("Bob Doe");
        assertNotNull(bobProfile);
        assertEquals("Bob Doe", bobProfile.getTitle());

        // Open the profile.
        ThreadUtils.runOnUiThreadBlocking(bobProfile::performClick);
        rule.setEditorDialogAndWait(mSettingsTestRule.getFragment().getEditorDialogForTest());
        rule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, /* waitForPreferenceUpdate= */ true);

        checkPreferenceCount(6 /* One toggle + one add button + four profiles. */);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testOpenProfileWithInvalidState() throws Exception {
        // Check the preferences on the initial screen.
        checkPreferenceCount(6 /* One toggle + one add button + four profiles. */);
        AutofillProfileEditorPreference billProfile = findPreference("Bill Doe");
        assertNotNull(billProfile);
        assertEquals("Bill Doe", billProfile.getTitle());

        // Open the profile.
        ThreadUtils.runOnUiThreadBlocking(billProfile::performClick);
        rule.setEditorDialogAndWait(mSettingsTestRule.getFragment().getEditorDialogForTest());
        rule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, /* waitForPreferenceUpdate= */ true);

        // Check if the preferences are updated correctly.
        checkPreferenceCount(6 /* One toggle + one add button + four profiles. */);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/381982174")
    public void testKeyboardShownOnDpadCenter() throws TimeoutException {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        AutofillProfileEditorPreference addProfile =
                fragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        assertNotNull(addProfile);

        // Open AutofillProfileEditorPreference.
        ThreadUtils.runOnUiThreadBlocking(addProfile::performClick);
        rule.setEditorDialogAndWait(fragment.getEditorDialogForTest());
        // The keyboard is shown as soon as AutofillProfileEditorPreference comes into view.
        waitForKeyboardStatus(true, mSettingsTestRule.getActivity());

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
        waitForKeyboardStatus(false, mSettingsTestRule.getActivity());

        // Send a d-pad key event to one of the text fields
        try {
            rule.sendKeycodeToTextFieldInEditorAndWait(KeyEvent.KEYCODE_DPAD_CENTER, 0);
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        // Check that the keyboard was shown.
        waitForKeyboardStatus(true, mSettingsTestRule.getActivity());

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
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount()).thenReturn(false);
        setUpMockSyncService(new HashSet<>());

        verifyAddressProfileIcons(/* expectedLocalIconLayout= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalProfiles_NoSync() throws Exception {
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        setUpMockSyncService(new HashSet<>());

        verifyAddressProfileIcons(R.layout.autofill_settings_local_profile_icon);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDisplayedProfileIcons_AddressesNotSynced() throws Exception {
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        setUpMockSyncService(new HashSet<>());

        verifyAddressProfileIcons(R.layout.autofill_settings_local_profile_icon);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDisplayedProfileIcons_AddressesSynced() throws Exception {
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        setUpMockSyncService(Collections.singleton(UserSelectableType.AUTOFILL));

        verifyAddressProfileIcons(/* expectedLocalIconLayout= */ 0);
    }

    @Test
    @MediumTest
    public void testSettingsState_thirdPartyMode() throws Exception {
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        setUpMockSyncService(Collections.singleton(UserSelectableType.AUTOFILL));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });

        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();

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
    }

    @Test
    @MediumTest
    public void testDisabledSettingsText_shownInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);

        assertNotNull(
                autofillProfileFragment.findPreference(AutofillAiDelegate.DISABLED_SETTINGS_INFO));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testDisabledSettingsText_linksToAutofillOptionsPage() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();
        Context context = autofillProfileFragment.getContext();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);

        CardWithButtonPreference disabledSettingsInfoPref =
                autofillProfileFragment.findPreference(AutofillAiDelegate.DISABLED_SETTINGS_INFO);
        assertNotNull(disabledSettingsInfoPref);
        onView(allOf(withId(R.id.icon), isDescendantOfA(withId(R.id.card_layout))))
                .check(matches(isDisplayed()));
        String title = disabledSettingsInfoPref.getTitle().toString();
        assertThat(title)
                .isEqualTo(context.getString(R.string.autofill_disable_settings_explanation_title));
        String summary = disabledSettingsInfoPref.getSummary().toString();
        assertThat(summary)
                .isEqualTo(context.getString(R.string.autofill_disable_settings_explanation));

        onView(withId(R.id.card_button))
                .check(matches(withText(R.string.autofill_disable_settings_button_label)))
                .perform(scrollTo(), click());

        // Verify that the Autofill options fragment is opened.
        assertTrue(rule.getLastestShownFragment() instanceof AutofillOptionsFragment);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSettingsTestRule.getActivity().onBackPressed();
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testDisabledSettingsText_linksToAutofillOptionsPage_defaultAvailabilityEnabled()
            throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        AutofillProfilesFragment autofillProfileFragment = mSettingsTestRule.getFragment();
        Context context = autofillProfileFragment.getContext();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);

        CardWithButtonPreference disabledSettingsInfoPref =
                autofillProfileFragment.findPreference(AutofillAiDelegate.DISABLED_SETTINGS_INFO);
        assertNotNull(disabledSettingsInfoPref);
        onView(allOf(withId(R.id.icon), isDescendantOfA(withId(R.id.card_layout))))
                .check(matches(isDisplayed()));
        String title = disabledSettingsInfoPref.getTitle().toString();
        assertThat(title)
                .isEqualTo(context.getString(R.string.autofill_disable_settings_explanation_title));
        String summary = disabledSettingsInfoPref.getSummary().toString();
        assertThat(summary)
                .isEqualTo(context.getString(R.string.autofill_disable_settings_explanation_v2));

        onView(withId(R.id.card_button))
                .check(matches(withText(R.string.autofill_disable_settings_button_label)))
                .perform(scrollTo(), click());

        // Verify that the Autofill options fragment is opened.
        assertTrue(rule.getLastestShownFragment() instanceof AutofillOptionsFragment);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSettingsTestRule.getActivity().onBackPressed();
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAddressSectionTitle_featureEnabled_showsTitle() throws Exception {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        Preference category = fragment.findPreference("autofill_section_title");
        assertNotNull(category);
        assertEquals(
                mSettingsTestRule
                        .getActivity()
                        .getString(R.string.autofill_addresses_section_title),
                category.getTitle());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAddressSectionTitle_featureDisabled_noTitle() throws Exception {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        Preference category = fragment.findPreference("autofill_section_title");
        assertNull(category);
    }

    @Test
    @MediumTest
    public void testTitle_showsContactInfo() throws Exception {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        assertThat(fragment.getPageTitle().get())
                .isEqualTo(
                        mSettingsTestRule
                                .getActivity()
                                .getString(R.string.autofill_contact_info_title));
    }

    private void checkPreferenceCount(int expectedPreferenceCount) {
        int preferenceCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mSettingsTestRule
                                        .getFragment()
                                        .getPreferenceScreen()
                                        .getPreferenceCount());
        assertEquals(expectedPreferenceCount, preferenceCount);
    }

    @Nullable
    private AutofillProfileEditorPreference findPreference(String title) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsTestRule.getFragment().findPreference(title));
    }

    private void waitForKeyboardStatus(final boolean keyboardVisible, final Activity activity) {
        CriteriaHelper.pollUiThread(
                () -> {
                    // TODO(crbug.com/521895796): Figure out if this should be android.R.id.content
                    // (i.e. the whole activity's content area) or R.id.settings_content (i.e. the
                    // settings activity/page's content).
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
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManagerMock);
        when(mIdentityManagerMock.getPrimaryAccountInfo()).thenReturn(accountInfo);
        when(mIdentityManagerMock.hasPrimaryAccount()).thenReturn(true);
    }

    private void setUpMockSyncService(Set<Integer> selectedTypes) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> when(mSyncService.getSelectedTypes()).thenReturn(selectedTypes));
    }

    @Test
    @SmallTest
    public void testHelpMenuTriggersAutofillHelp() {
        // Settings in a tab doesn't have a help button or menu.
        Assume.assumeTrue(!SettingsInTab.shouldOpenSettingsInTab());

        onView(withId(R.id.menu_id_targeted_help)).perform(click());

        verify(mHelpAndFeedbackLauncher)
                .show(
                        mSettingsTestRule.getActivity(),
                        ContextUtils.getApplicationContext()
                                .getString(R.string.help_context_autofill),
                        /* url= */ null);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testEmailVerificationToggle_InitialStateTrue() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(fragment.getProfile())
                            .setBoolean(Pref.AUTOFILL_EMAIL_VERIFICATION_ENABLED, true);
                    fragment.onPersonalDataChanged();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    ChromeSwitchPreference toggle =
                            fragment.findPreference("autofill_email_verification_enabled");
                    Criteria.checkThat(toggle, Matchers.notNullValue());
                    Criteria.checkThat(toggle.isChecked(), Matchers.is(true));
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testEmailVerificationToggle_InitialStateFalse() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(fragment.getProfile())
                            .setBoolean(Pref.AUTOFILL_EMAIL_VERIFICATION_ENABLED, false);
                    fragment.onPersonalDataChanged();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    ChromeSwitchPreference toggle =
                            fragment.findPreference("autofill_email_verification_enabled");
                    Criteria.checkThat(toggle, Matchers.notNullValue());
                    Criteria.checkThat(toggle.isChecked(), Matchers.is(false));
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testEmailVerificationToggle_ClickToDisable() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(fragment.getProfile())
                            .setBoolean(Pref.AUTOFILL_EMAIL_VERIFICATION_ENABLED, true);
                    fragment.onPersonalDataChanged();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    ChromeSwitchPreference toggle =
                            fragment.findPreference("autofill_email_verification_enabled");
                    Criteria.checkThat(toggle, Matchers.notNullValue());
                    Criteria.checkThat(toggle.isChecked(), Matchers.is(true));
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    ChromeSwitchPreference toggle =
                            fragment.findPreference("autofill_email_verification_enabled");
                    toggle.performClick();
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    boolean enabled =
                            UserPrefs.get(fragment.getProfile())
                                    .getBoolean(Pref.AUTOFILL_EMAIL_VERIFICATION_ENABLED);
                    Criteria.checkThat(enabled, Matchers.is(false));
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testEmailVerificationToggle_ClickToEnable() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(fragment.getProfile())
                            .setBoolean(Pref.AUTOFILL_EMAIL_VERIFICATION_ENABLED, false);
                    fragment.onPersonalDataChanged();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    ChromeSwitchPreference toggle =
                            fragment.findPreference("autofill_email_verification_enabled");
                    Criteria.checkThat(toggle, Matchers.notNullValue());
                    Criteria.checkThat(toggle.isChecked(), Matchers.is(false));
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    ChromeSwitchPreference toggle =
                            fragment.findPreference("autofill_email_verification_enabled");
                    toggle.performClick();
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    boolean enabled =
                            UserPrefs.get(fragment.getProfile())
                                    .getBoolean(Pref.AUTOFILL_EMAIL_VERIFICATION_ENABLED);
                    Criteria.checkThat(enabled, Matchers.is(true));
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testEmailVerificationToggle_Visibility() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        CriteriaHelper.pollUiThread(
                () -> {
                    ChromeSwitchPreference toggle =
                            fragment.findPreference("autofill_email_verification_enabled");
                    Criteria.checkThat(toggle, Matchers.notNullValue());
                    Criteria.checkThat(toggle.isVisible(), Matchers.is(true));
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testVerifiedEmailList_EmptyState() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        PersonalDataManager pdm = setUpSpiedPersonalDataManager();
        doReturn(List.of()).when(pdm).getEmailVerificationAddresses();

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    PreferenceCategory category =
                            fragment.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_SECTION);
                    Criteria.checkThat(category, Matchers.notNullValue());

                    // The section contains the toggle, the info and the empty state label.
                    Criteria.checkThat(category.getPreferenceCount(), Matchers.is(3));
                    Criteria.checkThat(
                            category.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_EMPTY),
                            Matchers.notNullValue());
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testVerifiedEmailList_SingleEmail() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        PersonalDataManager pdm = setUpSpiedPersonalDataManager();
        doReturn(List.of("test@example.com")).when(pdm).getEmailVerificationAddresses();
        doReturn("https://example.com").when(pdm).getEmailVerificationIssuer("test@example.com");

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    PreferenceCategory category =
                            fragment.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_SECTION);
                    Criteria.checkThat(category, Matchers.notNullValue());
                    Preference pref = category.findPreference("test@example.com");
                    Criteria.checkThat(
                            pref, Matchers.instanceOf(AutofillVerifiedEmailPreference.class));
                    Criteria.checkThat(pref.getTitle().toString(), Matchers.is("test@example.com"));
                    // The issuer is no longer shown as a secondary line.
                    Criteria.checkThat(pref.getSummary(), Matchers.nullValue());
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testVerifiedEmailList_MultipleEmails() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        PersonalDataManager pdm = setUpSpiedPersonalDataManager();
        doReturn(List.of("test@example.com", "other@gmail.com"))
                .when(pdm)
                .getEmailVerificationAddresses();
        doReturn("https://example.com").when(pdm).getEmailVerificationIssuer("test@example.com");
        doReturn("https://gmail.com").when(pdm).getEmailVerificationIssuer("other@gmail.com");

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    PreferenceCategory category =
                            fragment.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_SECTION);
                    Criteria.checkThat(category, Matchers.notNullValue());
                    Preference pref1 = category.findPreference("test@example.com");
                    Preference pref2 = category.findPreference("other@gmail.com");
                    Criteria.checkThat(pref1, Matchers.notNullValue());
                    Criteria.checkThat(pref2, Matchers.notNullValue());
                    Criteria.checkThat(
                            fragment.findPreference("autofill_email_verification_enabled"),
                            Matchers.notNullValue());
                    // The section contains the toggle, the info and the two emails.
                    Criteria.checkThat(category.getPreferenceCount(), Matchers.is(4));
                    Criteria.checkThat(
                            category.getPreference(0).getKey(),
                            Matchers.is(AutofillProfilesFragment.PREF_EMAIL_VERIFICATION));
                    Criteria.checkThat(
                            category.getPreference(1).getKey(),
                            Matchers.is(AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_INFO));
                    Criteria.checkThat(
                            category.getPreference(2).getKey(), Matchers.is("test@example.com"));
                    Criteria.checkThat(
                            category.getPreference(3).getKey(), Matchers.is("other@gmail.com"));
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testVerifiedEmailList_EmailTruncation() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        PersonalDataManager pdm = setUpSpiedPersonalDataManager();
        String longEmail = "a".repeat(90) + "@example.com";
        doReturn(List.of(longEmail)).when(pdm).getEmailVerificationAddresses();
        doReturn("https://example.com").when(pdm).getEmailVerificationIssuer(longEmail);

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    PreferenceCategory category =
                            fragment.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_SECTION);
                    Criteria.checkThat(category, Matchers.notNullValue());
                    Preference pref = category.findPreference(longEmail);
                    Criteria.checkThat(pref, Matchers.notNullValue());
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testRemoveEmail_ClickShowsDialog() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        PersonalDataManager pdm = setUpSpiedPersonalDataManager();
        doReturn(List.of("test@example.com")).when(pdm).getEmailVerificationAddresses();
        doReturn("https://example.com").when(pdm).getEmailVerificationIssuer("test@example.com");

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    Preference pref = fragment.findPreference("test@example.com");
                    Criteria.checkThat(pref, Matchers.notNullValue());
                });

        clickRemovePermission("test@example.com");

        onView(withText(R.string.autofill_settings_remove_verified_email_permission_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        String expectedBody =
                ContextUtils.getApplicationContext()
                        .getString(
                                R.string.autofill_settings_remove_verified_email_permission_body,
                                "example.com");
        onView(withText(expectedBody)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(android.R.id.button1))
                .inRoot(isDialog())
                .check(matches(withText(R.string.remove)));
        onView(withId(android.R.id.button2)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(android.R.id.button2)).inRoot(isDialog()).perform(click());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testRemoveEmail_DialogCancelDoesNotRemove() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        PersonalDataManager pdm = setUpSpiedPersonalDataManager();
        doReturn(List.of("test@example.com")).when(pdm).getEmailVerificationAddresses();
        doReturn("https://example.com").when(pdm).getEmailVerificationIssuer("test@example.com");

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        clickRemovePermission("test@example.com");

        onView(withId(android.R.id.button2)).inRoot(isDialog()).perform(click());

        verify(pdm, never()).removeEmailVerificationAddress(any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testRemoveEmail_DialogConfirmRemovesEmail() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        PersonalDataManager pdm = setUpSpiedPersonalDataManager();
        doReturn(List.of("test@example.com")).when(pdm).getEmailVerificationAddresses();
        doReturn("https://example.com").when(pdm).getEmailVerificationIssuer("test@example.com");
        doNothing().when(pdm).removeEmailVerificationAddress(any());

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        clickRemovePermission("test@example.com");

        onView(withId(android.R.id.button1)).inRoot(isDialog()).perform(click());

        verify(pdm).removeEmailVerificationAddress("test@example.com");
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testRemoveEmail_MultiEmailRemoval() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        PersonalDataManager pdm = setUpSpiedPersonalDataManager();
        doReturn(List.of("test@example.com", "other@gmail.com"))
                .when(pdm)
                .getEmailVerificationAddresses();
        doReturn("https://example.com").when(pdm).getEmailVerificationIssuer("test@example.com");
        doReturn("https://gmail.com").when(pdm).getEmailVerificationIssuer("other@gmail.com");
        doNothing().when(pdm).removeEmailVerificationAddress(any());

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        clickRemovePermission("test@example.com");

        doReturn(List.of("other@gmail.com")).when(pdm).getEmailVerificationAddresses();
        onView(withId(android.R.id.button1)).inRoot(isDialog()).perform(click());
        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        verify(pdm).removeEmailVerificationAddress("test@example.com");
        verify(pdm, never()).removeEmailVerificationAddress("other@gmail.com");

        CriteriaHelper.pollUiThread(
                () -> {
                    PreferenceCategory category =
                            fragment.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_SECTION);
                    Criteria.checkThat(
                            category.findPreference("test@example.com"), Matchers.nullValue());
                    Criteria.checkThat(
                            category.findPreference("other@gmail.com"), Matchers.notNullValue());
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testRemoveEmail_ListTransitionToEmpty() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        PersonalDataManager pdm = setUpSpiedPersonalDataManager();
        doReturn(List.of("test@example.com")).when(pdm).getEmailVerificationAddresses();
        doReturn("https://example.com").when(pdm).getEmailVerificationIssuer("test@example.com");
        doNothing().when(pdm).removeEmailVerificationAddress(any());

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        clickRemovePermission("test@example.com");

        doReturn(List.of()).when(pdm).getEmailVerificationAddresses();
        onView(withId(android.R.id.button1)).inRoot(isDialog()).perform(click());
        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    PreferenceCategory category =
                            fragment.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_SECTION);
                    Criteria.checkThat(category, Matchers.notNullValue());
                    // The section contains the toggle, the info and the empty state label.
                    Criteria.checkThat(category.getPreferenceCount(), Matchers.is(3));
                    Criteria.checkThat(
                            category.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_EMPTY),
                            Matchers.notNullValue());
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testEmailVerification_InteractionToggleAndList() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        PersonalDataManager pdm = setUpSpiedPersonalDataManager();
        doReturn(List.of("test@example.com")).when(pdm).getEmailVerificationAddresses();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(fragment.getProfile())
                            .setBoolean(Pref.AUTOFILL_EMAIL_VERIFICATION_ENABLED, false);
                    fragment.onPersonalDataChanged();
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    PreferenceCategory category =
                            fragment.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_SECTION);
                    Criteria.checkThat(category, Matchers.notNullValue());
                    // Only the toggle and the info are shown when the feature is disabled.
                    Criteria.checkThat(category.getPreferenceCount(), Matchers.is(2));
                    Criteria.checkThat(
                            category.findPreference("test@example.com"), Matchers.nullValue());
                });
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testEmailVerification_InfoShownWhenToggleOff() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(fragment.getProfile())
                            .setBoolean(Pref.AUTOFILL_EMAIL_VERIFICATION_ENABLED, false);
                    fragment.onPersonalDataChanged();
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Preference info =
                            fragment.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_INFO);
                    Criteria.checkThat(info, Matchers.notNullValue());
                    Criteria.checkThat(info.isSelectable(), Matchers.is(false));
                });
        verifyEmailVerificationInfoDisplayed();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testEmailVerification_InfoShownWhenToggleOn() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(fragment.getProfile())
                            .setBoolean(Pref.AUTOFILL_EMAIL_VERIFICATION_ENABLED, true);
                    fragment.onPersonalDataChanged();
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            fragment.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_INFO),
                            Matchers.notNullValue());
                });
        verifyEmailVerificationInfoDisplayed();
    }

    private void verifyEmailVerificationInfoDisplayed() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        int[] stringIds = {
            R.string.settings_heading_when_on,
            R.string.autofill_settings_email_verification_when_on_confirm,
            R.string.settings_heading_consider,
            R.string.autofill_settings_email_verification_consider_supported_sites,
            R.string.autofill_settings_email_verification_consider_per_email,
            R.string.autofill_settings_email_verification_consider_signed_in,
        };
        // The list may be rebuilt asynchronously after onPersonalDataChanged(), which can make an
        // earlier scroll target stale. Re-scroll on every poll until the info view is bound.
        CriteriaHelper.pollUiThread(
                () -> {
                    View header =
                            fragment.getListView()
                                    .findViewById(R.id.email_verification_when_on_header);
                    if (header == null || !header.isShown()) {
                        fragment.scrollToPreference(
                                AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_INFO);
                    }
                    Criteria.checkThat(header, Matchers.notNullValue());
                    Criteria.checkThat(header.isShown(), Matchers.is(true));
                    // The header's parent is the root of the info layout.
                    String infoText = collectText((View) header.getParent());
                    for (int id : stringIds) {
                        Criteria.checkThat(
                                infoText, Matchers.containsString(fragment.getString(id)));
                    }
                },
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL * 3,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /** Returns the concatenated text of all TextViews in the given view hierarchy. */
    private static String collectText(View view) {
        if (view instanceof TextView textView) {
            return textView.getText() + "\n";
        }
        StringBuilder text = new StringBuilder();
        if (view instanceof ViewGroup group) {
            for (int i = 0; i < group.getChildCount(); i++) {
                text.append(collectText(group.getChildAt(i)));
            }
        }
        return text.toString();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL})
    @MediumTest
    public void testEmailVerification_FeatureDisabled() {
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        CriteriaHelper.pollUiThread(
                () -> {
                    ChromeSwitchPreference toggle =
                            fragment.findPreference("autofill_email_verification_enabled");
                    PreferenceCategory category =
                            fragment.findPreference(
                                    AutofillProfilesFragment.PREF_EMAIL_VERIFICATION_SECTION);
                    Criteria.checkThat(toggle, Matchers.nullValue());
                    Criteria.checkThat(category, Matchers.nullValue());
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL)
    public void testSearchIndexWhenEmailVerificationEnabled() {
        mSettingsTestRule.startSettingsActivity();
        SettingsIndexData indexDataMock = mock(SettingsIndexData.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillProfilesFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsTestRule.getActivity(),
                            indexDataMock,
                            mSettingsTestRule.getFragment().getProfile());
                });

        verify(indexDataMock, atLeastOnce())
                .addEntryForKey(
                        eq(AutofillProfilesFragment.class.getName()),
                        eq(AutofillProfilesFragment.PREF_EMAIL_VERIFICATION),
                        anyInt());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.EMAIL_VERIFICATION_PROTOCOL)
    public void testSearchIndexWhenEmailVerificationDisabled() {
        mSettingsTestRule.startSettingsActivity();
        SettingsIndexData indexDataMock = mock(SettingsIndexData.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillProfilesFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsTestRule.getActivity(),
                            indexDataMock,
                            mSettingsTestRule.getFragment().getProfile());
                });

        verify(indexDataMock, never())
                .addEntryForKey(
                        eq(AutofillProfilesFragment.class.getName()),
                        eq(AutofillProfilesFragment.PREF_EMAIL_VERIFICATION),
                        anyInt());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testSaveAndFillAddressesToggle_whenUserEnabled() {
        PersonalDataManager spyPdm = setUpSpiedPersonalDataManager();
        doReturn(true).when(spyPdm).isAutofillProfileEnabled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSettingsTestRule.getFragment().rebuildProfileList();
                });
        ChromeSwitchPreference switchPref =
                mSettingsTestRule
                        .getFragment()
                        .findPreference(AutofillProfilesFragment.SAVE_AND_FILL_ADDRESSES);
        assertNotNull(switchPref);
        assertTrue(switchPref.isChecked());
        assertTrue(switchPref.isEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testSaveAndFillAddressesToggle_whenUserDisabled() {
        PersonalDataManager spyPdm = setUpSpiedPersonalDataManager();
        doReturn(false).when(spyPdm).isAutofillProfileEnabled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSettingsTestRule.getFragment().rebuildProfileList();
                });
        ChromeSwitchPreference switchPref =
                mSettingsTestRule
                        .getFragment()
                        .findPreference(AutofillProfilesFragment.SAVE_AND_FILL_ADDRESSES);
        assertNotNull(switchPref);
        assertFalse(switchPref.isChecked());
        assertTrue(switchPref.isEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testSaveAndFillAddressesToggle_whenManagedByPolicy() {
        PersonalDataManager spyPdm = setUpSpiedPersonalDataManager();
        doReturn(true).when(spyPdm).isAutofillProfileManaged();
        doReturn(false).when(spyPdm).isAutofillProfileEnabled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSettingsTestRule.getFragment().rebuildProfileList();
                });
        AutofillProfilesFragment fragment = mSettingsTestRule.getFragment();
        ChromeSwitchPreference switchPref =
                fragment.findPreference(AutofillProfilesFragment.SAVE_AND_FILL_ADDRESSES);
        assertNotNull(switchPref);
        // When managed by organization policy, the toggle must be shown as turned OFF.
        assertFalse(switchPref.isChecked());
        assertFalse(switchPref.isEnabled());
        assertNotNull(switchPref.getManagedPreferenceDelegate());
        assertTrue(
                switchPref
                        .getManagedPreferenceDelegate()
                        .isPreferenceControlledByPolicy(switchPref));
        assertTrue(switchPref.getManagedPreferenceDelegate().isPreferenceClickDisabled(switchPref));

        // Add address button should not be shown when the setting is managed / disabled.
        AutofillProfileEditorPreference addProfile =
                fragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        assertNull(addProfile);
    }
}
