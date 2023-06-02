// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.settings.AddressEditor;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridgeJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Unit tests for {@link SaveUpdateAddressProfilePrompt}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.AUTOFILL_ADDRESS_PROFILE_SAVE_PROMPT_NICKNAME_SUPPORT,
        ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES})
public class SaveUpdateAddressProfilePromptTest {
    private static final long NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER = 100L;
    private static final boolean NO_MIGRATION = false;
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private SaveUpdateAddressProfilePromptController.Natives mPromptControllerJni;
    @Mock
    private AutofillProfileBridge.Natives mAutofillProfileBridgeJni;
    @Mock
    private PersonalDataManager mPersonalDataManager;
    @Mock
    private Profile mProfile;
    @Mock
    private AddressEditor mAddressEditor;

    @Captor
    private ArgumentCaptor<Callback<AutofillAddress>> mCallbackCaptor;

    private Activity mActivity;
    private SaveUpdateAddressProfilePromptController mPromptController;
    private FakeModalDialogManager mModalDialogManager;
    private SaveUpdateAddressProfilePrompt mPrompt;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PersonalDataManager.setInstanceForTesting(mPersonalDataManager);

        mActivity = Robolectric.setupActivity(TestActivity.class);

        mPromptController = SaveUpdateAddressProfilePromptController.create(
                NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER);
        mJniMocker.mock(
                SaveUpdateAddressProfilePromptControllerJni.TEST_HOOKS, mPromptControllerJni);
        mJniMocker.mock(AutofillProfileBridgeJni.TEST_HOOKS, mAutofillProfileBridgeJni);
    }

    @After
    public void tearDown() {
        PersonalDataManager.setInstanceForTesting(null);
    }

    private void createAndShowPrompt(boolean isUpdate) {
        createAndShowPrompt(isUpdate, NO_MIGRATION);
    }

    private void createAndShowPrompt(boolean isUpdate, boolean isMigrationToAccount) {
        AutofillProfile dummyProfile = AutofillProfile.builder().build();
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mPrompt = new SaveUpdateAddressProfilePrompt(mPromptController, mModalDialogManager,
                mActivity, mProfile, dummyProfile, isUpdate, isMigrationToAccount);
        mPrompt.setAddressEditorForTesting(mAddressEditor);
        mPrompt.show();
    }

    private void validateTextView(TextView view, String text) {
        Assert.assertNotNull(view);
        Assert.assertEquals(text, view.getText());
    }

    @Test
    @SmallTest
    public void dialogShown() {
        createAndShowPrompt(false);
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    @SmallTest
    public void positiveButtonPressed() {
        createAndShowPrompt(false);
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
        mModalDialogManager.clickPositiveButton();
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni, times(1))
                .onUserAccepted(eq(NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER), any());
        verify(mPromptControllerJni, times(1))
                .onPromptDismissed(eq(NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER), any());
    }

    @Test
    @SmallTest
    public void negativeButtonPressed() {
        createAndShowPrompt(false);

        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
        mModalDialogManager.clickNegativeButton();
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni, times(1))
                .onUserDeclined(eq(NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER), any());
        verify(mPromptControllerJni, times(1))
                .onPromptDismissed(eq(NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER), any());
    }

    @Test
    @SmallTest
    public void dialogDismissed() {
        createAndShowPrompt(false);
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
        // Simulate dialog dismissal by native.
        mPrompt.dismiss();
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        // Check that callback was still called when the dialog is dismissed.
        verify(mPromptControllerJni, times(1))
                .onPromptDismissed(eq(NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER), any());
    }

    @Test
    @SmallTest
    public void dialogStrings() {
        createAndShowPrompt(false);

        View dialog = mPrompt.getDialogViewForTesting();
        PropertyModel propertyModel = mModalDialogManager.getShownDialogModel();

        mPrompt.setDialogDetails("title", "positive button text", "negative button text");
        Assert.assertEquals("title", propertyModel.get(ModalDialogProperties.TITLE));
        Assert.assertEquals("positive button text",
                propertyModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertEquals("negative button text",
                propertyModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    @SmallTest
    public void dialogStrings_SourceNotice() {
        createAndShowPrompt(false, true);
        View dialog = mPrompt.getDialogViewForTesting();

        mPrompt.setSourceNotice(null);
        Assert.assertEquals(View.GONE,
                dialog.findViewById(R.id.autofill_address_profile_prompt_source_notice)
                        .getVisibility());

        mPrompt.setSourceNotice("");
        Assert.assertEquals(View.GONE,
                dialog.findViewById(R.id.autofill_address_profile_prompt_source_notice)
                        .getVisibility());

        mPrompt.setSourceNotice("source notice");
        Assert.assertEquals(View.VISIBLE,
                dialog.findViewById(R.id.autofill_address_profile_prompt_source_notice)
                        .getVisibility());
        validateTextView(dialog.findViewById(R.id.autofill_address_profile_prompt_source_notice),
                "source notice");
    }

    @Test
    @SmallTest
    public void dialogStrings_SaveAddress() {
        createAndShowPrompt(false);

        View dialog = mPrompt.getDialogViewForTesting();

        mPrompt.setSaveOrMigrateDetails("address", "email", "phone");
        validateTextView(dialog.findViewById(R.id.address), "address");
        validateTextView(dialog.findViewById(R.id.email), "email");
        validateTextView(dialog.findViewById(R.id.phone), "phone");
    }

    @Test
    @SmallTest
    public void dialogStrings_UpdateAddress() {
        createAndShowPrompt(true);

        View dialog = mPrompt.getDialogViewForTesting();

        mPrompt.setUpdateDetails("subtitle", "old details", "new details");
        validateTextView(dialog.findViewById(R.id.subtitle), "subtitle");
        validateTextView(dialog.findViewById(R.id.details_old), "old details");
        validateTextView(dialog.findViewById(R.id.details_new), "new details");
    }

    @Test
    @SmallTest
    public void showHeaders() {
        createAndShowPrompt(true);

        View dialog = mPrompt.getDialogViewForTesting();

        mPrompt.setUpdateDetails("subtitle", "", "new details");
        Assert.assertEquals(dialog.findViewById(R.id.header_new).getVisibility(), View.GONE);
        Assert.assertEquals(dialog.findViewById(R.id.header_old).getVisibility(), View.GONE);
        Assert.assertEquals(
                dialog.findViewById(R.id.no_header_space).getVisibility(), View.VISIBLE);

        mPrompt.setUpdateDetails("subtitle", "old details", "new details");
        Assert.assertEquals(dialog.findViewById(R.id.header_new).getVisibility(), View.VISIBLE);
        Assert.assertEquals(dialog.findViewById(R.id.header_old).getVisibility(), View.VISIBLE);
        Assert.assertEquals(dialog.findViewById(R.id.no_header_space).getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ADDRESS_PROFILE_SAVE_PROMPT_NICKNAME_SUPPORT})
    public void setupAddressNickname_FeatureDisabled() {
        createAndShowPrompt(false);

        View dialog = mPrompt.getDialogViewForTesting();
        Assert.assertEquals(
                dialog.findViewById(R.id.nickname_input_layout).getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void setupAddressNickname_FeatureEnabled() {
        createAndShowPrompt(false);

        View dialog = mPrompt.getDialogViewForTesting();
        TextView nicknameInput = dialog.findViewById(R.id.nickname_input);

        Assert.assertEquals(nicknameInput.getVisibility(), View.VISIBLE);
        Assert.assertEquals(nicknameInput.getHint(), "Add a label");

        nicknameInput.requestFocus();
        Assert.assertEquals(nicknameInput.getHint(), "Label");

        nicknameInput.setText("Text");
        nicknameInput.clearFocus();
        Assert.assertEquals(nicknameInput.getHint(), "Label");

        nicknameInput.requestFocus();
        Assert.assertEquals(nicknameInput.getHint(), "Label");

        nicknameInput.setText("");
        nicknameInput.clearFocus();
        Assert.assertEquals(nicknameInput.getHint(), "Add a label");
    }

    @Test
    @SmallTest
    public void setupAddressNickname_NoNicknamesDuringUpdate() {
        createAndShowPrompt(true);

        View dialog = mPrompt.getDialogViewForTesting();
        Assert.assertNull(dialog.findViewById(R.id.nickname_input_layout));
    }

    @Test
    @SmallTest
    public void clickEditButton() {
        createAndShowPrompt(true);
        View dialog = mPrompt.getDialogViewForTesting();
        ImageButton editButton = dialog.findViewById(R.id.edit_button);
        editButton.performClick();
        verify(mAddressEditor).showEditorDialog();
    }
}
