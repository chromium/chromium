// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.DATE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.NOTICE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.IMPORTANT_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_VISIBLE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.SHOW_BACKGROUND;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.TEXT_APPEARANCE;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE_CHANGED_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldProperties.TEXT_FIELD_TYPE;
import static org.chromium.chrome.browser.autofill.editors.utils.TestUtils.setDropdownValue;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.ClickableSpan;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.AutofillProfileBridgeJni;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType;
import org.chromium.chrome.browser.autofill.editors.common.EditorDialogToolbar;
import org.chromium.chrome.browser.autofill.editors.common.date_field.DateFieldView;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.autofill.DropdownKeyValue;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.autofill_ai.AttributeInstance;
import org.chromium.components.autofill.autofill_ai.AttributeInstance.DateValue;
import org.chromium.components.autofill.autofill_ai.AttributeInstance.StringValue;
import org.chromium.components.autofill.autofill_ai.AttributeType;
import org.chromium.components.autofill.autofill_ai.AttributeTypeName;
import org.chromium.components.autofill.autofill_ai.DataType;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.time.LocalDate;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
public class EntityEditorModuleTest {
    private static final String USER_EMAIL = "example@gmail.com";
    private static final AttributeType PASSPORT_NAME_ATTRIBUTE_TYPE =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.PASSPORT_NAME,
                    /* typeNameAsString= */ "Passport name",
                    /* dataType= */ DataType.NAME,
                    /* fieldType= */ FieldType.NAME_FULL);
    private static final AttributeType PASSPORT_COUNTRY_ATTRIBUTE_TYPE =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.PASSPORT_COUNTRY,
                    /* typeNameAsString= */ "Passport country",
                    /* dataType= */ DataType.COUNTRY,
                    /* fieldType= */ FieldType.PASSPORT_ISSUING_COUNTRY);
    private static final AttributeType PASSPORT_NUMBER_ATTRIBUTE_TYPE =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.PASSPORT_NUMBER,
                    /* typeNameAsString= */ "Passport number",
                    /* dataType= */ DataType.STRING,
                    /* fieldType= */ FieldType.PASSPORT_NUMBER);
    private static final AttributeType PASSPORT_ISSUE_DATE_TYPE =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.PASSPORT_ISSUE_DATE,
                    /* typeNameAsString= */ "Issue date",
                    /* dataType= */ DataType.DATE,
                    /* fieldType= */ FieldType.PASSPORT_ISSUE_DATE);
    private static final AttributeType PASSPORT_EXPIRATION_DATE_TYPE =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.PASSPORT_EXPIRATION_DATE,
                    /* typeNameAsString= */ "Expiration date",
                    /* dataType= */ DataType.DATE,
                    /* fieldType= */ FieldType.PASSPORT_EXPIRATION_DATE);
    private static final EntityType PASSPORT_TYPE =
            new EntityType(
                    /* typeName= */ EntityTypeName.PASSPORT,
                    /* isReadOnly= */ false,
                    /* isEnabled= */ true,
                    /* isEligibleForWalletStorage= */ false,
                    /* isMaskedStorageSupported= */ true,
                    /* typeNameAsString= */ "Passport",
                    /* typeNameAsMetricsString= */ "Passport",
                    /* addEntityTypeString= */ "Add passport",
                    /* editEntityTypeString= */ "Edit passport",
                    /* deleteEntityTypeString= */ "Delete passport",
                    /* attributeTypes= */ List.of(
                            PASSPORT_NAME_ATTRIBUTE_TYPE,
                            PASSPORT_COUNTRY_ATTRIBUTE_TYPE,
                            PASSPORT_NUMBER_ATTRIBUTE_TYPE,
                            PASSPORT_ISSUE_DATE_TYPE,
                            PASSPORT_EXPIRATION_DATE_TYPE),
                    /* requiredAttributes= */ List.of(PASSPORT_NUMBER_ATTRIBUTE_TYPE));

    private static final AttributeType sVehicleMakeType =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.VEHICLE_MAKE,
                    /* typeNameAsString= */ "Make",
                    /* dataType= */ DataType.STRING,
                    /* fieldType= */ FieldType.VEHICLE_MAKE);
    private static final AttributeType sVehicleModelType =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.VEHICLE_MODEL,
                    /* typeNameAsString= */ "Model",
                    /* dataType= */ DataType.STRING,
                    /* fieldType= */ FieldType.VEHICLE_MODEL);
    private static final AttributeType sVehicleYearType =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.VEHICLE_YEAR,
                    /* typeNameAsString= */ "Year",
                    /* dataType= */ DataType.STRING,
                    /* fieldType= */ FieldType.VEHICLE_YEAR);
    private static final AttributeType sVehicleOwnerType =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.VEHICLE_OWNER,
                    /* typeNameAsString= */ "Owner",
                    /* dataType= */ DataType.NAME,
                    /* fieldType= */ FieldType.NAME_FULL);
    private static final AttributeType sVehicleLicensePlateType =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.VEHICLE_PLATE_NUMBER,
                    /* typeNameAsString= */ "License place",
                    /* dataType= */ DataType.STRING,
                    /* fieldType= */ FieldType.VEHICLE_LICENSE_PLATE);
    private static final AttributeType sVehiclePlateStateType =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.VEHICLE_PLATE_STATE,
                    /* typeNameAsString= */ "State",
                    /* dataType= */ DataType.STATE,
                    /* fieldType= */ FieldType.VEHICLE_PLATE_STATE);
    private static final AttributeType sVehicleVinType =
            new AttributeType(
                    /* typeName= */ AttributeTypeName.VEHICLE_VIN,
                    /* typeNameAsString= */ "VIN",
                    /* dataType= */ DataType.STRING,
                    /* fieldType= */ FieldType.VEHICLE_VIN);

    private static final EntityType sVehicleType =
            new EntityType(
                    EntityTypeName.VEHICLE,
                    /* isReadOnly= */ false,
                    /* isEnabled= */ true,
                    /* isEligibleForWalletStorage= */ true,
                    /* isMaskedStorageSupported= */ true,
                    /* typeNameAsString= */ "Vehicle",
                    /* typeNameAsMetricsString= */ "Vehicle",
                    /* addEntityTypeString= */ "Add vehicle",
                    /* editEntityTypeString= */ "Edit vehicle",
                    /* deleteEntityTypeString= */ "Delete vehicle",
                    /* attributeTypes= */ List.of(
                            sVehicleMakeType,
                            sVehicleModelType,
                            sVehicleYearType,
                            sVehicleOwnerType,
                            sVehicleLicensePlateType,
                            sVehiclePlateStateType,
                            sVehicleVinType),
                    /* requiredAttributes= */ List.of(sVehicleLicensePlateType, sVehicleVinType));

    private static final EntityInstance LOCAL_PASSPORT =
            new EntityInstance.Builder(PASSPORT_TYPE)
                    .setGuid("guid")
                    .setRecordType(RecordType.LOCAL)
                    .setModifiedDate(LocalDate.of(2026, 2, 15))
                    .setUseCount(0)
                    .addAttribute(
                            new AttributeInstance(
                                    PASSPORT_NUMBER_ATTRIBUTE_TYPE, /* value= */ "AA123456"))
                    .addAttribute(
                            new AttributeInstance(
                                    PASSPORT_ISSUE_DATE_TYPE,
                                    /* date= */ LocalDate.of(2026, 2, 15)))
                    .build();

    private static final EntityInstance NEW_LOCAL_PASSPORT =
            new EntityInstance.Builder(PASSPORT_TYPE)
                    .setGuid("")
                    .setRecordType(RecordType.LOCAL)
                    .setModifiedDate(LocalDate.of(2026, 2, 15))
                    .setUseCount(0)
                    .build();

    private static final EntityInstance NEW_WALLET_PASSPORT =
            new EntityInstance.Builder(PASSPORT_TYPE)
                    .setGuid("")
                    .setRecordType(RecordType.SERVER_WALLET)
                    .setModifiedDate(LocalDate.of(2026, 2, 15))
                    .setUseCount(0)
                    .build();

    private static final EntityInstance WALLET_PASSPORT =
            new EntityInstance.Builder(PASSPORT_TYPE)
                    .setGuid("guid")
                    .setRecordType(RecordType.SERVER_WALLET)
                    .setModifiedDate(LocalDate.of(2026, 2, 15))
                    .setUseCount(0)
                    .addAttribute(
                            new AttributeInstance(
                                    PASSPORT_NAME_ATTRIBUTE_TYPE, /* value= */ "John Doe"))
                    .addAttribute(
                            new AttributeInstance(
                                    PASSPORT_COUNTRY_ATTRIBUTE_TYPE, /* value= */ "Germany"))
                    .build();

    private static final EntityInstance PRIVATE_WALLET_PASSPORT =
            new EntityInstance.Builder(PASSPORT_TYPE)
                    .setGuid("guid")
                    .setRecordType(RecordType.SERVER_WALLET)
                    .setIsMaskedServerEntity(true)
                    .setModifiedDate(LocalDate.of(2026, 2, 15))
                    .setUseCount(0)
                    .build();

    private final CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId(USER_EMAIL, new GaiaId("gaia_id"));

    // Note: can't initialize this list statically because of how Robolectric
    // initializes Android library dependencies.
    private final List<DropdownKeyValue> mSupportedCountries =
            List.of(
                    new DropdownKeyValue("US", "United States"),
                    new DropdownKeyValue("DE", "Germany"),
                    new DropdownKeyValue("CU", "Cuba"));

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);
    @Mock private Delegate mDelegate;
    @Mock private Profile mProfile;
    @Mock private IdentityManager mIdentityManager;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private AutofillProfileBridge.Natives mAutofillProfileBridgeJni;

    @Captor private ArgumentCaptor<EntityInstance> mEntityInstanceCaptor;

    private Activity mActivity;
    private EntityEditorCoordinator mCoordinator;
    private View mContainerView;

    @Before
    public void setUp() {
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);
        when(mAutofillProfileBridgeJni.getSupportedCountries()).thenReturn(mSupportedCountries);
        AutofillProfileBridgeJni.setInstanceForTesting(mAutofillProfileBridgeJni);
        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);

        mActivity = Robolectric.setupActivity(TestActivity.class);
    }

    @Test
    @SmallTest
    public void testBackgroundColor() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(NEW_LOCAL_PASSPORT);
        verifyHasExpectedBackgroundColor(
                mContainerView, SemanticColorUtils.getSettingsBackgroundColor(mActivity));
        verifyHasExpectedBackgroundColor(
                mContainerView.findViewById(R.id.action_bar),
                SemanticColorUtils.getSettingsBackgroundColor(mActivity));
        verifyHasExpectedBackgroundColor(
                mContainerView.findViewById(R.id.button_bar),
                SemanticColorUtils.getSettingsBackgroundColor(mActivity));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testBackgroundColorWhenAutofillAiDisabled() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(NEW_LOCAL_PASSPORT);
        verifyHasExpectedBackgroundColor(
                mContainerView, SemanticColorUtils.getDefaultBgColor(mActivity));
        verifyHasExpectedBackgroundColor(
                mContainerView.findViewById(R.id.action_bar),
                SemanticColorUtils.getDefaultBgColor(mActivity));
        verifyHasExpectedBackgroundColor(
                mContainerView.findViewById(R.id.button_bar),
                SemanticColorUtils.getDefaultBgColor(mActivity));
    }

    @Test
    @SmallTest
    public void testOpenHelpAndFeedback() {
        showEditorDialog(LOCAL_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        Callback<Activity> callback = model.get(EntityEditorProperties.OPEN_HELP_CALLBACK);
        callback.onResult(mActivity);
        verify(mHelpAndFeedbackLauncher)
                .show(
                        eq(mActivity),
                        eq(mActivity.getString(R.string.help_context_autofill)),
                        isNull());
    }

    @Test
    @SmallTest
    public void testValidateOnShow() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(NEW_LOCAL_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        assertFalse(model.get(EntityEditorProperties.VALIDATE_ON_SHOW));
    }

    @Test
    @SmallTest
    public void testShowEditorDialogForNewEntity() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(NEW_LOCAL_PASSPORT);
        EditorDialogToolbar toolbar = mContainerView.findViewById(R.id.action_bar);
        assertEquals(PASSPORT_TYPE.getAddEntityTypeString(), toolbar.getTitle());
        assertFalse(toolbar.getBrandingIconForTest().isVisible());
        assertTrue(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void testShowEditorDialogForNewWalletEntity() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(NEW_WALLET_PASSPORT);
        EditorDialogToolbar toolbar = mContainerView.findViewById(R.id.action_bar);
        assertEquals(PASSPORT_TYPE.getAddEntityTypeString(), toolbar.getTitle());
        assertTrue(toolbar.getBrandingIconForTest().isVisible());
        assertEquals(
                mActivity.getString(R.string.autofill_google_wallet_title),
                toolbar.getBrandingIconForTest().getActionView().getContentDescription());
        assertTrue(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void testShowEditorDialogForExistingEntity() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(LOCAL_PASSPORT);
        EditorDialogToolbar toolbar = mContainerView.findViewById(R.id.action_bar);
        assertEquals(PASSPORT_TYPE.getEditEntityTypeString(), toolbar.getTitle());
        assertFalse(toolbar.getBrandingIconForTest().isVisible());
        assertTrue(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void testClickCancelButton() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(LOCAL_PASSPORT);
        mContainerView.findViewById(R.id.payments_edit_cancel_button).performClick();
        assertFalse(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void testDeleteLocalEntity() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(LOCAL_PASSPORT);
        PropertyModel model = mCoordinator.getEditorModelForTest();
        assertTrue(model.get(EntityEditorProperties.ALLOW_DELETE));
        assertEquals(
                model.get(EntityEditorProperties.DELETE_CONFIRMATION_TITLE),
                PASSPORT_TYPE.getDeleteEntityTypeString());
        assertEquals(
                model.get(EntityEditorProperties.DELETE_CONFIRMATION_TEXT),
                mActivity.getString(
                        R.string.autofill_ai_entity_editor_delete_local_entity_dialog_text));
        assertEquals(
                model.get(EntityEditorProperties.DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID),
                R.string.autofill_delete_suggestion_button);

        HistogramWatcher deletionCancelledHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                EntityEditorMediator.ENTITY_DELETED_HISTOGRAM
                                        + LOCAL_PASSPORT
                                                .getEntityType()
                                                .getTypeNameAsMetricsString(),
                                /* value= */ true,
                                /* times= */ 1)
                        .expectBooleanRecordTimes(
                                EntityEditorMediator.ENTITY_DELETED_SETTINGS_HISTOGRAM
                                        + LOCAL_PASSPORT
                                                .getEntityType()
                                                .getTypeNameAsMetricsString(),
                                /* value= */ true,
                                /* times= */ 1)
                        .build();
        model.get(EntityEditorProperties.DELETE_CALLBACK).onResult(false);
        verify(mDelegate, never()).onDelete(LOCAL_PASSPORT);

        HistogramWatcher deletionConfirmedHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                EntityEditorMediator.ENTITY_DELETED_HISTOGRAM
                                        + LOCAL_PASSPORT
                                                .getEntityType()
                                                .getTypeNameAsMetricsString(),
                                /* value= */ true,
                                /* times= */ 1)
                        .expectBooleanRecordTimes(
                                EntityEditorMediator.ENTITY_DELETED_SETTINGS_HISTOGRAM
                                        + LOCAL_PASSPORT
                                                .getEntityType()
                                                .getTypeNameAsMetricsString(),
                                /* value= */ true,
                                /* times= */ 1)
                        .build();
        model.get(EntityEditorProperties.DELETE_CALLBACK).onResult(true);
        verify(mDelegate).onDelete(LOCAL_PASSPORT);
    }

    @Test
    @SmallTest
    public void testDeleteWalletEntity() {
        showEditorDialog(WALLET_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        assertFalse(model.get(EntityEditorProperties.ALLOW_DELETE));
    }

    @Test
    @SmallTest
    public void testDeleteNewEntity() {
        EntityInstance newPassport =
                new EntityInstance.Builder(PASSPORT_TYPE)
                        .setGuid("")
                        .setRecordType(RecordType.LOCAL)
                        .setModifiedDate(LocalDate.of(2026, 2, 15))
                        .setUseCount(0)
                        .build();
        showEditorDialog(newPassport);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        assertFalse(model.get(EntityEditorProperties.ALLOW_DELETE));
    }

    @Test
    @SmallTest
    public void testEditorFields() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(LOCAL_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        verifyLocalPassportFields(model.get(EntityEditorProperties.EDITOR_FIELDS));
    }

    @Test
    @SmallTest
    public void testLocalEntitySourceNotice() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(LOCAL_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        verifySourceNotice(
                model.get(EntityEditorProperties.EDITOR_FIELDS),
                mActivity.getString(
                        R.string.autofill_ai_save_or_update_local_entity_source_notice));
    }

    @Test
    @SmallTest
    public void testWalletEntitySourceNotice() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(mAccountInfo);
        showEditorDialog(WALLET_PASSPORT);

        String walletTitle = mActivity.getString(R.string.autofill_google_wallet_title);
        String expectedNoticeText =
                mActivity
                        .getString(
                                R.string.autofill_ai_save_or_update_entity_in_wallet_source_notice)
                        .replace("$1", walletTitle)
                        .replace("$2", walletTitle)
                        .replace("$3", USER_EMAIL)
                        .replace("<link>", "")
                        .replace("</link>", "");

        PropertyModel model = mCoordinator.getEditorModelForTest();
        verifySourceNotice(model.get(EntityEditorProperties.EDITOR_FIELDS), expectedNoticeText);
    }

    @Test
    @SmallTest
    public void testWalletEntitySourceNotice_ClickLink() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(mAccountInfo);
        showEditorDialog(WALLET_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        ListModel<EditorItem> editorFields = model.get(EntityEditorProperties.EDITOR_FIELDS);

        clickSourceNoticeLink();
        verify(mDelegate).onOpenGoogleWallet(false);
    }

    @Test
    @SmallTest
    public void testPrivateWalletEntitySourceNotice_ClickLink() {
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(mAccountInfo);
        showEditorDialog(PRIVATE_WALLET_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        ListModel<EditorItem> editorFields = model.get(EntityEditorProperties.EDITOR_FIELDS);

        clickSourceNoticeLink();
        verify(mDelegate).onOpenGoogleWallet(true);
    }

    @Test
    @SmallTest
    public void testCommitChanges() {
        EntityInstance entity =
                new EntityInstance.Builder(PASSPORT_TYPE)
                        .setGuid("guid")
                        .setRecordType(RecordType.LOCAL)
                        .setModifiedDate(LocalDate.of(2026, 2, 15))
                        .setUseCount(0)
                        .addAttribute(
                                new AttributeInstance(
                                        PASSPORT_COUNTRY_ATTRIBUTE_TYPE, /* value= */ "Cuba"))
                        .addAttribute(
                                new AttributeInstance(
                                        PASSPORT_NUMBER_ATTRIBUTE_TYPE, /* value= */ "AA123456"))
                        .build();
        showEditorDialog(entity);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        ListModel<EditorItem> editorFields = model.get(EntityEditorProperties.EDITOR_FIELDS);
        // Make sure that the fields order is correct before editing them.
        EditorItem passportNameItem = editorFields.get(0);
        assertEquals("", passportNameItem.model.get(VALUE));
        EditorItem passportCountryItem = editorFields.get(1);
        assertEquals("Cuba", passportCountryItem.model.get(VALUE));
        EditorItem passportNumberItem = editorFields.get(2);
        assertEquals("AA123456", passportNumberItem.model.get(VALUE));

        // Update some fields.
        passportNameItem.model.set(VALUE, "John Doe");
        passportCountryItem.model.set(VALUE, "Germany");

        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        verify(mDelegate).onDone(mEntityInstanceCaptor.capture(), anyInt(), anyInt());
        EntityInstance updatedEntityInstance = mEntityInstanceCaptor.getValue();

        AttributeInstance passportName =
                updatedEntityInstance.getAttribute(PASSPORT_NAME_ATTRIBUTE_TYPE);
        assertEquals(new StringValue("John Doe"), passportName.getAttributeValue());
        AttributeInstance passportCountry =
                updatedEntityInstance.getAttribute(PASSPORT_COUNTRY_ATTRIBUTE_TYPE);
        assertEquals(new StringValue("Germany"), passportCountry.getAttributeValue());
        AttributeInstance passportNumber =
                updatedEntityInstance.getAttribute(PASSPORT_NUMBER_ATTRIBUTE_TYPE);
        assertEquals(new StringValue("AA123456"), passportNumber.getAttributeValue());
    }

    @Test
    @SmallTest
    public void testCommitChangesWithInvalidDate() {
        EntityInstance entity =
                new EntityInstance.Builder(PASSPORT_TYPE)
                        .setGuid("guid")
                        .setRecordType(RecordType.LOCAL)
                        .setModifiedDate(LocalDate.of(2026, 2, 15))
                        .setUseCount(0)
                        .addAttribute(
                                new AttributeInstance(
                                        PASSPORT_COUNTRY_ATTRIBUTE_TYPE, /* value= */ "Cuba"))
                        .addAttribute(
                                new AttributeInstance(
                                        PASSPORT_NUMBER_ATTRIBUTE_TYPE, /* value= */ "AA123456"))
                        .build();
        showEditorDialog(entity);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        ListModel<EditorItem> editorFields = model.get(EntityEditorProperties.EDITOR_FIELDS);
        EditorItem issueDateItem = editorFields.get(3);
        EditorItem sourceNoticeItem = editorFields.get(5);

        ViewGroup content = mCoordinator.getEntityEditorViewForTest().getContentView();
        DateFieldView issueDate = (DateFieldView) content.getChildAt(3);

        setDropdownValue(
                issueDate.getMonthPickerForTest(),
                DateFieldView.getMonthName(mActivity, /* month= */ 6));
        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        // Only the month is set, the date is not valid yet.
        verify(mDelegate, times(0)).onDone(any(), anyInt(), anyInt());
        assertFalse(TextUtils.isEmpty(issueDateItem.model.get(ERROR_MESSAGE)));
        // The source notice is only show for required fields.
        assertFalse(sourceNoticeItem.model.get(NOTICE_VISIBLE));

        setDropdownValue(issueDate.getDayPickerForTest(), "20");
        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        // Only the month and day are set, the date is not valid yet.
        verify(mDelegate, times(0)).onDone(any(), anyInt(), anyInt());
        assertFalse(TextUtils.isEmpty(issueDateItem.model.get(ERROR_MESSAGE)));
        // The source notice is only show for required fields.
        assertFalse(sourceNoticeItem.model.get(NOTICE_VISIBLE));

        setDropdownValue(issueDate.getYearPickerForTest(), "2026");
        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        // The date is completely valid, the editor should be closed now.
        verify(mDelegate).onDone(mEntityInstanceCaptor.capture(), anyInt(), anyInt());

        EntityInstance updatedEntityInstance = mEntityInstanceCaptor.getValue();
        AttributeInstance passportIssueDate =
                updatedEntityInstance.getAttribute(PASSPORT_ISSUE_DATE_TYPE);
        assertTrue(passportIssueDate.getAttributeValue() instanceof DateValue);
        assertEquals(
                LocalDate.of(2026, 6, 20),
                ((DateValue) passportIssueDate.getAttributeValue()).getDate());
        // The source notice error message must be hidden after successful validation.
        assertTrue(TextUtils.isEmpty(issueDateItem.model.get(ERROR_MESSAGE)));
        assertFalse(sourceNoticeItem.model.get(NOTICE_VISIBLE));
    }

    @Test
    @SmallTest
    public void testCommitChangesWithWhitespaces() {
        EntityInstance entity =
                new EntityInstance.Builder(PASSPORT_TYPE)
                        .setGuid("guid")
                        .setRecordType(RecordType.LOCAL)
                        .setModifiedDate(LocalDate.of(2026, 2, 15))
                        .setUseCount(0)
                        .addAttribute(
                                new AttributeInstance(
                                        PASSPORT_COUNTRY_ATTRIBUTE_TYPE, /* value= */ "Cuba"))
                        .addAttribute(
                                new AttributeInstance(
                                        PASSPORT_NUMBER_ATTRIBUTE_TYPE, /* value= */ "AA123456"))
                        .build();
        showEditorDialog(entity);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        ListModel<EditorItem> editorFields = model.get(EntityEditorProperties.EDITOR_FIELDS);
        EditorItem passportNameItem = editorFields.get(0);
        EditorItem passportNumberItem = editorFields.get(2);
        EditorItem issueDateItem = editorFields.get(3);
        EditorItem sourceNoticeItem = editorFields.get(5);

        // Update some fields to values with whitespaces.
        passportNameItem.model.set(VALUE, "     ");
        passportNumberItem.model.set(VALUE, "    ");

        // Set partial date to make sure date error message is displayed as well.
        ViewGroup content = mCoordinator.getEntityEditorViewForTest().getContentView();
        DateFieldView issueDate = (DateFieldView) content.getChildAt(3);
        setDropdownValue(
                issueDate.getMonthPickerForTest(),
                DateFieldView.getMonthName(mActivity, /* month= */ 6));

        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        // The passport number field is required, it's not possible to leave it empty.
        verify(mDelegate, times(0)).onDone(any(), anyInt(), anyInt());
        assertFalse(TextUtils.isEmpty(passportNumberItem.model.get(ERROR_MESSAGE)));
        assertFalse(TextUtils.isEmpty(issueDateItem.model.get(ERROR_MESSAGE)));
        assertTrue(sourceNoticeItem.model.get(NOTICE_VISIBLE));

        verifyRequiredFieldsItem(
                editorFields,
                mActivity
                        .getString(
                                R.string
                                        .autofill_ai_entity_editor_single_required_field_error_message)
                        .replace("$1", PASSPORT_NUMBER_ATTRIBUTE_TYPE.getTypeNameAsString()));

        passportNumberItem.model.set(VALUE, "  BB123456  ");
        setDropdownValue(
                issueDate.getMonthPickerForTest(), DateFieldView.getMonthDropdownHint(mActivity));
        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        verify(mDelegate).onDone(mEntityInstanceCaptor.capture(), anyInt(), anyInt());

        EntityInstance updatedEntityInstance = mEntityInstanceCaptor.getValue();
        // The name attribute should not be added to the entity because it wasn't set before.
        assertFalse(updatedEntityInstance.hasAttribute(PASSPORT_NAME_ATTRIBUTE_TYPE));
        assertEquals(
                new StringValue("BB123456"),
                updatedEntityInstance
                        .getAttribute(PASSPORT_NUMBER_ATTRIBUTE_TYPE)
                        .getAttributeValue());
        // All error messages must be hidden after validation.
        assertTrue(TextUtils.isEmpty(passportNumberItem.model.get(ERROR_MESSAGE)));
        assertTrue(TextUtils.isEmpty(issueDateItem.model.get(ERROR_MESSAGE)));
        assertFalse(sourceNoticeItem.model.get(NOTICE_VISIBLE));
    }

    /** Test that the entity editor works correctly if the date fields are required. */
    @Test
    @SmallTest
    public void testCommitChangesWithDatesRequired() {
        EntityType passportType =
                new EntityType(
                        /* typeName= */ EntityTypeName.PASSPORT,
                        /* isReadOnly= */ false,
                        /* isEnabled= */ true,
                        /* isEligibleForWalletStorage= */ false,
                        /* isMaskedStorageSupported= */ true,
                        /* typeNameAsString= */ "Passport",
                        /* typeNameAsMetricsString= */ "Passport",
                        /* addEntityTypeString= */ "Add passport",
                        /* editEntityTypeString= */ "Edit passport",
                        /* deleteEntityTypeString= */ "Delete passport",
                        /* attributeTypes= */ List.of(
                                PASSPORT_NAME_ATTRIBUTE_TYPE,
                                PASSPORT_COUNTRY_ATTRIBUTE_TYPE,
                                PASSPORT_NUMBER_ATTRIBUTE_TYPE,
                                PASSPORT_ISSUE_DATE_TYPE,
                                PASSPORT_EXPIRATION_DATE_TYPE),
                        /* requiredAttributes= */ List.of(
                                PASSPORT_ISSUE_DATE_TYPE, PASSPORT_EXPIRATION_DATE_TYPE));
        EntityInstance entity =
                new EntityInstance.Builder(passportType)
                        .setGuid("guid")
                        .setRecordType(RecordType.LOCAL)
                        .setModifiedDate(LocalDate.of(2026, 2, 15))
                        .setUseCount(0)
                        .addAttribute(
                                new AttributeInstance(
                                        PASSPORT_COUNTRY_ATTRIBUTE_TYPE, /* value= */ "Cuba"))
                        .addAttribute(
                                new AttributeInstance(
                                        PASSPORT_NUMBER_ATTRIBUTE_TYPE, /* value= */ "AA123456"))
                        .build();
        showEditorDialog(entity);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        ListModel<EditorItem> editorFields = model.get(EntityEditorProperties.EDITOR_FIELDS);
        EditorItem passportIssueDate = editorFields.get(3);
        EditorItem passportExpirationDate = editorFields.get(4);
        EditorItem sourceNotice = editorFields.get(5);

        // Make sure the fields are required.
        assertTrue(passportIssueDate.model.get(IS_REQUIRED));
        assertTrue(passportExpirationDate.model.get(IS_REQUIRED));

        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        // The passport number field is required, it's not possible to leave it empty.
        verify(mDelegate, times(0)).onDone(any(), anyInt(), anyInt());
        assertFalse(TextUtils.isEmpty(passportIssueDate.model.get(ERROR_MESSAGE)));
        assertFalse(TextUtils.isEmpty(passportExpirationDate.model.get(ERROR_MESSAGE)));
        assertTrue(sourceNotice.model.get(NOTICE_VISIBLE));

        passportIssueDate.model.set(VALUE, LocalDate.of(2026, 2, 15).toString());
        // Manually run the callback because it's only called when the value is changed though the
        // UI.
        passportIssueDate
                .model
                .get(VALUE_CHANGED_CALLBACK)
                .onResult(LocalDate.of(2026, 2, 15).toString());
        // Error messages must be hidden after the required field's value changes.
        assertTrue(TextUtils.isEmpty(passportIssueDate.model.get(ERROR_MESSAGE)));
        assertTrue(TextUtils.isEmpty(passportExpirationDate.model.get(ERROR_MESSAGE)));
        assertFalse(sourceNotice.model.get(NOTICE_VISIBLE));

        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        verify(mDelegate).onDone(mEntityInstanceCaptor.capture(), anyInt(), anyInt());

        EntityInstance updatedEntityInstance = mEntityInstanceCaptor.getValue();
        // The name attribute should not be added to the entity because it wasn't set before.
        assertTrue(updatedEntityInstance.hasAttribute(PASSPORT_ISSUE_DATE_TYPE));
        assertEquals(
                new DateValue(LocalDate.of(2026, 2, 15).toString()),
                updatedEntityInstance.getAttribute(PASSPORT_ISSUE_DATE_TYPE).getAttributeValue());
    }

    @Test
    @SmallTest
    public void testCommitChangesWithTwoRequiredFields() {
        EntityInstance localVehicle =
                new EntityInstance.Builder(sVehicleType)
                        .setGuid("guid")
                        .setRecordType(RecordType.LOCAL)
                        .setIsMaskedServerEntity(false)
                        .setModifiedDate(LocalDate.of(2026, 2, 15))
                        .setUseCount(0)
                        .build();
        showEditorDialog(localVehicle);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        ListModel<EditorItem> editorFields = model.get(EntityEditorProperties.EDITOR_FIELDS);
        EditorItem vehicleLicensePlate = editorFields.get(4);
        EditorItem vehicleIdentificationNumber = editorFields.get(6);
        EditorItem sourceNotice = editorFields.get(7);

        // Make sure both fields are required.
        assertTrue(vehicleLicensePlate.model.get(IS_REQUIRED));
        assertTrue(vehicleIdentificationNumber.model.get(IS_REQUIRED));

        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        // The entity should not be saved because all required fields are left empty.
        verify(mDelegate, times(0)).onDone(any(), anyInt(), anyInt());
        assertFalse(TextUtils.isEmpty(vehicleLicensePlate.model.get(ERROR_MESSAGE)));
        assertFalse(TextUtils.isEmpty(vehicleIdentificationNumber.model.get(ERROR_MESSAGE)));
        assertTrue(sourceNotice.model.get(NOTICE_VISIBLE));

        verifyRequiredFieldsItem(
                editorFields,
                mActivity
                        .getString(
                                R.string
                                        .autofill_ai_entity_editor_two_required_fields_error_message)
                        .replace("$1", sVehicleLicensePlateType.getTypeNameAsString())
                        .replace("$2", sVehicleVinType.getTypeNameAsString()));

        vehicleLicensePlate.model.set(VALUE, "AA123456BB");
        // Make sure the error messages are hidden after the value is changed.
        assertTrue(TextUtils.isEmpty(vehicleLicensePlate.model.get(ERROR_MESSAGE)));
        assertTrue(TextUtils.isEmpty(vehicleIdentificationNumber.model.get(ERROR_MESSAGE)));
        assertFalse(sourceNotice.model.get(NOTICE_VISIBLE));
        // Click the "Done" button and make sure that the editor is closed because only one required
        // attribute is required to save the entity.
        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        verify(mDelegate).onDone(mEntityInstanceCaptor.capture(), anyInt(), anyInt());

        EntityInstance updatedEntityInstance = mEntityInstanceCaptor.getValue();
        // The name attribute should not be added to the entity because it wasn't set before.
        assertTrue(updatedEntityInstance.hasAttribute(sVehicleLicensePlateType));
        assertEquals(
                new StringValue("AA123456BB"),
                updatedEntityInstance.getAttribute(sVehicleLicensePlateType).getAttributeValue());
        // Error messages must be hidden after successful validation.
        assertTrue(TextUtils.isEmpty(vehicleLicensePlate.model.get(ERROR_MESSAGE)));
        assertTrue(TextUtils.isEmpty(vehicleIdentificationNumber.model.get(ERROR_MESSAGE)));
        assertFalse(sourceNotice.model.get(NOTICE_VISIBLE));
    }

    @Test
    @SmallTest
    public void testCommitChangesWithThreeRequiredFields() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        EntityType passportTypeWithThreeRequiredFields =
                new EntityType(
                        /* typeName= */ EntityTypeName.PASSPORT,
                        /* isReadOnly= */ false,
                        /* isEnabled= */ true,
                        /* isEligibleForWalletStorage= */ false,
                        /* isMaskedStorageSupported= */ true,
                        /* typeNameAsString= */ "Passport",
                        /* typeNameAsMetricsString= */ "Passport",
                        /* addEntityTypeString= */ "Add passport",
                        /* editEntityTypeString= */ "Edit passport",
                        /* deleteEntityTypeString= */ "Delete passport",
                        /* attributeTypes= */ List.of(
                                PASSPORT_NAME_ATTRIBUTE_TYPE,
                                PASSPORT_COUNTRY_ATTRIBUTE_TYPE,
                                PASSPORT_NUMBER_ATTRIBUTE_TYPE,
                                PASSPORT_ISSUE_DATE_TYPE,
                                PASSPORT_EXPIRATION_DATE_TYPE),
                        /* requiredAttributes= */ List.of(
                                PASSPORT_NAME_ATTRIBUTE_TYPE,
                                PASSPORT_NUMBER_ATTRIBUTE_TYPE,
                                PASSPORT_ISSUE_DATE_TYPE));
        EntityInstance passportEntity =
                new EntityInstance.Builder(passportTypeWithThreeRequiredFields)
                        .setGuid("guid")
                        .setRecordType(RecordType.LOCAL)
                        .setIsMaskedServerEntity(false)
                        .setModifiedDate(LocalDate.of(2026, 2, 15))
                        .setUseCount(0)
                        .build();
        showEditorDialog(passportEntity);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        ListModel<EditorItem> editorFields = model.get(EntityEditorProperties.EDITOR_FIELDS);
        EditorItem passportNameItem = editorFields.get(0);
        EditorItem passportNumberItem = editorFields.get(2);
        EditorItem passportIssueDateItem = editorFields.get(3);

        // Make sure all fields are required.
        assertTrue(passportNameItem.model.get(IS_REQUIRED));
        assertTrue(passportNumberItem.model.get(IS_REQUIRED));
        assertTrue(passportIssueDateItem.model.get(IS_REQUIRED));

        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        // The entity should not be saved because all required fields are left empty.
        verify(mDelegate, times(0)).onDone(any(), anyInt(), anyInt());
        assertFalse(TextUtils.isEmpty(passportNameItem.model.get(ERROR_MESSAGE)));
        assertFalse(TextUtils.isEmpty(passportNumberItem.model.get(ERROR_MESSAGE)));
        assertFalse(TextUtils.isEmpty(passportIssueDateItem.model.get(ERROR_MESSAGE)));

        verifyRequiredFieldsItem(
                editorFields,
                mActivity
                        .getString(
                                R.string
                                        .autofill_ai_entity_editor_three_required_fields_error_message)
                        .replace("$1", PASSPORT_NAME_ATTRIBUTE_TYPE.getTypeNameAsString())
                        .replace("$2", PASSPORT_NUMBER_ATTRIBUTE_TYPE.getTypeNameAsString())
                        .replace("$3", PASSPORT_ISSUE_DATE_TYPE.getTypeNameAsString()));

        passportNumberItem.model.set(VALUE, "AA123456BB");
        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        verify(mDelegate).onDone(mEntityInstanceCaptor.capture(), anyInt(), anyInt());

        EntityInstance updatedEntityInstance = mEntityInstanceCaptor.getValue();
        assertTrue(updatedEntityInstance.hasAttribute(PASSPORT_NUMBER_ATTRIBUTE_TYPE));
        assertEquals(
                new StringValue("AA123456BB"),
                updatedEntityInstance
                        .getAttribute(PASSPORT_NUMBER_ATTRIBUTE_TYPE)
                        .getAttributeValue());
    }

    @Test
    @SmallTest
    public void testCommitChangesWithNoRequiredFields() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        EntityType passportTypeWithNoRequiredFields =
                new EntityType(
                        /* typeName= */ EntityTypeName.PASSPORT,
                        /* isReadOnly= */ false,
                        /* isEnabled= */ true,
                        /* isEligibleForWalletStorage= */ false,
                        /* isMaskedStorageSupported= */ true,
                        /* typeNameAsString= */ "Passport",
                        /* typeNameAsMetricsString= */ "Passport",
                        /* addEntityTypeString= */ "Add passport",
                        /* editEntityTypeString= */ "Edit passport",
                        /* deleteEntityTypeString= */ "Delete passport",
                        /* attributeTypes= */ List.of(
                                PASSPORT_NAME_ATTRIBUTE_TYPE,
                                PASSPORT_COUNTRY_ATTRIBUTE_TYPE,
                                PASSPORT_NUMBER_ATTRIBUTE_TYPE,
                                PASSPORT_ISSUE_DATE_TYPE,
                                PASSPORT_EXPIRATION_DATE_TYPE),
                        /* requiredAttributes= */ Collections.emptyList());
        EntityInstance passportEntity =
                new EntityInstance.Builder(passportTypeWithNoRequiredFields)
                        .setGuid("guid")
                        .setRecordType(RecordType.LOCAL)
                        .setIsMaskedServerEntity(false)
                        .setModifiedDate(LocalDate.of(2026, 2, 15))
                        .setUseCount(0)
                        .build();
        showEditorDialog(passportEntity);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        ListModel<EditorItem> editorFields = model.get(EntityEditorProperties.EDITOR_FIELDS);

        // Make sure there's no required fields notice by finding the entity source notice and
        // verifying that it's the only notice item.
        verifySourceNotice(
                model.get(EntityEditorProperties.EDITOR_FIELDS),
                mActivity.getString(
                        R.string.autofill_ai_save_or_update_local_entity_source_notice));
        assertEquals(
                1,
                findItemsWithType(model.get(EntityEditorProperties.EDITOR_FIELDS), NOTICE).size());

        // Click the "Done" button and make sure that the editor is closed because there are no
        // required fields in the provided entity.
        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        verify(mDelegate).onDone(mEntityInstanceCaptor.capture(), anyInt(), anyInt());
    }

    private void showEditorDialog(EntityInstance entityInstance) {
        mCoordinator = new EntityEditorCoordinator(mActivity, mDelegate, mProfile, entityInstance);
        mContainerView = mCoordinator.getEntityEditorViewForTest().getContainerView();
        mCoordinator.showEditorDialog();
    }

    private void verifyLocalPassportFields(ListModel<EditorItem> editorFields) {
        verifyTextFieldContent(
                editorFields.get(0),
                /* fieldType= */ FieldType.NAME_FULL,
                /* label= */ "Passport name",
                /* value= */ "");
        // When the country attribute is not set, the country code returned by the
        // PersonalDataManager should be used.
        verifyDropdownFieldContent(
                editorFields.get(1), /* label= */ "Passport country", /* value= */ "US");
        verifyTextFieldContent(
                editorFields.get(2),
                /* fieldType= */ FieldType.PASSPORT_NUMBER,
                /* label= */ "Passport number",
                /* value= */ "AA123456");
        verifyDateFieldContent(
                editorFields.get(3),
                /* label= */ "Issue date",
                /* value= */ LocalDate.of(2026, 2, 15).toString());
        verifyDateFieldContent(
                editorFields.get(4), /* label= */ "Expiration date", /* value= */ "");
    }

    private void verifyTextFieldContent(
            EditorItem item, @FieldType int fieldType, String label, String value) {
        assertEquals(TEXT_INPUT, item.type);
        assertEquals(fieldType, item.model.get(TEXT_FIELD_TYPE));
        assertEquals(label, item.model.get(LABEL));
        assertEquals(value, item.model.get(VALUE));
    }

    private void verifyDropdownFieldContent(EditorItem item, String label, String value) {
        assertEquals(DROPDOWN, item.type);
        // Country list gets sorted by the AutofillProfileBridge.
        assertEquals(
                List.of(
                        new DropdownKeyValue("CU", "Cuba"),
                        new DropdownKeyValue("DE", "Germany"),
                        new DropdownKeyValue("US", "United States")),
                item.model.get(DROPDOWN_KEY_VALUE_LIST));
        assertFalse(item.model.get(IS_REQUIRED));
        assertEquals(label, item.model.get(LABEL));
        assertEquals(value, item.model.get(VALUE));
    }

    private void verifyDateFieldContent(EditorItem item, String label, String value) {
        assertEquals(DATE, item.type);
        assertFalse(item.model.get(IS_REQUIRED));
        assertEquals(label, item.model.get(LABEL));
        assertEquals(value, item.model.get(VALUE));
    }

    private void verifyRequiredFieldsItem(ListModel<EditorItem> editorFields, String expectedText) {
        for (EditorItem item : editorFields) {
            if (item.type == NOTICE && expectedText.equals(item.model.get(NOTICE_TEXT))) {
                assertFalse(item.model.get(SHOW_BACKGROUND));
                assertFalse(item.model.get(IMPORTANT_FOR_ACCESSIBILITY));
                assertTrue(item.model.get(NOTICE_VISIBLE));
                assertEquals(R.style.TextAppearance_ErrorCaption, item.model.get(TEXT_APPEARANCE));
                return;
            }
        }
        fail("Required fields notice not found");
    }

    private void verifySourceNotice(ListModel<EditorItem> editorFields, String expectedNoticeText) {
        for (EditorItem item : editorFields) {
            if (item.type == NOTICE
                    && expectedNoticeText.equals(item.model.get(NOTICE_TEXT).toString())) {
                assertTrue(item.model.get(SHOW_BACKGROUND));
                assertTrue(item.model.get(IMPORTANT_FOR_ACCESSIBILITY));
                assertTrue(item.model.get(NOTICE_VISIBLE));
                return;
            }
        }
        fail("Source notice not found");
    }

    private void verifyHasExpectedBackgroundColor(View view, int expectedColor) {
        Drawable background = view.getBackground();
        assertTrue(background instanceof ColorDrawable);
        ColorDrawable colorDrawable = (ColorDrawable) background;

        assertEquals(expectedColor, colorDrawable.getColor());
    }

    private List<EditorItem> findItemsWithType(
            ListModel<EditorItem> editorFields, @ItemType int type) {
        List<EditorItem> items = new ArrayList<>();
        for (EditorItem item : editorFields) {
            if (item.type == type) {
                items.add(item);
            }
        }
        return items;
    }

    private void clickClickableSpan(CharSequence text) {
        Spanned spanned = (Spanned) text;
        ClickableSpan[] spans = spanned.getSpans(0, text.length(), ClickableSpan.class);
        assertEquals(1, spans.length);
        spans[0].onClick(null);
    }

    private void clickSourceNoticeLink() {
        PropertyModel model = mCoordinator.getEditorModelForTest();
        ListModel<EditorItem> editorFields = model.get(EntityEditorProperties.EDITOR_FIELDS);

        for (EditorItem item : editorFields) {
            if (item.type == NOTICE && item.model.get(SHOW_BACKGROUND)) {
                CharSequence noticeText = item.model.get(NOTICE_TEXT);
                clickClickableSpan(noticeText);
                return;
            }
        }
        fail("Source notice not found");
    }
}
