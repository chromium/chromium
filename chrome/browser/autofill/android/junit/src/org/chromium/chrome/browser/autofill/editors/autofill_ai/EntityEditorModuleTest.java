// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.DATE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.NOTICE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.IMPORTANT_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.SHOW_BACKGROUND;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldProperties.TEXT_FIELD_TYPE;

import android.app.Activity;
import android.view.View;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.AutofillProfileBridgeJni;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.chrome.browser.autofill.editors.common.EditorDialogToolbar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.autofill.DropdownKeyValue;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.autofill_ai.AttributeInstance;
import org.chromium.components.autofill.autofill_ai.AttributeInstance.StringValue;
import org.chromium.components.autofill.autofill_ai.AttributeType;
import org.chromium.components.autofill.autofill_ai.AttributeTypeName;
import org.chromium.components.autofill.autofill_ai.DataType;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.time.LocalDate;
import java.time.ZoneId;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
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
                            PASSPORT_EXPIRATION_DATE_TYPE));

    private static final EntityInstance LOCAL_PASSPORT =
            new EntityInstance.Builder(PASSPORT_TYPE)
                    .setGUID("guid")
                    .setRecordType(RecordType.LOCAL)
                    .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
                    .setUseCount(0)
                    .addAttribute(
                            new AttributeInstance(
                                    PASSPORT_NUMBER_ATTRIBUTE_TYPE, /* value= */ "AA123456"))
                    .addAttribute(
                            new AttributeInstance(
                                    PASSPORT_ISSUE_DATE_TYPE, /* value= */ "2026-02-15"))
                    .build();

    private static final EntityInstance NEW_LOCAL_PASSPORT =
            new EntityInstance.Builder(PASSPORT_TYPE)
                    .setGUID("")
                    .setRecordType(RecordType.LOCAL)
                    .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
                    .setUseCount(0)
                    .build();

    private static final EntityInstance WALLET_PASSPORT =
            new EntityInstance.Builder(PASSPORT_TYPE)
                    .setGUID("guid")
                    .setRecordType(RecordType.SERVER_WALLET)
                    .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
                    .setUseCount(0)
                    .addAttribute(
                            new AttributeInstance(
                                    PASSPORT_NAME_ATTRIBUTE_TYPE, /* value= */ "John Doe"))
                    .addAttribute(
                            new AttributeInstance(
                                    PASSPORT_COUNTRY_ATTRIBUTE_TYPE, /* value= */ "Germany"))
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Mock private Delegate mDelegate;
    @Mock private Profile mProfile;
    @Mock private IdentityManager mIdentityManager;

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

        mActivity = Robolectric.setupActivity(TestActivity.class);
    }

    @Test
    @SmallTest
    public void testShowEditorDialogForNewEntity() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(NEW_LOCAL_PASSPORT);
        EditorDialogToolbar toolbar = mContainerView.findViewById(R.id.action_bar);
        assertEquals(PASSPORT_TYPE.getAddEntityTypeString(), toolbar.getTitle());
        assertTrue(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void testShowEditorDialogForExistingEntity() {
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        showEditorDialog(LOCAL_PASSPORT);
        EditorDialogToolbar toolbar = mContainerView.findViewById(R.id.action_bar);
        assertEquals(PASSPORT_TYPE.getEditEntityTypeString(), toolbar.getTitle());
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
                mActivity.getString(R.string.autofill_ai_local_entity_editor_source_notice));
    }

    @Test
    @SmallTest
    public void testWalletEntitySourceNotice() {
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(mAccountInfo);
        showEditorDialog(WALLET_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        verifySourceNotice(
                model.get(EntityEditorProperties.EDITOR_FIELDS),
                mActivity
                        .getString(R.string.autofill_ai_wallet_entity_editor_source_notice)
                        .replace("$1", USER_EMAIL));
    }

    @Test
    @SmallTest
    public void testCommitChanges() {
        EntityInstance entity =
                new EntityInstance.Builder(PASSPORT_TYPE)
                        .setGUID("guid")
                        .setRecordType(RecordType.LOCAL)
                        .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
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
        verify(mDelegate).onDone(mEntityInstanceCaptor.capture());
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
        verifyDateFieldContent(editorFields.get(3), /* label= */ "Issue date");
        verifyDateFieldContent(editorFields.get(4), /* label= */ "Expiration date");
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

    private void verifyDateFieldContent(EditorItem item, String label) {
        assertEquals(DATE, item.type);
        assertFalse(item.model.get(IS_REQUIRED));
        assertEquals(label, item.model.get(LABEL));
    }

    private void verifySourceNotice(ListModel<EditorItem> editorFields, String expectedNoticeText) {
        for (EditorItem item : editorFields) {
            if (item.type == NOTICE && expectedNoticeText.equals(item.model.get(NOTICE_TEXT))) {
                assertTrue(item.model.get(SHOW_BACKGROUND));
                assertTrue(item.model.get(IMPORTANT_FOR_ACCESSIBILITY));
                return;
            }
        }
        fail("Source notice not found");
    }
}
