// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
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
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceScreen;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.GoogleWalletLauncher;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager.EntityDataManagerObserver;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.autofill.editors.address.AddressEditorMediator;
import org.chromium.chrome.browser.autofill.editors.address.EditorDialogView;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.utils.TestUtils;
import org.chromium.components.browser_ui.settings.CardWithButtonPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;

import java.time.LocalDate;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/** Unit test suite for AutofillProfilesFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HOME_AND_WORK})
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

    @Rule
    public final SettingsActivityTestRule<AutofillProfilesFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillProfilesFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private SyncService mSyncService;
    @Mock private ReauthenticatorBridge mMockReauthenticatorBridge;
    @Mock private EntityDataManager mEntityDataManager;

    private AutofillTestHelper mHelper;

    @Before
    public void setUp() throws TimeoutException {
        ReauthenticatorBridge.setInstanceForTesting(mMockReauthenticatorBridge);
        Intents.init();
        when(mEntityDataManager.getEntitiesWithLabels()).thenReturn(Collections.emptyList());
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);
        when(mEntityDataManager.isWalletPublicPassStorageEnabled()).thenReturn(true);
        when(mEntityDataManager.canListEntityInstancesInSettings()).thenReturn(true);
        mSettingsActivityTestRule.startSettingsActivity();
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
    }

    @After
    public void tearDown() throws TimeoutException {
        Intents.release();
        mHelper.clearAllDataForTesting();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAddProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

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
        AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();

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
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAddIncompletedProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

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
        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

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
        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

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
        Context context = mSettingsActivityTestRule.getFragment().getContext();
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
        Context context = mSettingsActivityTestRule.getFragment().getContext();
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
        Context context = mSettingsActivityTestRule.getFragment().getContext();

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
        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

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

        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();
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

        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

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
        rule.setEditorDialogAndWait(
                mSettingsActivityTestRule.getFragment().getEditorDialogForTest());
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
        rule.setEditorDialogAndWait(
                mSettingsActivityTestRule.getFragment().getEditorDialogForTest());
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
        AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
        AutofillProfileEditorPreference addProfile =
                fragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        assertNotNull(addProfile);

        // Open AutofillProfileEditorPreference.
        ThreadUtils.runOnUiThreadBlocking(addProfile::performClick);
        rule.setEditorDialogAndWait(fragment.getEditorDialogForTest());
        // The keyboard is shown as soon as AutofillProfileEditorPreference comes into view.
        waitForKeyboardStatus(true, mSettingsActivityTestRule.getActivity());

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
        waitForKeyboardStatus(false, mSettingsActivityTestRule.getActivity());

        // Send a d-pad key event to one of the text fields
        try {
            rule.sendKeycodeToTextFieldInEditorAndWait(KeyEvent.KEYCODE_DPAD_CENTER, 0);
        } catch (Exception ex) {
            ex.printStackTrace();
        }
        // Check that the keyboard was shown.
        waitForKeyboardStatus(true, mSettingsActivityTestRule.getActivity());

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

        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

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
        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);

        assertNotNull(
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.DISABLED_SETTINGS_INFO));
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
        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();
        Context context = autofillProfileFragment.getContext();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);

        CardWithButtonPreference disabledSettingsInfoPref =
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.DISABLED_SETTINGS_INFO);
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
                    mSettingsActivityTestRule.getActivity().onBackPressed();
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
        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();
        Context context = autofillProfileFragment.getContext();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);

        CardWithButtonPreference disabledSettingsInfoPref =
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.DISABLED_SETTINGS_INFO);
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
                    mSettingsActivityTestRule.getActivity().onBackPressed();
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_SHOW_WALLET_DISABLED_BANNER)
    public void testDisabledWalletDataSharingDataCard_shownWhenDisabled() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);
                });
        when(mEntityDataManager.isWalletPublicPassStorageEnabled()).thenReturn(false);

        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        ThreadUtils.runOnUiThreadBlocking(autofillProfileFragment::onPersonalDataChanged);

        assertNotNull(
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.DISABLED_WALLET_DATA_SHARING));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testDisabledWalletDataSharingDataCard_notShownWhenWalletPublicPassEnabled()
            throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);
                });
        when(mEntityDataManager.isWalletPublicPassStorageEnabled()).thenReturn(true);

        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        ThreadUtils.runOnUiThreadBlocking(autofillProfileFragment::onPersonalDataChanged);

        assertNull(
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.DISABLED_WALLET_DATA_SHARING));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testDisabledWalletDataSharingDataCard_notShownInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        when(mEntityDataManager.isWalletPublicPassStorageEnabled()).thenReturn(false);

        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        ThreadUtils.runOnUiThreadBlocking(autofillProfileFragment::onPersonalDataChanged);

        assertNull(
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.DISABLED_WALLET_DATA_SHARING));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_SHOW_WALLET_DISABLED_BANNER)
    public void testDisabledWalletDataSharingDataCard_notShownWhenFeatureDisabled()
            throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);
                });
        when(mEntityDataManager.isWalletPublicPassStorageEnabled()).thenReturn(false);

        AutofillProfilesFragment autofillProfileFragment = mSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        ThreadUtils.runOnUiThreadBlocking(autofillProfileFragment::onPersonalDataChanged);

        assertNull(
                autofillProfileFragment.findPreference(
                        AutofillProfilesFragment.DISABLED_WALLET_DATA_SHARING));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_notRenderedIfCannotListEntityInstancesInSettings()
            throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();
        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        vehicleType,
                        /* entityInstanceLabel= */ "Vehicle",
                        /* entityInstanceSubLabel= */ "Mercedez",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.canListEntityInstancesInSettings()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference category = fragment.findPreference("Vehicle");
                    Criteria.checkThat(
                            "Vehicle category should NOT exist", category, Matchers.nullValue());
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAutofillAiEntities_renderedCorrectly() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();
        EntityType passportType = TestUtils.getPassportEntityType();
        EntityType nationalIdType = TestUtils.getNationalIdEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        vehicleType,
                        /* entityInstanceLabel= */ "Vehicle",
                        /* entityInstanceSubLabel= */ "Mercedez",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        EntityInstanceWithLabels entity2 =
                new EntityInstanceWithLabels(
                        "guid2",
                        passportType,
                        /*entityName*/ "Passport",
                        /* entityInstanceSubLabel= */ "Germany",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Arrays.asList(entity1));
        instancesMap.put(passportType, Arrays.asList(entity2));
        instancesMap.put(nationalIdType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference vehicleCategory = fragment.findPreference("Vehicle");
                    Criteria.checkThat(
                            "Vehicle entity category should exist",
                            vehicleCategory,
                            Matchers.notNullValue());
                    PreferenceGroup vehicleGroup = (PreferenceGroup) vehicleCategory;
                    Preference vehicleEntity = fragment.findPreference("guid1");
                    Criteria.checkThat(
                            "Vehicle entity should exist", vehicleEntity, Matchers.notNullValue());
                    Criteria.checkThat(
                            "Vehicle summary should match",
                            vehicleEntity.getSummary(),
                            Matchers.is("Mercedez"));
                    Preference addVehicle = vehicleGroup.findPreference("Vehicle" + " Add");
                    Criteria.checkThat(
                            "Add Vehicle button should exist in category",
                            addVehicle,
                            Matchers.notNullValue());

                    Preference passportCategory = fragment.findPreference("Passport");
                    Criteria.checkThat(
                            "Passport entity category should exist",
                            passportCategory,
                            Matchers.notNullValue());
                    PreferenceGroup passportGroup = (PreferenceGroup) passportCategory;
                    Preference passportEntity = fragment.findPreference("guid2");
                    Criteria.checkThat(
                            "Passport entity should exist",
                            passportEntity,
                            Matchers.notNullValue());
                    Preference addPassport = passportGroup.findPreference("Passport" + " Add");
                    Criteria.checkThat(
                            "Add Passport button should exist because it is writable by default in"
                                    + " TestUtils",
                            addPassport,
                            Matchers.notNullValue());

                    Preference nationalIdCategory = fragment.findPreference("National ID");
                    Criteria.checkThat(
                            "National ID entity category should exist",
                            nationalIdCategory,
                            Matchers.notNullValue());
                    PreferenceGroup nationalIdGroup = (PreferenceGroup) nationalIdCategory;
                    Preference addNationalId =
                            nationalIdGroup.findPreference("National ID" + " Add");
                    Criteria.checkThat(
                            "Add National ID button should exist in category even if no entities"
                                    + " exist",
                            addNationalId,
                            Matchers.notNullValue());

                    PreferenceScreen screen = fragment.getPreferenceScreen();
                    int categoryCount = 0;
                    for (int i = 0; i < screen.getPreferenceCount(); i++) {
                        Preference pref = screen.getPreference(i);
                        if (pref instanceof PreferenceCategory) {
                            categoryCount++;
                        }
                    }
                    Criteria.checkThat(
                            "Entities category count should be 3 (Passport, Vehicle, and National"
                                    + " ID)",
                            categoryCount,
                            Matchers.is(3));
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_notRenderedIfDisabledAndEmpty() throws Exception {
        EntityType disabledType =
                TestUtils.getVehicleEntityType(
                        /* isReadOnly= */ false,
                        /* isEnabled= */ false,
                        /* isEligibleForWalletStorage= */ false);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(disabledType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference category =
                            fragment.findPreference(disabledType.getTypeNameAsString());
                    Criteria.checkThat(
                            "Disabled empty category should NOT exist",
                            category,
                            Matchers.nullValue());
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_notRenderedIfReadOnlyAndEmpty() throws Exception {
        EntityType readOnlyType =
                TestUtils.getPassportEntityType(
                        /* isReadOnly= */ true,
                        /* isEnabled= */ true,
                        /* isEligibleForWalletStorage= */ false);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(readOnlyType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference category =
                            fragment.findPreference(readOnlyType.getTypeNameAsString());
                    Criteria.checkThat(
                            "ReadOnly empty category should NOT exist",
                            category,
                            Matchers.nullValue());
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_renderedIfDisabledButNotEmpty() throws Exception {
        EntityType disabledType =
                TestUtils.getVehicleEntityType(
                        /* isReadOnly= */ false,
                        /* isEnabled= */ false,
                        /* isEligibleForWalletStorage= */ false);

        EntityInstanceWithLabels entity =
                new EntityInstanceWithLabels(
                        "guid1",
                        disabledType,
                        "Label",
                        "Sublabel",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(disabledType, Arrays.asList(entity));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    Preference category =
                            fragment.findPreference(disabledType.getTypeNameAsString());
                    Criteria.checkThat(
                            "Disabled NOT empty category should exist",
                            category,
                            Matchers.notNullValue());
                    PreferenceGroup group = (PreferenceGroup) category;
                    assertNotNull(group.findPreference("guid1"));
                    assertNull(group.findPreference(disabledType.getTypeNameAsString() + " Add"));
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_sorting() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        vehicleType,
                        /* entityInstanceLabel= */ "B",
                        /* entityInstanceSubLabel= */ "2",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        EntityInstanceWithLabels entity2 =
                new EntityInstanceWithLabels(
                        "guid2",
                        vehicleType,
                        /* entityInstanceLabel= */ "A",
                        /* entityInstanceSubLabel= */ "1",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        EntityInstanceWithLabels entity3 =
                new EntityInstanceWithLabels(
                        "guid3",
                        vehicleType,
                        /* entityInstanceLabel= */ "A",
                        /* entityInstanceSubLabel= */ "2",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        // Sorting is now expected to be done by getInstancesToList.
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Arrays.asList(entity2, entity3, entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    PreferenceCategory category = fragment.findPreference("Vehicle");
                    Criteria.checkThat(
                            "Vehicle category should exist", category, Matchers.notNullValue());
                    Criteria.checkThat(
                            "Category should have 4 preferences (3 entities + 1 add button)",
                            category.getPreferenceCount(),
                            Matchers.is(4));
                    Criteria.checkThat(
                            "First entity should be guid2 (A1)",
                            category.getPreference(0).getKey(),
                            Matchers.is("guid2"));
                    Criteria.checkThat(
                            "Second entity should be guid3 (A2)",
                            category.getPreference(1).getKey(),
                            Matchers.is("guid3"));
                    Criteria.checkThat(
                            "Third entity should be guid1 (B2)",
                            category.getPreference(2).getKey(),
                            Matchers.is("guid1"));
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_opensEditorOnAddClickForLocalEntity() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
        when(mEntityDataManager.canEnableOrDisableAutofillAi()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference addVehicle =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PreferenceCategory category =
                                    mSettingsActivityTestRule
                                            .getFragment()
                                            .findPreference("Vehicle");
                            return category.findPreference("Vehicle" + " Add");
                        });
        assertNotNull(addVehicle);
        int callCount = rule.mClickUpdate.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(addVehicle::performClick);
        rule.mClickUpdate.waitForCallback(callCount);

        onView(withText("Add Vehicle")).check(matches(isDisplayed()));

        // Click the "Done" button.
        onView(withText("Done")).perform(click());
        verify(mEntityDataManager)
                .addOrUpdateEntityInstance(
                        any(),
                        eq(R.string.autofill_ai_save_or_update_local_entity_source_notice),
                        eq(R.string.done),
                        any());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_opensEditorOnAddClickForWalletEntity() throws Exception {
        EntityType vehicleType =
                TestUtils.getVehicleEntityType(
                        /* isReadOnly= */ false,
                        /* isEnabled= */ true,
                        /* isEligibleForWalletStorage= */ true);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
        when(mEntityDataManager.canEnableOrDisableAutofillAi()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference addVehicle =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PreferenceCategory category =
                                    mSettingsActivityTestRule
                                            .getFragment()
                                            .findPreference("Vehicle");
                            return category.findPreference("Vehicle" + " Add");
                        });
        assertNotNull(addVehicle);
        int callCount = rule.mClickUpdate.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(addVehicle::performClick);
        rule.mClickUpdate.waitForCallback(callCount);

        onView(withText("Add Vehicle")).check(matches(isDisplayed()));

        // Click the "Done" button and trigger the local save fallback snackbar. Verify that the
        // snackbar is displayed.
        onView(withText("Done")).perform(click());
        ArgumentCaptor<Runnable> localSaveFallbackCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mEntityDataManager)
                .addOrUpdateEntityInstance(
                        any(),
                        eq(R.string.autofill_ai_save_or_update_entity_in_wallet_source_notice),
                        eq(R.string.done),
                        localSaveFallbackCaptor.capture());

        ThreadUtils.runOnUiThreadBlocking(() -> localSaveFallbackCaptor.getValue().run());

        String snackbarMessage =
                mSettingsActivityTestRule
                        .getActivity()
                        .getString(
                                R.string
                                        .autofill_ai_save_or_update_entity_failed_wallet_save_dialog_title);
        waitForSnackbar(snackbarMessage);
    }

    /** Wait for the snackbar to show on the main activity post deletion. */
    private void waitForSnackbar(String expectedSnackbarMessage) {
        SettingsActivity activity = mSettingsActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    SnackbarManager snackbarManager = activity.getSnackbarManager();
                    Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(true));
                    TextView snackbarMessage = activity.findViewById(R.id.snackbar_message);
                    Criteria.checkThat(snackbarMessage, Matchers.notNullValue());
                    Criteria.checkThat(
                            snackbarMessage.getText().toString(),
                            Matchers.is(expectedSnackbarMessage));
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_opensEditorOnClick() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        vehicleType,
                        /* entityInstanceLabel= */ "Vehicle",
                        /* entityInstanceSubLabel= */ "Mercedez",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        EntityInstance entityInstance =
                new EntityInstance.Builder(vehicleType)
                        .setGUID("guid1")
                        .setRecordType(
                                org.chromium.components.autofill.autofill_ai.RecordType.LOCAL)
                        .setModifiedDate(LocalDate.of(2026, 2, 12))
                        .setUseCount(0)
                        .build();

        when(mEntityDataManager.getEntityInstance("guid1")).thenReturn(entityInstance);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference vehicleEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));
        assertNotNull(vehicleEntity);
        ThreadUtils.runOnUiThreadBlocking(vehicleEntity::performClick);

        onView(withText("Edit Vehicle")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)
    public void testAutofillAiEntities_opensEditorOnAddClick_eligibleForWalletFalse()
            throws Exception {
        EntityType vehicleType =
                TestUtils.getVehicleEntityType(
                        /* isReadOnly= */ false,
                        /* isEnabled= */ true,
                        /* isEligibleForWalletStorage= */ false);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.canEnableOrDisableAutofillAi()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference addVehicle =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PreferenceCategory category =
                                    mSettingsActivityTestRule
                                            .getFragment()
                                            .findPreference("Vehicle");
                            return category.findPreference("Vehicle" + " Add");
                        });
        assertNotNull(addVehicle);
        int callCount = rule.mClickUpdate.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(addVehicle::performClick);
        rule.mClickUpdate.waitForCallback(callCount);

        onView(withText("Add Vehicle")).check(matches(isDisplayed()));

        Context context = mSettingsActivityTestRule.getFragment().getContext();
        String expectedNoticeText =
                context.getString(R.string.autofill_ai_save_or_update_local_entity_source_notice);
        onView(withText(expectedNoticeText)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)
    public void testAutofillAiEntities_opensEditorOnAddClick_eligibleForWalletTrue()
            throws Exception {
        setUpMockPrimaryAccount(TestAccounts.ACCOUNT1);
        EntityType vehicleType =
                TestUtils.getVehicleEntityType(
                        /* isReadOnly= */ false,
                        /* isEnabled= */ true,
                        /* isEligibleForWalletStorage= */ true);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.canEnableOrDisableAutofillAi()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference addVehicle =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PreferenceCategory category =
                                    mSettingsActivityTestRule
                                            .getFragment()
                                            .findPreference("Vehicle");
                            return category.findPreference("Vehicle" + " Add");
                        });
        assertNotNull(addVehicle);
        int callCount = rule.mClickUpdate.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(addVehicle::performClick);
        rule.mClickUpdate.waitForCallback(callCount);

        onView(withText("Add Vehicle")).check(matches(isDisplayed()));

        Context context = mSettingsActivityTestRule.getFragment().getContext();
        String walletTitle = context.getString(R.string.autofill_google_wallet_title);
        String expectedNoticeText =
                context.getString(
                                R.string.autofill_ai_save_or_update_entity_in_wallet_source_notice)
                        .replace("$1", walletTitle)
                        .replace("$2", walletTitle)
                        .replace("$3", TestAccounts.ACCOUNT1.getEmail())
                        .replace("<link>", "")
                        .replace("</link>", "");
        onView(withText(expectedNoticeText)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_PRIVATE_PASSES_DEEP_LINK)
    public void testAutofillAiEntities_opensWalletOnClick() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();

        String expectedUrl = "https://wallet.com/private";

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        vehicleType,
                        /* entityInstanceLabel= */ "Vehicle",
                        /* entityInstanceSubLabel= */ "Mercedez",
                        /* storedInWallet= */ true,
                        /* walletEntityUrl= */ expectedUrl);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference vehicleEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));
        assertNotNull(vehicleEntity);

        // Mock the intent that should be fired.
        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, null);
        // Since we don't have Google Wallet installed in tests, it will fallback to CCT.
        var intentMatcher = allOf(hasAction(Intent.ACTION_VIEW), hasData(Uri.parse(expectedUrl)));
        intending(intentMatcher).respondWith(result);

        ThreadUtils.runOnUiThreadBlocking(vehicleEntity::performClick);

        intended(intentMatcher);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_PRIVATE_PASSES_DEEP_LINK)
    public void testAutofillAiEntities_opensWalletDefaultPage_whenUrlIsNull() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        vehicleType,
                        /* entityInstanceLabel= */ "Vehicle",
                        /* entityInstanceSubLabel= */ "Mercedez",
                        /* storedInWallet= */ true,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference vehicleEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));
        assertNotNull(vehicleEntity);

        // Mock the intent that should be fired.
        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, null);
        // Since the walletEntityUrl is null, it should fallback to the general passes page.
        var intentMatcher =
                allOf(
                        hasAction(Intent.ACTION_VIEW),
                        hasData(Uri.parse(GoogleWalletLauncher.GOOGLE_WALLET_PASSES_URL)));
        intending(intentMatcher).respondWith(result);

        ThreadUtils.runOnUiThreadBlocking(vehicleEntity::performClick);

        intended(intentMatcher);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_PRIVATE_PASSES_DEEP_LINK)
    public void testAutofillAiEntities_opensWalletPrivatePassPageOnClick() throws Exception {
        EntityType passportType = TestUtils.getPassportEntityType();

        String expectedUrl = "https://wallet.com/private";

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        passportType,
                        /* entityInstanceLabel= */ "Passport",
                        /* entityInstanceSubLabel= */ "Germany",
                        /* storedInWallet= */ true,
                        /* walletEntityUrl= */ expectedUrl);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference passportEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));
        assertNotNull(passportEntity);

        // Mock the intent that should be fired.
        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, null);
        var intentMatcher = allOf(hasAction(Intent.ACTION_VIEW), hasData(Uri.parse(expectedUrl)));
        intending(intentMatcher).respondWith(result);

        ThreadUtils.runOnUiThreadBlocking(passportEntity::performClick);

        intended(intentMatcher);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_PRIVATE_PASSES_DEEP_LINK)
    public void testAutofillAiEntities_opensWalletPrivatePassPageOnClick_featureDisabled()
            throws Exception {
        EntityType passportType = TestUtils.getPassportEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        passportType,
                        /* entityInstanceLabel= */ "Passport",
                        /* entityInstanceSubLabel= */ "Germany",
                        /* storedInWallet= */ true,
                        /* walletEntityUrl= */ "https://wallet.com/private");

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(passportType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list to pick up the new mock entities.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference passportEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));
        assertNotNull(passportEntity);

        // Mock the intent that should be fired.
        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, null);
        // Since the deep link feature is disabled, it should fallback to the general passes page.
        var intentMatcher =
                allOf(
                        hasAction(Intent.ACTION_VIEW),
                        hasData(Uri.parse(GoogleWalletLauncher.GOOGLE_WALLET_PASSES_URL)));
        intending(intentMatcher).respondWith(result);

        ThreadUtils.runOnUiThreadBlocking(passportEntity::performClick);

        intended(intentMatcher);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_rebuildsOnEntityChange() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();

        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        vehicleType,
                        /* entityInstanceLabel= */ "Vehicle",
                        /* entityInstanceSubLabel= */ "Mercedez",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap1 =
                new LinkedHashMap<>();
        instancesMap1.put(vehicleType, Arrays.asList(entity1));

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap1);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Capture the observer registered by the fragment.
        ArgumentCaptor<EntityDataManagerObserver> captor =
                ArgumentCaptor.forClass(EntityDataManagerObserver.class);
        verify(mEntityDataManager, atLeastOnce()).registerDataObserver(captor.capture());
        EntityDataManagerObserver observer = captor.getValue();

        // Initially check that the entity is rendered.
        ThreadUtils.runOnUiThreadBlocking(() -> observer.onEntityInstancesChanged());
        CriteriaHelper.pollUiThread(
                () -> {
                    Preference vehicleEntity =
                            mSettingsActivityTestRule.getFragment().findPreference("guid1");
                    Criteria.checkThat(
                            "Vehicle entity should exist", vehicleEntity, Matchers.notNullValue());
                });

        // Change the entities and notify the observer.
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap2 =
                new LinkedHashMap<>();
        instancesMap2.put(vehicleType, Collections.emptyList());
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap2);
        ThreadUtils.runOnUiThreadBlocking(() -> observer.onEntityInstancesChanged());

        // Verify that the entity is gone.
        CriteriaHelper.pollUiThread(
                () -> {
                    Preference vehicleEntity =
                            mSettingsActivityTestRule.getFragment().findPreference("guid1");
                    Criteria.checkThat(
                            "Vehicle entity should no longer exist",
                            vehicleEntity,
                            Matchers.nullValue());
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAddressSectionTitle_featureEnabled_showsTitle() throws Exception {
        AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
        Preference category = fragment.findPreference("autofill_section_title");
        assertNotNull(category);
        assertEquals(
                mSettingsActivityTestRule
                        .getActivity()
                        .getString(R.string.autofill_addresses_section_title),
                category.getTitle());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testAddressSectionTitle_featureDisabled_noTitle() throws Exception {
        AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
        Preference category = fragment.findPreference("autofill_section_title");
        assertNull(category);
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID})
    public void testTitle_HoTDisabled_showsAddresses() throws Exception {
        AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
        assertThat(fragment.getPageTitle().get())
                .isEqualTo(
                        mSettingsActivityTestRule
                                .getActivity()
                                .getString(R.string.autofill_addresses_settings_title));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID})
    public void testTitle_HoTEnabled_showsContactInfo() throws Exception {
        AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
        assertThat(fragment.getPageTitle().get())
                .isEqualTo(
                        mSettingsActivityTestRule
                                .getActivity()
                                .getString(R.string.autofill_contact_info_title));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_opensEditorOnSuccessfulReauth() throws Exception {
        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        TestUtils.getVehicleEntityType(),
                        /* entityInstanceLabel= */ "Vehicle",
                        /* entityInstanceSubLabel= */ "Mercedez",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(TestUtils.getVehicleEntityType(), Arrays.asList(entity1));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        EntityInstance entityInstance =
                new EntityInstance.Builder(TestUtils.getVehicleEntityType())
                        .setGUID("guid1")
                        .setRecordType(
                                org.chromium.components.autofill.autofill_ai.RecordType.LOCAL)
                        .setModifiedDate(LocalDate.of(2026, 2, 12))
                        .setUseCount(0)
                        .setRequiresReauthToSee(true)
                        .build();

        when(mEntityDataManager.getEntityInstance("guid1")).thenReturn(entityInstance);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference vehicleEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));

        // Click entity and capture reauth callback.
        ThreadUtils.runOnUiThreadBlocking(vehicleEntity::performClick);
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mMockReauthenticatorBridge).reauthenticate(callbackCaptor.capture());

        // Simulate successful reauth.
        ThreadUtils.runOnUiThreadBlocking(() -> callbackCaptor.getValue().onResult(true));

        onView(withText("Edit Vehicle")).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testAutofillAiEntities_doesNotOpenEditorOnFailedReauth() throws Exception {
        EntityInstanceWithLabels entity1 =
                new EntityInstanceWithLabels(
                        "guid1",
                        TestUtils.getVehicleEntityType(),
                        /* entityInstanceLabel= */ "Vehicle",
                        /* entityInstanceSubLabel= */ "Mercedez",
                        /* storedInWallet= */ false,
                        /* walletEntityUrl= */ null);

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(TestUtils.getVehicleEntityType(), Arrays.asList(entity1));
        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);

        EntityInstance entityInstance =
                new EntityInstance.Builder(TestUtils.getVehicleEntityType())
                        .setGUID("guid1")
                        .setRecordType(
                                org.chromium.components.autofill.autofill_ai.RecordType.LOCAL)
                        .setModifiedDate(LocalDate.of(2026, 2, 12))
                        .setUseCount(0)
                        .setRequiresReauthToSee(true)
                        .build();

        when(mEntityDataManager.getEntityInstance("guid1")).thenReturn(entityInstance);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Trigger a rebuild of the profile list.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        Preference vehicleEntity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mSettingsActivityTestRule.getFragment().findPreference("guid1"));

        // Click entity and capture reauth callback.
        ThreadUtils.runOnUiThreadBlocking(vehicleEntity::performClick);
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mMockReauthenticatorBridge).reauthenticate(callbackCaptor.capture());

        // Simulate failed reauth.
        ThreadUtils.runOnUiThreadBlocking(() -> callbackCaptor.getValue().onResult(false));

        // Editor should NOT be displayed.
        onView(withText("Edit Vehicle")).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)
    public void testAddEntityButton_disabledInThirdPartyMode() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.canEnableOrDisableAutofillAi()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        // Set third party mode.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    PreferenceCategory category = fragment.findPreference("Vehicle");
                    Criteria.checkThat(category, Matchers.notNullValue());
                    Preference addVehicle = category.findPreference("Vehicle" + " Add");
                    Criteria.checkThat(addVehicle, Matchers.notNullValue());
                    Criteria.checkThat(addVehicle.isEnabled(), Matchers.is(false));
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testOnOpenGoogleWallet_OpensWallet() {
        intending(hasAction(Intent.ACTION_VIEW))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSettingsActivityTestRule.getFragment().onOpenGoogleWalletForTesting(false);
                });

        intended(hasAction(Intent.ACTION_VIEW));
        intended(hasData(GoogleWalletLauncher.GOOGLE_WALLET_PASSES_URL));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testOnOpenGoogleWallet_OpensHelpCenterForPrivateEntity() {
        intending(hasAction(Intent.ACTION_VIEW))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSettingsActivityTestRule.getFragment().onOpenGoogleWalletForTesting(true);
                });

        intended(hasAction(Intent.ACTION_VIEW));
        intended(hasData(GoogleWalletLauncher.GOOGLE_WALLET_PRIVATE_PASSES_HELP_CENTER));
    }

    private void checkPreferenceCount(int expectedPreferenceCount) {
        int preferenceCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mSettingsActivityTestRule
                                        .getFragment()
                                        .getPreferenceScreen()
                                        .getPreferenceCount());
        assertEquals(expectedPreferenceCount, preferenceCount);
    }

    @Nullable
    private AutofillProfileEditorPreference findPreference(String title) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().findPreference(title));
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
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManagerMock);
        when(mIdentityManagerMock.getPrimaryAccountInfo()).thenReturn(accountInfo);
        when(mIdentityManagerMock.hasPrimaryAccount()).thenReturn(true);
    }

    private void setUpMockSyncService(Set<Integer> selectedTypes) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> SyncServiceFactory.setInstanceForTesting(mSyncService));
        when(mSyncService.getSelectedTypes()).thenReturn(selectedTypes);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)
    public void testAddEntityButton_defaultAvailabilityOn_enabledIfCanEnable() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.canEnableOrDisableAutofillAi()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    PreferenceCategory category = fragment.findPreference("Vehicle");
                    Criteria.checkThat(category, Matchers.notNullValue());
                    Preference addVehicle = category.findPreference("Vehicle" + " Add");
                    Criteria.checkThat(addVehicle, Matchers.notNullValue());
                    Criteria.checkThat(addVehicle.isEnabled(), Matchers.is(true));
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)
    public void testAddEntityButton_defaultAvailabilityOn_disabledIfCannotEnable()
            throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.canEnableOrDisableAutofillAi()).thenReturn(false);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    PreferenceCategory category = fragment.findPreference("Vehicle");
                    Criteria.checkThat(category, Matchers.notNullValue());
                    Preference addVehicle = category.findPreference("Vehicle" + " Add");
                    Criteria.checkThat(addVehicle, Matchers.notNullValue());
                    Criteria.checkThat(addVehicle.isEnabled(), Matchers.is(false));
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)
    public void testAddEntityButton_defaultAvailabilityOff_enabledIfEligibleAndOptedIn()
            throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    PreferenceCategory category = fragment.findPreference("Vehicle");
                    Criteria.checkThat(category, Matchers.notNullValue());
                    Preference addVehicle = category.findPreference("Vehicle" + " Add");
                    Criteria.checkThat(addVehicle, Matchers.notNullValue());
                    Criteria.checkThat(addVehicle.isEnabled(), Matchers.is(true));
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)
    public void testAddEntityButton_defaultAvailabilityOff_disabledIfNotEligible()
            throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(false);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    PreferenceCategory category = fragment.findPreference("Vehicle");
                    Criteria.checkThat(category, Matchers.notNullValue());
                    Preference addVehicle = category.findPreference("Vehicle" + " Add");
                    Criteria.checkThat(addVehicle, Matchers.notNullValue());
                    Criteria.checkThat(addVehicle.isEnabled(), Matchers.is(false));
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_AVAILABLE_BY_DEFAULT)
    public void testAddEntityButton_defaultAvailabilityOff_disabledIfNotOptedIn() throws Exception {
        EntityType vehicleType = TestUtils.getVehicleEntityType();
        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> instancesMap =
                new LinkedHashMap<>();
        instancesMap.put(vehicleType, Collections.emptyList());

        when(mEntityDataManager.getInstancesToList()).thenReturn(instancesMap);
        when(mEntityDataManager.isEligibleToAutofillAi()).thenReturn(true);
        when(mEntityDataManager.getAutofillAiOptInStatus()).thenReturn(false);
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSettingsActivityTestRule.getFragment().onPersonalDataChanged());

        CriteriaHelper.pollUiThread(
                () -> {
                    AutofillProfilesFragment fragment = mSettingsActivityTestRule.getFragment();
                    PreferenceCategory category = fragment.findPreference("Vehicle");
                    Criteria.checkThat(category, Matchers.notNullValue());
                    Preference addVehicle = category.findPreference("Vehicle" + " Add");
                    Criteria.checkThat(addVehicle, Matchers.notNullValue());
                    Criteria.checkThat(addVehicle.isEnabled(), Matchers.is(false));
                });
    }

    @Test
    @SmallTest
    public void testHelpMenuTriggersAutofillHelp() {
        onView(withId(R.id.menu_id_targeted_help)).perform(click());

        verify(mHelpAndFeedbackLauncher)
                .show(
                        mSettingsActivityTestRule.getActivity(),
                        ContextUtils.getApplicationContext()
                                .getString(R.string.help_context_autofill),
                        /* url= */ null);
    }
}
