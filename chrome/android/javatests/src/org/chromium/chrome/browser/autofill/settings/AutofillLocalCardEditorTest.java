// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import android.os.Bundle;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Instrumentation tests for AutofillLocalCardEditor.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillLocalCardEditorTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final AutofillTestRule rule = new AutofillTestRule();
    @Rule
    public final SettingsActivityTestRule<AutofillLocalCardEditor> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillLocalCardEditor.class);

    private static final CreditCard SAMPLE_LOCAL_CARD =
            new CreditCard(/* guid= */ "", /* origin= */ "",
                    /* isLocal= */ true, /* isCached= */ false, /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork = */ "visa",
                    /* issuerIconDrawableId= */ 0, /* billingAddressId= */ "", /* serverId= */ "");

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
    }

    @Test
    @MediumTest
    public void nicknameFieldEmpty_cardDoesNotHaveNickname() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameText.getText().toString()).isEmpty();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void nicknameFieldSet_cardHasNickname() throws Exception {
        String nickname = "test nickname";
        SAMPLE_LOCAL_CARD.setNickname(nickname);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameText.getText().toString())
                .isEqualTo(nickname);
        // If the nickname is not modified the Done button should be disabled.
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void testNicknameFieldIsShown() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getVisibility())
                .isEqualTo(View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testInvalidNicknameShowsErrorMessage() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
            } catch (Exception e) {
                Assert.fail("Failed to set the nickname");
            }
        });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(activity.getResources().getString(
                        R.string.autofill_credit_card_editor_invalid_nickname));
        // Since the nickname has an error, the done button should be disabled.
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void testErrorMessageHiddenAfterNicknameIsEditedFromInvalidToValid() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
            } catch (Exception e) {
                Assert.fail("Failed to set the nickname");
            }
        });
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(activity.getResources().getString(
                        R.string.autofill_credit_card_editor_invalid_nickname));

        // Set the nickname to valid one.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                autofillLocalCardEditorFragment.mNicknameText.setText("Valid Nickname");
            } catch (Exception e) {
                Assert.fail("Failed to set the nickname");
            }
        });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    public void testErrorMessageHiddenAfterNicknameIsEditedFromInvalidToEmpty() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
            } catch (Exception e) {
                Assert.fail("Failed to set the nickname");
            }
        });
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(activity.getResources().getString(
                        R.string.autofill_credit_card_editor_invalid_nickname));

        // Clear the nickname.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                autofillLocalCardEditorFragment.mNicknameText.setText(null);
            } catch (Exception e) {
                Assert.fail("Failed to set the nickname");
            }
        });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    public void testNicknameLengthCappedAt25Characters() throws Exception {
        String veryLongNickname = "This is a very very long nickname";
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                autofillLocalCardEditorFragment.mNicknameText.setText(veryLongNickname);
            } catch (Exception e) {
                Assert.fail("Failed to set the nickname");
            }
        });

        assertThat(autofillLocalCardEditorFragment.mNicknameText.getText().toString())
                .isEqualTo(veryLongNickname.substring(0, 25));
    }

    private Bundle fragmentArgs(String guid) {
        Bundle args = new Bundle();
        args.putString(AutofillEditorBase.AUTOFILL_GUID, guid);
        return args;
    }
}
