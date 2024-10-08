// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.MenuItem;
import android.widget.Button;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

// TODO(b/309163597): Add Robolectric tests to test the local editor behavior.
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillLocalIbanEditorTest {
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<AutofillLocalIbanEditor> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillLocalIbanEditor.class);

    @Mock private ObservableSupplierImpl<ModalDialogManager> mModalDialogManagerSupplierMock;
    @Mock private PersonalDataManager mPersonalDataManagerMock;
    private AutofillTestHelper mAutofillTestHelper;

    private Bundle fragmentArgs(String guid) {
        Bundle args = new Bundle();
        args.putString(AutofillEditorBase.AUTOFILL_GUID, guid);
        return args;
    }

    private static final Iban VALID_BELGIUM_IBAN =
            new Iban.Builder()
                    .setLabel("")
                    .setNickname("My IBAN")
                    .setRecordType(IbanRecordType.UNKNOWN)
                    .setValue("BE71096123456769")
                    .build();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mAutofillTestHelper = new AutofillTestHelper();
    }

    private AutofillLocalIbanEditor setUpDefaultAutofillLocalIbanEditorFragment() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();
        assertThat(autofillLocalIbanEditorFragment.mDoneButton.isEnabled()).isFalse();
        return autofillLocalIbanEditorFragment;
    }

    private void setNicknameInEditor(
            AutofillLocalIbanEditor autofillLocalIbanEditorFragment, String nickname) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalIbanEditorFragment.mNickname.setText(nickname);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set Nickname", e);
                    }
                });
    }

    private void setValueInEditor(
            AutofillLocalIbanEditor autofillLocalIbanEditorFragment, String value) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalIbanEditorFragment.mValue.setText(value);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set IBAN", e);
                    }
                });
    }

    private void performButtonClick(Button button) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        button.performClick();
                    } catch (Exception e) {
                        throw new AssertionError("Failed to click the button", e);
                    }
                });
    }

    private void openDeletePaymentMethodConfirmationDialog(
            AutofillLocalIbanEditor autofillLocalIbanEditorFragment,
            ModalDialogManager modalDialogManager) {
        when(mModalDialogManagerSupplierMock.get()).thenReturn(modalDialogManager);
        autofillLocalIbanEditorFragment.setModalDialogManagerSupplier(
                mModalDialogManagerSupplierMock);

        MenuItem deleteButton = mock(MenuItem.class);
        when(deleteButton.getItemId()).thenReturn(R.id.delete_menu_id);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    autofillLocalIbanEditorFragment.onOptionsItemSelected(deleteButton);
                });
    }

    @Test
    @MediumTest
    public void testValidIbanValueEnablesSaveButton() throws Exception {
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                setUpDefaultAutofillLocalIbanEditorFragment();

        // Valid Russia IBAN value.
        setValueInEditor(
                autofillLocalIbanEditorFragment, /* value= */ "RU0204452560040702810412345678901");

        assertThat(autofillLocalIbanEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    public void testInvalidIbanValueDoesNotEnableSaveButton() throws Exception {
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                setUpDefaultAutofillLocalIbanEditorFragment();

        // Invalid Russia IBAN value.
        setValueInEditor(
                autofillLocalIbanEditorFragment, /* value= */ "RU0204452560040702810412345678902");

        assertThat(autofillLocalIbanEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void testEditIban_whenIbanIsNotEdited_keepsSaveButtonDisabled() throws Exception {
        String guid = mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();

        assertThat(autofillLocalIbanEditorFragment.mNickname.getText().toString())
                .isEqualTo("My IBAN");
        assertThat(autofillLocalIbanEditorFragment.mValue.getText().toString())
                .isEqualTo("BE71096123456769");
        // If neither the value nor the nickname is modified, the Done button should remain
        // disabled.
        assertThat(autofillLocalIbanEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void testEditIban_whenIbanValueIsEditedFromValidToInvalid_disablesSaveButton()
            throws Exception {
        String guid = mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();

        // Change IBAN value from valid to invalid.
        setValueInEditor(autofillLocalIbanEditorFragment, /* value= */ "BE710961234567600");

        assertThat(autofillLocalIbanEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void testEditIban_whenIbanValueIsEditedToAnotherValidValue_enablesSaveButton()
            throws Exception {
        String guid = mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();

        setValueInEditor(
                autofillLocalIbanEditorFragment, /* value= */ "GB82 WEST 1234 5698 7654 32");

        assertThat(autofillLocalIbanEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    public void testEditIban_whenIbanNicknameIsEdited_enablesSaveButton() throws Exception {
        String guid = mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();

        setNicknameInEditor(autofillLocalIbanEditorFragment, /* nickname= */ "My doctor's IBAN");

        assertThat(autofillLocalIbanEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    public void deleteIbanConfirmationDialog_deleteEntryCanceled_dialogDismissed()
            throws Exception {
        String guid = mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();

        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManagerMock);

        FakeModalDialogManager fakeModalDialogManager =
                new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        openDeletePaymentMethodConfirmationDialog(
                autofillLocalIbanEditorFragment, fakeModalDialogManager);

        // Verify the dialog is open.
        Assert.assertNotNull(fakeModalDialogManager.getShownDialogModel());
        ThreadUtils.runOnUiThreadBlocking(() -> fakeModalDialogManager.clickNegativeButton());
        // Verify the dialog is closed.
        Assert.assertNull(fakeModalDialogManager.getShownDialogModel());
        // Verify the IBAN entry is not deleted.
        verify(mPersonalDataManagerMock, never()).deleteIban(guid);
    }

    @Test
    @MediumTest
    public void deleteIbanConfirmationDialog_deleteEntryConfirmed_dialogDismissedAndEntryDeleted()
            throws Exception {
        String guid = mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();

        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManagerMock);

        FakeModalDialogManager fakeModalDialogManager =
                new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        openDeletePaymentMethodConfirmationDialog(
                autofillLocalIbanEditorFragment, fakeModalDialogManager);

        // Verify the dialog is open.
        Assert.assertNotNull(fakeModalDialogManager.getShownDialogModel());
        ThreadUtils.runOnUiThreadBlocking(() -> fakeModalDialogManager.clickPositiveButton());
        // Verify the dialog is closed.
        Assert.assertNull(fakeModalDialogManager.getShownDialogModel());
        // Verify the IBAN entry is deleted.
        verify(mPersonalDataManagerMock, times(1)).deleteIban(guid);
    }

    @Test
    @MediumTest
    public void testHelpButtonShown() throws Exception {
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(VALID_BELGIUM_IBAN.getGuid()));

        onView(withId(R.id.help_menu_id)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenNewIbanIsAddedWithNickname() throws Exception {
        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_ADDED_WITH_NICKNAME);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();
        setValueInEditor(autofillLocalIbanEditorFragment, "BE71096123456769");
        setNicknameInEditor(autofillLocalIbanEditorFragment, "My IBAN");

        performButtonClick(autofillLocalIbanEditorFragment.mDoneButton);

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenNewIbanIsAddedWithoutNickname() throws Exception {
        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_ADDED_WITHOUT_NICKNAME);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();
        setValueInEditor(autofillLocalIbanEditorFragment, "BE71096123456769");
        performButtonClick(autofillLocalIbanEditorFragment.mDoneButton);

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenIbanIsDeleted() throws Exception {
        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_DELETED);

        String guid = mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();

        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManagerMock);

        FakeModalDialogManager fakeModalDialogManager =
                new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        openDeletePaymentMethodConfirmationDialog(
                autofillLocalIbanEditorFragment, fakeModalDialogManager);
        ThreadUtils.runOnUiThreadBlocking(() -> fakeModalDialogManager.clickPositiveButton());

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenEditorIsClosedAfterEditingIbanNickname() throws Exception {
        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_EDITOR_CLOSED_WITH_CHANGES);

        String guid = mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();
        setNicknameInEditor(autofillLocalIbanEditorFragment, "My new IBAN");
        performButtonClick(autofillLocalIbanEditorFragment.mDoneButton);

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenEditorIsClosedAfterEditingIbanValue() throws Exception {
        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_EDITOR_CLOSED_WITH_CHANGES);

        String guid = mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();
        setValueInEditor(autofillLocalIbanEditorFragment, "RU0204452560040702810412345678901");
        performButtonClick(autofillLocalIbanEditorFragment.mDoneButton);

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenEditorIsClosedWithoutEditingIban() throws Exception {
        HistogramWatcher settingsPageIbanActionHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillLocalIbanEditor.SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                        AutofillLocalIbanEditor.IbanAction.IBAN_EDITOR_CLOSED_WITHOUT_CHANGES);

        String guid = mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_IBAN);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();
        performButtonClick(autofillLocalIbanEditorFragment.mDoneButton);

        settingsPageIbanActionHistogramWatcher.assertExpected();
    }
}
