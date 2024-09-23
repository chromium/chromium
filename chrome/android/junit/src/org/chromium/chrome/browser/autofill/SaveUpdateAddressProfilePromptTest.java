// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
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
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Unit tests for {@link SaveUpdateAddressProfilePrompt}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SaveUpdateAddressProfilePromptTest {
    private static final long NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER = 100L;
    private static final boolean NO_MIGRATION = false;
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private SaveUpdateAddressProfilePromptController.Natives mPromptControllerJni;
    @Mock private AutofillProfileBridge.Natives mAutofillProfileBridgeJni;
    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private Profile mProfile;
    @Mock private AddressEditorCoordinator mAddressEditor;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SyncService mSyncService;

    @Captor private ArgumentCaptor<Callback<AutofillAddress>> mCallbackCaptor;

    private Activity mActivity;
    private SaveUpdateAddressProfilePromptController mPromptController;
    private FakeModalDialogManager mModalDialogManager;
    private SaveUpdateAddressProfilePrompt mPrompt;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);

        mActivity = Robolectric.setupActivity(TestActivity.class);

        mPromptController =
                SaveUpdateAddressProfilePromptController.create(
                        NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER);
        mJniMocker.mock(
                SaveUpdateAddressProfilePromptControllerJni.TEST_HOOKS, mPromptControllerJni);
        mJniMocker.mock(AutofillProfileBridgeJni.TEST_HOOKS, mAutofillProfileBridgeJni);
    }

    private void createAndShowPrompt(boolean isUpdate) {
        createAndShowPrompt(isUpdate, NO_MIGRATION);
    }

    private void createAndShowPrompt(boolean isUpdate, boolean isMigrationToAccount) {
        AutofillProfile dummyProfile = AutofillProfile.builder().build();
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mPrompt =
                new SaveUpdateAddressProfilePrompt(
                        mPromptController,
                        mModalDialogManager,
                        mActivity,
                        mProfile,
                        dummyProfile,
                        isUpdate,
                        isMigrationToAccount);
        mPrompt.setAddressEditorForTesting(mAddressEditor);
        mPrompt.show();
    }

    private void validateTextView(TextView view, String text) {
        assertNotNull(view);
        assertEquals(text, view.getText());
    }

    @Test
    @SmallTest
    public void dialogShown() {
        createAndShowPrompt(false);
        assertNotNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    @SmallTest
    public void positiveButtonPressed() {
        createAndShowPrompt(false);
        assertNotNull(mModalDialogManager.getShownDialogModel());
        mModalDialogManager.clickPositiveButton();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni, times(1))
                .onUserAccepted(eq(NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER), any());
        verify(mPromptControllerJni, times(1))
                .onPromptDismissed(eq(NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER), any());
    }

    @Test
    @SmallTest
    public void negativeButtonPressed() {
        createAndShowPrompt(false);

        assertNotNull(mModalDialogManager.getShownDialogModel());
        mModalDialogManager.clickNegativeButton();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni, times(1))
                .onUserDeclined(eq(NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER), any());
        verify(mPromptControllerJni, times(1))
                .onPromptDismissed(eq(NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER), any());
    }

    @Test
    @SmallTest
    public void dialogDismissed() {
        createAndShowPrompt(false);
        assertNotNull(mModalDialogManager.getShownDialogModel());
        // Simulate dialog dismissal by native.
        mPrompt.dismiss();
        assertNull(mModalDialogManager.getShownDialogModel());
        // Check that callback was still called when the dialog is dismissed.
        verify(mPromptControllerJni, times(1))
                .onPromptDismissed(eq(NATIVE_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_CONTROLLER), any());
    }

    @Test
    @SmallTest
    public void dialogStrings() {
        createAndShowPrompt(false);

        PropertyModel propertyModel = mModalDialogManager.getShownDialogModel();

        mPrompt.setDialogDetails("title", "positive button text", "negative button text");
        assertEquals("title", propertyModel.get(ModalDialogProperties.TITLE));
        assertEquals(
                "positive button text",
                propertyModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                "negative button text",
                propertyModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    @SmallTest
    public void dialogStrings_RecordTypeNotice() {
        createAndShowPrompt(false, true);
        View dialog = mPrompt.getDialogViewForTesting();

        mPrompt.setRecordTypeNotice(null);
        assertEquals(
                View.GONE,
                dialog.findViewById(R.id.autofill_address_profile_prompt_record_type_notice)
                        .getVisibility());

        mPrompt.setRecordTypeNotice("");
        assertEquals(
                View.GONE,
                dialog.findViewById(R.id.autofill_address_profile_prompt_record_type_notice)
                        .getVisibility());

        mPrompt.setRecordTypeNotice("record type notice");
        assertEquals(
                View.VISIBLE,
                dialog.findViewById(R.id.autofill_address_profile_prompt_record_type_notice)
                        .getVisibility());
        validateTextView(
                dialog.findViewById(R.id.autofill_address_profile_prompt_record_type_notice),
                "record type notice");
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
        assertEquals(dialog.findViewById(R.id.header_new).getVisibility(), View.GONE);
        assertEquals(dialog.findViewById(R.id.header_old).getVisibility(), View.GONE);
        assertEquals(dialog.findViewById(R.id.no_header_space).getVisibility(), View.VISIBLE);

        mPrompt.setUpdateDetails("subtitle", "old details", "new details");
        assertEquals(dialog.findViewById(R.id.header_new).getVisibility(), View.VISIBLE);
        assertEquals(dialog.findViewById(R.id.header_old).getVisibility(), View.VISIBLE);
        assertEquals(dialog.findViewById(R.id.no_header_space).getVisibility(), View.GONE);
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
