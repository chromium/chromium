// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.os.Bundle;
import android.view.MenuItem;
import android.widget.Button;
import android.widget.EditText;

import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerUtilsJni;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsIntentUtil;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link AutofillLocalIbanEditor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutofillLocalIbanEditorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mMockProfile;
    @Mock private ChromeBrowserInitializer mMockInitializer;
    @Mock private PersonalDataManager mMockPersonalDataManager;
    @Mock private ProfileManagerUtilsJni mMockProfileManagerUtilsJni;

    private ActivityScenario<SettingsActivity> mActivityScenario;
    private SettingsActivity mSettingsActivity;
    private AutofillLocalIbanEditor mIbanEditor;

    private Button mDoneButton;
    private EditText mNickname;
    private EditText mValue;

    private static final Iban VALID_BELGIUM_IBAN =
            new Iban.Builder()
                    .setLabel("")
                    .setNickname("My IBAN")
                    .setRecordType(IbanRecordType.UNKNOWN)
                    .setValue("BE71096123456769")
                    .build();

    @Before
    public void setUp() {
        PersonalDataManagerFactory.setInstanceForTesting(mMockPersonalDataManager);
        ChromeBrowserInitializer.setForTesting(mMockInitializer);
        ProfileManagerUtilsJni.setInstanceForTesting(mMockProfileManagerUtilsJni);
        ProfileManager.setLastUsedProfileForTesting(mMockProfile);

        // Mock successful IBAN creation.
        when(mMockPersonalDataManager.addOrUpdateLocalIban(any())).thenReturn("123");

        // Mock isValidIban to return true for specific test IBANs.
        when(mMockPersonalDataManager.isValidIban(anyString()))
                .thenAnswer(
                        new Answer<Boolean>() {
                            @Override
                            public Boolean answer(InvocationOnMock invocation) {
                                List<String> validIbans =
                                        Arrays.asList(
                                                "BE71096123456769",
                                                "RU0204452560040702810412345678901",
                                                "GB82 WEST 1234 5698 7654 32");

                                String iban = invocation.getArgument(0);
                                return validIbans.contains(iban);
                            }
                        });
    }

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
        }
    }

    /**
     * Initializes the AutofillLocalIbanEditor fragment for testing.
     *
     * @param useDefaultIban If true, uses a default IBAN and sets up a mock response.
     */
    private void initFragment(boolean useDefaultIban) {
        Bundle arguments = new Bundle();
        if (useDefaultIban) {
            // Set the "guid" argument to be a non-null value.
            arguments.putString("guid", "");
            when(mMockPersonalDataManager.getIban("")).thenReturn(VALID_BELGIUM_IBAN);
        }

        Intent intent =
                SettingsIntentUtil.createIntent(
                        ContextUtils.getApplicationContext(),
                        AutofillLocalIbanEditor.class.getName(),
                        arguments);

        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(
                activity -> {
                    mSettingsActivity = activity;
                    mSettingsActivity.setTheme(R.style.Theme_MaterialComponents);
                });

        // Retrieve the main fragment and tested UI elements for test access.
        mIbanEditor = (AutofillLocalIbanEditor) mSettingsActivity.getMainFragment();

        mDoneButton = mSettingsActivity.findViewById(R.id.button_primary);
        mNickname = mSettingsActivity.findViewById(R.id.iban_nickname_edit);
        mValue = mSettingsActivity.findViewById(R.id.iban_value_edit);
    }

    private void openDeletePaymentMethodConfirmationDialog(ModalDialogManager modalDialogManager) {
        mIbanEditor.setModalDialogManagerSupplier(() -> modalDialogManager);
        MenuItem deleteButton = mock(MenuItem.class);
        when(deleteButton.getItemId()).thenReturn(R.id.delete_menu_id);
        mIbanEditor.onOptionsItemSelected(deleteButton);
    }

    @Test
    @MediumTest
    public void testValidIbanValueEnablesSaveButton() {
        initFragment(/* useDefaultIban= */ true);

        // Valid Russia IBAN value.
        mValue.setText(/* value= */ "RU0204452560040702810412345678901");
        assertTrue(mDoneButton.isEnabled());
    }

    @Test
    @MediumTest
    public void testInvalidIbanValueDoesNotEnableSaveButton() {
        initFragment(/* useDefaultIban= */ true);

        // Invalid Russia IBAN value.
        mValue.setText(/* value= */ "RU0204452560040702810412345678902");
        assertFalse(mDoneButton.isEnabled());
    }

    @Test
    @MediumTest
    public void testEditIban_whenIbanIsNotEdited_keepsSaveButtonDisabled() {
        initFragment(/* useDefaultIban= */ true);

        assertThat(mNickname.getText().toString()).isEqualTo("My IBAN");
        assertThat(mValue.getText().toString()).isEqualTo("BE71096123456769");
        // If neither the value nor the nickname is modified, the Done button should remain
        // disabled.
        assertFalse(mDoneButton.isEnabled());
    }

    @Test
    @MediumTest
    public void testEditIban_whenIbanValueIsEditedFromValidToInvalid_disablesSaveButton() {
        initFragment(/* useDefaultIban= */ true);

        assertFalse(mDoneButton.isEnabled());
        // Change IBAN value from valid to invalid.
        mValue.setText(/* value= */ "BE710961234567600");
        assertFalse(mDoneButton.isEnabled());
    }

    @Test
    @MediumTest
    public void testEditIban_whenIbanValueIsEditedToAnotherValidValue_enablesSaveButton() {
        initFragment(/* useDefaultIban= */ true);

        assertFalse(mDoneButton.isEnabled());
        // Change IBAN value from valid to valid.
        mValue.setText(/* value= */ "GB82 WEST 1234 5698 7654 32");
        assertTrue(mDoneButton.isEnabled());
    }

    @Test
    @MediumTest
    public void testEditIban_whenIbanNicknameIsEdited_enablesSaveButton() {
        initFragment(/* useDefaultIban= */ true);

        assertFalse(mDoneButton.isEnabled());
        mNickname.setText(/* nickname= */ "My doctor's IBAN");
        assertTrue(mDoneButton.isEnabled());
    }

    @Test
    @MediumTest
    public void deleteIbanConfirmationDialog_deleteEntryCanceled_dialogDismissed() {
        initFragment(/* useDefaultIban= */ true);
        FakeModalDialogManager fakeModalDialogManager =
                new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        openDeletePaymentMethodConfirmationDialog(fakeModalDialogManager);

        // Verify the dialog is open.
        assertNotNull(fakeModalDialogManager.getShownDialogModel());
        fakeModalDialogManager.clickNegativeButton();

        // Verify the dialog is closed.
        assertNull(fakeModalDialogManager.getShownDialogModel());

        // Verify the IBAN entry is not deleted.
        verify(mMockPersonalDataManager, never()).deleteIban("");
    }

    @Test
    @MediumTest
    public void deleteIbanConfirmationDialog_deleteEntryConfirmed_dialogDismissedAndEntryDeleted() {
        initFragment(/* useDefaultIban= */ true);
        FakeModalDialogManager fakeModalDialogManager =
                new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        openDeletePaymentMethodConfirmationDialog(fakeModalDialogManager);

        // Verify the dialog is open.
        assertNotNull(fakeModalDialogManager.getShownDialogModel());
        fakeModalDialogManager.clickPositiveButton();

        // Verify the dialog is closed.
        assertNull(fakeModalDialogManager.getShownDialogModel());

        // Verify the IBAN entry is deleted.
        verify(mMockPersonalDataManager).deleteIban("");
    }

    @Test
    @MediumTest
    public void testHelpButtonShown() {
        initFragment(/* useDefaultIban= */ true);
        onView(withId(R.id.help_menu_id)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenNewIbanIsAddedWithNickname() {
        // Default IBAN is not initialized.
        initFragment(/* useDefaultIban= */ false);
        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_ADDED_WITH_NICKNAME);

        // IBAN and nickname provided.
        mValue.setText("BE71096123456769");
        mNickname.setText("My IBAN");

        mDoneButton.performClick();

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenNewIbanIsAddedWithoutNickname() {
        // Default IBAN is not initialized.
        initFragment(/* useDefaultIban= */ false);

        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_ADDED_WITHOUT_NICKNAME);

        // Only IBAN provided.
        mValue.setText("BE71096123456769");

        mDoneButton.performClick();

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenIbanIsDeleted() {
        initFragment(/* useDefaultIban= */ true);

        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_DELETED);

        FakeModalDialogManager fakeModalDialogManager =
                new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        openDeletePaymentMethodConfirmationDialog(fakeModalDialogManager);

        fakeModalDialogManager.clickPositiveButton();

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenEditorIsClosedAfterEditingIbanNickname() {
        initFragment(/* useDefaultIban= */ true);

        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_EDITOR_CLOSED_WITH_CHANGES);

        mNickname.setText("My new IBAN");
        mDoneButton.performClick();

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenEditorIsClosedAfterEditingIbanValue() {
        initFragment(/* useDefaultIban= */ true);

        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_EDITOR_CLOSED_WITH_CHANGES);

        mValue.setText("RU0204452560040702810412345678901");
        mDoneButton.performClick();

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenEditorIsClosedWithoutEditingIban() {
        initFragment(/* useDefaultIban= */ true);

        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_EDITOR_CLOSED_WITHOUT_CHANGES);

        mDoneButton.performClick();

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }
}
