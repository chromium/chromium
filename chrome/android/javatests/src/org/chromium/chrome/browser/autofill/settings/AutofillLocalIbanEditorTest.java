// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillLocalIbanEditorTest {
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<AutofillLocalIbanEditor> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillLocalIbanEditor.class);

    private AutofillTestHelper mAutofillTestHelper;

    private Bundle fragmentArgs(String guid) {
        Bundle args = new Bundle();
        args.putString(AutofillEditorBase.AUTOFILL_GUID, guid);
        return args;
    }

    private static final Iban VALID_BELGIUM_IBAN =
            new Iban.Builder()
                    .setGuid("")
                    .setNickname("My IBAN")
                    .setRecordType(IbanRecordType.UNKNOWN)
                    .setValue("BE71096123456769")
                    .build();

    @Before
    public void setUp() {
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalIbanEditorFragment.mNickname.setText(nickname);
                    } catch (Exception e) {
                        Assert.fail("Failed to set Nickname");
                    }
                });
    }

    private void setValueInEditor(
            AutofillLocalIbanEditor autofillLocalIbanEditorFragment, String value) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalIbanEditorFragment.mValue.setText(value);
                    } catch (Exception e) {
                        Assert.fail("Failed to set IBAN");
                    }
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
}
