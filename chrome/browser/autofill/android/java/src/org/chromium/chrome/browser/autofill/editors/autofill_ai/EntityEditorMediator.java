// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CONFIRMATION_TEXT;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CONFIRMATION_TITLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.DATE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.NOTICE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.IMPORTANT_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.SHOW_BACKGROUND;
import static org.chromium.chrome.browser.autofill.editors.common.date_field.DateFieldProperties.DATE_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldProperties.TEXT_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldProperties.TEXT_FIELD_TYPE;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.components.autofill.autofill_ai.AttributeInstance;
import org.chromium.components.autofill.autofill_ai.AttributeType;
import org.chromium.components.autofill.autofill_ai.DataType;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashMap;
import java.util.Map;

/** Mediator for the Entity Editor. */
@NullMarked
class EntityEditorMediator {
    @VisibleForTesting
    static final String ENTITY_DELETED_HISTOGRAM = "Autofill.Ai.EntityDeleted.Any.";

    @VisibleForTesting
    static final String ENTITY_DELETED_SETTINGS_HISTOGRAM = "Autofill.Ai.EntityDeleted.Settings.";

    private final Context mContext;
    private final Delegate mDelegate;
    private final IdentityManager mIdentityManager;
    private final PersonalDataManager mPersonalDataManager;
    private final EntityInstance mEntityInstance;
    private final PropertyModel mEditorModel;
    private final Map<AttributeType, PropertyModel> mAttributeFields = new HashMap<>();

    EntityEditorMediator(
            Context context,
            Delegate delegate,
            IdentityManager identityManager,
            PersonalDataManager personalDataManager,
            EntityInstance entityInstance) {
        mContext = context;
        mDelegate = delegate;
        mIdentityManager = identityManager;
        mPersonalDataManager = personalDataManager;
        mEntityInstance = entityInstance;
        mEditorModel = buildEditorModel();
    }

    PropertyModel getEditorModel() {
        return mEditorModel;
    }

    private PropertyModel buildEditorModel() {
        String editorTitle =
                TextUtils.isEmpty(mEntityInstance.getGUID())
                        ? mEntityInstance.getEntityType().getAddEntityTypeString()
                        : mEntityInstance.getEntityType().getEditEntityTypeString();
        return new PropertyModel.Builder(EntityEditorProperties.ALL_KEYS)
                .with(EDITOR_TITLE, editorTitle)
                .with(DONE_RUNNABLE, this::onDone)
                .with(CANCEL_RUNNABLE, this::onCancel)
                .with(
                        DELETE_CONFIRMATION_TITLE,
                        mEntityInstance.getEntityType().getDeleteEntityTypeString())
                .with(
                        DELETE_CONFIRMATION_TEXT,
                        mContext.getString(
                                R.string.autofill_ai_entity_editor_delete_local_entity_dialog_text))
                .with(
                        DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID,
                        R.string.autofill_delete_suggestion_button)
                .with(DELETE_CALLBACK, this::onDelete)
                .with(ALLOW_DELETE, mEntityInstance.getRecordType() == RecordType.LOCAL)
                .with(EDITOR_FIELDS, getEditorFields())
                .build();
    }

    private void onDelete(boolean userConfirmedDeletion) {
        RecordHistogram.recordBooleanHistogram(
                ENTITY_DELETED_HISTOGRAM + mEntityInstance.getEntityType().getTypeNameAsString(),
                userConfirmedDeletion);
        RecordHistogram.recordBooleanHistogram(
                ENTITY_DELETED_SETTINGS_HISTOGRAM
                        + mEntityInstance.getEntityType().getTypeNameAsString(),
                userConfirmedDeletion);
        if (userConfirmedDeletion) {
            mDelegate.onDelete(mEntityInstance);
        }
    }

    private void onCancel() {
        assumeNonNull(mEditorModel).set(EntityEditorProperties.VISIBLE, false);
    }

    private void onDone() {
        assumeNonNull(mEditorModel).set(EntityEditorProperties.VISIBLE, false);
        commitChanges();
        mDelegate.onDone(mEntityInstance);
    }

    private void commitChanges() {
        for (Map.Entry<AttributeType, PropertyModel> entry : mAttributeFields.entrySet()) {
            mEntityInstance.setAttributeValue(entry.getKey(), entry.getValue().get(VALUE));
        }
    }

    private ListModel<EditorItem> getEditorFields() {
        mAttributeFields.clear();
        ListModel<EditorItem> editorFields = new ListModel<>();
        for (AttributeType attributeType : mEntityInstance.getEntityType().getAttributeTypes()) {
            switch (attributeType.getDataType()) {
                case DataType.NAME:
                case DataType.STATE:
                case DataType.STRING:
                    EditorItem stringItem = getTextFieldItem(mEntityInstance, attributeType);
                    editorFields.add(stringItem);
                    mAttributeFields.put(attributeType, stringItem.model);
                    break;
                case DataType.COUNTRY:
                    EditorItem countryItem = getCountryDropdownItem(mEntityInstance, attributeType);
                    editorFields.add(countryItem);
                    mAttributeFields.put(attributeType, countryItem.model);
                    break;
                case DataType.DATE:
                    editorFields.add(getDateDropdown(attributeType));
                    break;
                default:
                    assert false
                            : "Unhandled entity attribute data type: "
                                    + attributeType.getDataType();
            }
        }
        maybeAddEntitySourceNoticeItem(editorFields, mEntityInstance.getRecordType());
        return editorFields;
    }

    private EditorItem getTextFieldItem(
            EntityInstance entityInstance, AttributeType attributeType) {
        String value = getStringAttribute(entityInstance, attributeType);
        return new EditorItem(
                TEXT_INPUT,
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(LABEL, attributeType.getTypeNameAsString())
                        .with(TEXT_FIELD_TYPE, attributeType.getFieldType())
                        .with(VALUE, value)
                        .build(),
                /* isFullLine= */ true);
    }

    private EditorItem getCountryDropdownItem(
            EntityInstance entityInstance, AttributeType attributeType) {
        String value = getStringAttribute(entityInstance, attributeType);
        if (TextUtils.isEmpty(value)) {
            value = mPersonalDataManager.getDefaultCountryCodeForNewAddress();
        }
        return new EditorItem(
                DROPDOWN,
                new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                        .with(LABEL, attributeType.getTypeNameAsString())
                        .with(
                                DROPDOWN_KEY_VALUE_LIST,
                                AutofillProfileBridge.getSupportedCountries())
                        .with(IS_REQUIRED, false)
                        .with(VALUE, value)
                        .build());
    }

    private EditorItem getDateDropdown(AttributeType attributeType) {
        return new EditorItem(
                DATE,
                new PropertyModel.Builder(DATE_ALL_KEYS)
                        .with(LABEL, attributeType.getTypeNameAsString())
                        .with(IS_REQUIRED, false)
                        .build(),
                /* isFullLine= */ true);
    }

    private String getStringAttribute(EntityInstance entityInstance, AttributeType attributeType) {
        @Nullable AttributeInstance attribute = entityInstance.getAttribute(attributeType);
        String attributeValue = "";
        if (attribute != null) {
            assert attribute.getAttributeValue() instanceof AttributeInstance.StringValue;
            attributeValue =
                    ((AttributeInstance.StringValue) attribute.getAttributeValue()).getValue();
        }
        return attributeValue;
    }

    private void maybeAddEntitySourceNoticeItem(
            ListModel<EditorItem> editorFields, @RecordType int recordType) {
        String sourceNotice = getEntitySourceNotice(recordType);
        if (TextUtils.isEmpty(sourceNotice)) {
            return;
        }
        editorFields.add(
                new EditorItem(
                        NOTICE,
                        new PropertyModel.Builder(NOTICE_ALL_KEYS)
                                .with(NOTICE_TEXT, getEntitySourceNotice(recordType))
                                .with(SHOW_BACKGROUND, true)
                                .with(IMPORTANT_FOR_ACCESSIBILITY, true)
                                .build(),
                        /* isFullLine= */ true));
    }

    private String getEntitySourceNotice(@RecordType int recordType) {
        switch (recordType) {
            case RecordType.LOCAL:
                return mContext.getString(R.string.autofill_ai_local_entity_editor_source_notice);
            case RecordType.SERVER_WALLET:
                String email = getUserEmail();
                return email == null
                        ? ""
                        : mContext.getString(
                                        R.string.autofill_ai_wallet_entity_editor_source_notice)
                                .replace("$1", email);
        }
        assert false : "Invalid entity record type: " + recordType;
        return "";
    }

    private @Nullable String getUserEmail() {
        CoreAccountInfo accountInfo = mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        return CoreAccountInfo.getEmailFrom(accountInfo);
    }
}
