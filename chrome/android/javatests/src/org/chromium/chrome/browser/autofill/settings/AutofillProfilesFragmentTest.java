// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.DialogInterface;
import android.view.KeyEvent;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.Source;
import org.chromium.chrome.browser.autofill.editors.EditorDialogView;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/**
 * Unit test suite for AutofillProfilesFragment.
 */

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ACCOUNT_PROFILE_STORAGE,
        ChromeFeatureList.SYNC_ENABLE_CONTACT_INFO_DATA_TYPE,
        ChromeFeatureList.SYNC_ENABLE_CONTACT_INFO_DATA_TYPE_IN_TRANSPORT_MODE})
public class AutofillProfilesFragmentTest {
    private static final AutofillProfile sLocalOrSyncProfile =
            AutofillProfile.builder()
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
                    .setSource(Source.ACCOUNT)
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

    @Rule
    public final AutofillTestRule rule = new AutofillTestRule();
    @ClassRule
    public static final SettingsActivityTestRule<AutofillProfilesFragment>
            sSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillProfilesFragment.class);
    @Rule
    public final TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    private IdentityManager mIdentityManagerMock;
    @Mock
    private SyncService mSyncService;

    private final AutofillTestHelper mHelper = new AutofillTestHelper();

    @BeforeClass
    public static void setUpClass() {
        sSettingsActivityTestRule.startSettingsActivity();
    }

    @Before
    public void setUp() throws TimeoutException {
        mHelper.setProfile(sLocalOrSyncProfile);
        mHelper.setProfile(AutofillProfile.builder()
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
        mHelper.setProfile(AutofillProfile.builder()
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
        mHelper.setProfile(AutofillProfile.builder()
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
        mHelper.clearAllDataForTesting();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES})
    public void testAddProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference addProfile =
                autofillProfileFragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        Assert.assertNotNull(addProfile);

        // Add a profile.
        updatePreferencesAndWait(autofillProfileFragment, addProfile,
                new String[] {"Ms.", "Alice Doe", "Google", "111 Added St", "Los Angeles", "CA",
                        "90291", "650-253-0000", "add@profile.com"},
                R.id.editor_dialog_done_button, false);

        Assert.assertEquals(7 /* One toggle + one add button + five profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference addedProfile =
                autofillProfileFragment.findPreference("Alice Doe");
        Assert.assertNotNull(addedProfile);
        Assert.assertEquals("111 Added St, 90291", addedProfile.getSummary());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES})
    public void testAddIncompletedProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference addProfile =
                autofillProfileFragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        Assert.assertNotNull(addProfile);

        // Add an incomplete profile.
        updatePreferencesAndWait(autofillProfileFragment, addProfile, new String[] {"", "Mike Doe"},
                R.id.editor_dialog_done_button, false);

        // Incomplete profile should still be added.
        Assert.assertEquals(7 /* One toggle + one add button + five profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference addedProfile =
                autofillProfileFragment.findPreference("Mike Doe");
        Assert.assertNotNull(addedProfile);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES})
    public void testAddProfileWithInvalidPhone() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference addProfile =
                autofillProfileFragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        Assert.assertNotNull(addProfile);

        // Try to add a profile with invalid phone.
        updatePreferencesAndWait(autofillProfileFragment, addProfile,
                new String[] {"", "", "", "", "", "", "", "123"}, R.id.editor_dialog_done_button,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDeleteLocalProfile() throws Exception {
        Context context = sSettingsActivityTestRule.getFragment().getContext();
        setUpMockSyncService(false, new HashSet());
        testDeleteProfile(context.getString(R.string.autofill_delete_local_address_source_notice));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDeleteSyncableProfile() throws Exception {
        Context context = sSettingsActivityTestRule.getFragment().getContext();
        setUpMockSyncService(true, Collections.singleton(UserSelectableType.AUTOFILL));
        testDeleteProfile(context.getString(R.string.autofill_delete_sync_address_source_notice));
    }

    public void testDeleteProfile(String expectedConfirmationMessage) throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();
        Context context = autofillProfileFragment.getContext();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference sebProfile =
                autofillProfileFragment.findPreference("Seb Doe");
        Assert.assertNotNull(sebProfile);
        Assert.assertEquals("Seb Doe", sebProfile.getTitle());

        // Delete the profile, but cancel on confirmation.
        TestThreadUtils.runOnUiThreadBlocking(sebProfile::performClick);
        EditorDialogView editorDialog = autofillProfileFragment.getEditorDialogForTest();
        rule.setEditorDialogAndWait(editorDialog);
        rule.clickInEditorAndWaitForConfirmationDialog(R.id.delete_menu_id);

        // Verify the confirmation message for non-account profile.
        AlertDialog confirmationDialog = editorDialog.getConfirmationDialogForTest();
        Assert.assertNotNull(confirmationDialog);
        TextView messageView = confirmationDialog.findViewById(R.id.confirmation_dialog_message);
        Assert.assertEquals(expectedConfirmationMessage, messageView.getText());

        // Get back to the profile list.
        rule.clickInConfirmationDialogAndWait(DialogInterface.BUTTON_NEGATIVE);
        rule.clickInEditorAndWait(R.id.payments_edit_cancel_button);

        // Make sure the profile is not deleted and the number of profiles didn't change.
        Assert.assertEquals(6 /* One toggle + one add button + four profile. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());

        // Delete a profile and confirm it.
        TestThreadUtils.runOnUiThreadBlocking(sebProfile::performClick);
        rule.setEditorDialogAndWait(autofillProfileFragment.getEditorDialogForTest());
        rule.clickInEditorAndWaitForConfirmationDialog(R.id.delete_menu_id);
        rule.clickInConfirmationDialogAndWait(DialogInterface.BUTTON_POSITIVE);
        rule.waitForThePreferenceUpdate();

        // Make sure the profile is deleted.
        Assert.assertEquals(5 /* One toggle + one add button + three profile. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference remainedProfile =
                autofillProfileFragment.findPreference("John Doe");
        Assert.assertNotNull(remainedProfile);
        AutofillProfileEditorPreference deletedProfile =
                autofillProfileFragment.findPreference("Seb Doe");
        Assert.assertNull(deletedProfile);
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
        Assert.assertEquals(7 /* One toggle + one add button + five profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference artikProfile =
                autofillProfileFragment.findPreference("Artik Doe");
        Assert.assertNotNull(artikProfile);

        // Delete Artik's account profile.
        TestThreadUtils.runOnUiThreadBlocking(artikProfile::performClick);
        EditorDialogView editorDialog = autofillProfileFragment.getEditorDialogForTest();
        rule.setEditorDialogAndWait(editorDialog);
        rule.clickInEditorAndWaitForConfirmationDialog(R.id.delete_menu_id);

        // Verify the message.
        AlertDialog confirmationDialog = editorDialog.getConfirmationDialogForTest();
        Assert.assertNotNull(confirmationDialog);
        TextView messageView = confirmationDialog.findViewById(R.id.confirmation_dialog_message);
        String expectedMessage =
                context.getString(R.string.autofill_delete_account_address_source_notice)
                        .replace("$1", email);
        Assert.assertEquals(expectedMessage, messageView.getText());

        rule.clickInConfirmationDialogAndWait(DialogInterface.BUTTON_POSITIVE);
        rule.waitForThePreferenceUpdate();

        // Make sure the profile is deleted.
        Assert.assertEquals(6 /* One toggle + one add button + 5 profile. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference deletedProfile =
                autofillProfileFragment.findPreference("Artik Doe");
        Assert.assertNull(deletedProfile);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES})
    public void testEditProfile() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference johnProfile =
                autofillProfileFragment.findPreference("John Doe");
        Assert.assertNotNull(johnProfile);
        Assert.assertEquals("John Doe", johnProfile.getTitle());

        // Edit a profile.
        TestThreadUtils.runOnUiThreadBlocking(johnProfile::performClick);
        EditorDialogView editorDialog = autofillProfileFragment.getEditorDialogForTest();
        rule.setEditorDialogAndWait(editorDialog);
        rule.setTextInEditorAndWait(new String[] {"Dr.", "Emily Doe", "Google", "111 Edited St",
                "Los Angeles", "CA", "90291", "650-253-0000", "edit@profile.com"});

        // Verify the absence of the profile source notice.
        TextView footerMessage = editorDialog.findViewById(R.id.footer_message);
        Assert.assertEquals(View.GONE, footerMessage.getVisibility());

        rule.clickInEditorAndWait(R.id.editor_dialog_done_button);
        rule.waitForThePreferenceUpdate();

        // Check if the preferences are updated correctly.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference editedProfile =
                autofillProfileFragment.findPreference("Emily Doe");
        Assert.assertNotNull(editedProfile);
        Assert.assertEquals("111 Edited St, 90291", editedProfile.getSummary());
        AutofillProfileEditorPreference oldProfile =
                autofillProfileFragment.findPreference("John Doe");
        Assert.assertNull(oldProfile);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES})
    public void testEditAccountProfile() throws Exception {
        String email = "test@account";
        setUpMockPrimaryAccount(email);

        mHelper.setProfile(AutofillProfile.builder()
                                   .setSource(Source.ACCOUNT)
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
        Assert.assertEquals(7 /* One toggle + one add button + 5 profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference johnProfile =
                autofillProfileFragment.findPreference("Account Updated #0");
        Assert.assertNotNull(johnProfile);

        TestThreadUtils.runOnUiThreadBlocking(johnProfile::performClick);
        EditorDialogView editorDialog = autofillProfileFragment.getEditorDialogForTest();
        rule.setEditorDialogAndWait(editorDialog);

        // Verify the profile source notice.
        TextView footerMessage = editorDialog.findViewById(R.id.footer_message);
        Assert.assertEquals(View.VISIBLE, footerMessage.getVisibility());
        String expectedMessage =
                context.getString(R.string.autofill_address_already_saved_in_account_source_notice)
                        .replace("$1", email);
        Assert.assertEquals(expectedMessage, footerMessage.getText());

        // Invalid input.
        rule.setTextInEditorAndWait(new String[] {"Dr.", "Account Updated #1", "Google",
                "" /* Street address is required. */, "Los Angeles", "CA", "90291", "650-253-0000",
                "edit@profile.com"});
        rule.clickInEditorAndWaitForValidationError(R.id.editor_dialog_done_button);

        // Fix invalid input.
        rule.setTextInEditorAndWait(new String[] {"Dr.", "Account Updated #2", "Google",
                "222 Fourth St" /* Enter street address. */, "Los Angeles", "CA", "90291",
                "650-253-0000", "edit@profile.com"});
        rule.clickInEditorAndWait(R.id.editor_dialog_done_button);
        rule.waitForThePreferenceUpdate();

        // Check if the preferences are updated correctly.
        Assert.assertEquals(7 /* One toggle + one add button + five profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference editedProfile =
                autofillProfileFragment.findPreference("Account Updated #2");
        Assert.assertNotNull(editedProfile);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES})
    public void testEditInvalidAccountProfile() throws Exception {
        mHelper.setProfile(
                AutofillProfile.builder()
                        .setSource(Source.ACCOUNT)
                        .setFullName("Account Updated #0")
                        .setCompanyName("Google")
                        .setStreetAddress(
                                "") /** Street address is required in US but already missing. */
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
        Assert.assertEquals(7 /* One toggle + one add button + 5 profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference johnProfile =
                autofillProfileFragment.findPreference("Account Updated #0");
        Assert.assertNotNull(johnProfile);

        // Edit profile.
        updatePreferencesAndWait(autofillProfileFragment, johnProfile,
                new String[] {"Dr.", "Account Updated #1", "Google",
                        "" /* Dont fix missing Street address. */, "Los Angeles", "CA", "90291",
                        "650-253-0000", "edit@profile.com"},
                R.id.editor_dialog_done_button, false);
        // Check if the preferences are updated correctly.
        Assert.assertEquals(7 /* One toggle + one add button + five profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference editedProfile =
                autofillProfileFragment.findPreference("Account Updated #1");
        Assert.assertNotNull(editedProfile);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testOpenProfileWithCompleteState() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference bobProfile =
                autofillProfileFragment.findPreference("Bob Doe");
        Assert.assertNotNull(bobProfile);
        Assert.assertEquals("Bob Doe", bobProfile.getTitle());

        // Open the profile.
        updatePreferencesAndWait(
                autofillProfileFragment, bobProfile, null, R.id.editor_dialog_done_button, false);

        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testOpenProfileWithInvalidState() throws Exception {
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Check the preferences on the initial screen.
        Assert.assertEquals(6 /* One toggle + one add button + four profiles. */,
                autofillProfileFragment.getPreferenceScreen().getPreferenceCount());
        AutofillProfileEditorPreference billProfile =
                autofillProfileFragment.findPreference("Bill Doe");
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
        AutofillProfilesFragment fragment = sSettingsActivityTestRule.getFragment();
        AutofillProfileEditorPreference addProfile =
                fragment.findPreference(AutofillProfilesFragment.PREF_NEW_PROFILE);
        Assert.assertNotNull(addProfile);

        // Open AutofillProfileEditorPreference.
        TestThreadUtils.runOnUiThreadBlocking(addProfile::performClick);
        rule.setEditorDialogAndWait(fragment.getEditorDialogForTest());
        // The keyboard is shown as soon as AutofillProfileEditorPreference comes into view.
        waitForKeyboardStatus(true, sSettingsActivityTestRule.getActivity());

        final List<EditText> fields =
                fragment.getEditorDialogForTest().getEditableTextFieldsForTest();
        // Ensure the first text field is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> { fields.get(0).requestFocus(); });
        // Hide the keyboard.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
        rule.clickInEditorAndWait(R.id.payments_edit_cancel_button);
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
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);
        Assert.assertEquals(0,
                autofillProfileFragment.findPreference(sAccountProfile.getFullName())
                        .getWidgetLayoutResource());
        Assert.assertEquals(0,
                autofillProfileFragment.findPreference(sLocalOrSyncProfile.getFullName())
                        .getWidgetLayoutResource());
    }

    /**
     * Cloud off icons are shown conditionally depending on the 3 feature flags
     * being turned on.
     */
    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ACCOUNT_PROFILE_STORAGE,
            ChromeFeatureList.SYNC_ENABLE_CONTACT_INFO_DATA_TYPE,
            ChromeFeatureList.SYNC_ENABLE_CONTACT_INFO_DATA_TYPE_IN_TRANSPORT_MODE})
    public void
    testLocalProfiles_NoRequiredFeatureFlags() throws Exception {
        setUpMockPrimaryAccount("test@account.com");
        setUpMockSyncService(false, new HashSet());
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);
        Assert.assertEquals(0,
                autofillProfileFragment.findPreference(sAccountProfile.getFullName())
                        .getWidgetLayoutResource());
        Assert.assertEquals(0,
                autofillProfileFragment.findPreference(sLocalOrSyncProfile.getFullName())
                        .getWidgetLayoutResource());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalProfiles_NoSync() throws Exception {
        setUpMockPrimaryAccount("test@account.com");
        setUpMockSyncService(false, new HashSet());
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);
        Assert.assertEquals(0,
                autofillProfileFragment.findPreference(sAccountProfile.getFullName())
                        .getWidgetLayoutResource());
        Assert.assertEquals(R.layout.autofill_local_profile_icon,
                autofillProfileFragment.findPreference(sLocalOrSyncProfile.getFullName())
                        .getWidgetLayoutResource());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalProfiles_AddressesNotSynced() throws Exception {
        setUpMockPrimaryAccount("test@account.com");
        setUpMockSyncService(true, new HashSet());
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);
        Assert.assertEquals(0,
                autofillProfileFragment.findPreference(sAccountProfile.getFullName())
                        .getWidgetLayoutResource());
        Assert.assertEquals(R.layout.autofill_local_profile_icon,
                autofillProfileFragment.findPreference(sLocalOrSyncProfile.getFullName())
                        .getWidgetLayoutResource());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalProfiles_AddressesSynced() throws Exception {
        setUpMockPrimaryAccount("test@account.com");
        setUpMockSyncService(true, Collections.singleton(UserSelectableType.AUTOFILL));
        AutofillProfilesFragment autofillProfileFragment = sSettingsActivityTestRule.getFragment();

        // Trigger address profile list rebuild.
        mHelper.setProfile(sAccountProfile);
        Assert.assertEquals(0,
                autofillProfileFragment.findPreference(sAccountProfile.getFullName())
                        .getWidgetLayoutResource());
        Assert.assertEquals(0,
                autofillProfileFragment.findPreference(sLocalOrSyncProfile.getFullName())
                        .getWidgetLayoutResource());
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
            rule.clickInEditorAndWait(R.id.payments_edit_cancel_button);
        } else {
            rule.clickInEditorAndWait(buttonId);
            rule.waitForThePreferenceUpdate();
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> SyncServiceFactory.overrideForTests(mSyncService));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(enabled);
        when(mSyncService.getSelectedTypes()).thenReturn(selectedTypes);
    }
}
