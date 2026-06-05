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
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.OPEN_HELP_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.TOOLBAR_BRANDING_ICON_ID;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.VALIDATE_ON_SHOW;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.DATE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.NOTICE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.IMPORTANT_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_VISIBLE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.SHOW_BACKGROUND;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.TEXT_APPEARANCE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.isEditable;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.validateForm;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsUtil.scrollToFieldWithErrorMessage;
import static org.chromium.chrome.browser.autofill.editors.common.date_field.DateFieldProperties.DATE_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE_CHANGED_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldProperties.TEXT_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.common.text_field.TextFieldProperties.TEXT_FIELD_TYPE;

import android.app.Activity;
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
import org.chromium.chrome.browser.autofill.editors.common.date_field.DateFieldValidator;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.VerificationStatus;
import org.chromium.components.autofill.autofill_ai.AttributeInstance;
import org.chromium.components.autofill.autofill_ai.AttributeType;
import org.chromium.components.autofill.autofill_ai.DataType;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
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
    private final Profile mProfile;
    private final IdentityManager mIdentityManager;
    private final PersonalDataManager mPersonalDataManager;
    private final EntityInstance mEntityInstance;
    private final PropertyModel mEditorModel;
    private final Map<AttributeType, PropertyModel> mAttributeFields = new HashMap<>();
    private @Nullable EditorItem mRequiredSourceNotice;

    EntityEditorMediator(
            Context context,
            Delegate delegate,
            Profile profile,
            IdentityManager identityManager,
            PersonalDataManager personalDataManager,
            EntityInstance entityInstance) {
        mContext = context;
        mDelegate = delegate;
        mProfile = profile;
        mIdentityManager = identityManager;
        mPersonalDataManager = personalDataManager;
        mEntityInstance = entityInstance;
        mEditorModel = buildEditorModel();
    }

    PropertyModel getEditorModel() {
        return mEditorModel;
    }

    private PropertyModel buildEditorModel() {
        final boolean isNewEntity = TextUtils.isEmpty(mEntityInstance.getGuid());
        final boolean isNewWalletEntity =
                isNewEntity && mEntityInstance.getRecordType() == RecordType.SERVER_WALLET;
        String editorTitle =
                isNewEntity
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
                .with(
                        ALLOW_DELETE,
                        !isNewEntity && mEntityInstance.getRecordType() == RecordType.LOCAL)
                .with(VALIDATE_ON_SHOW, false)
                .with(EDITOR_FIELDS, getEditorFields())
                .with(OPEN_HELP_CALLBACK, this::onOpenHelpAndFeedback)
                .with(
                        TOOLBAR_BRANDING_ICON_ID,
                        isNewWalletEntity ? R.layout.autofill_editor_toolbar_icon : 0)
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
        if (!validateEntityForm()) {
            scrollToFieldWithErrorMessage(mEditorModel.get(EDITOR_FIELDS));
            return;
        }
        assumeNonNull(mEditorModel).set(EntityEditorProperties.VISIBLE, false);
        commitChanges();
        mDelegate.onDone(
                mEntityInstance,
                mEntityInstance.getRecordType() == RecordType.LOCAL
                        ? R.string.autofill_ai_save_or_update_local_entity_source_notice
                        : R.string.autofill_ai_save_or_update_entity_in_wallet_source_notice,
                R.string.done);
    }

    /**
     * The entity editor makes sure at least 1 of the required fields are non-empty. All required
     * fields must be non-empty in other editors.
     *
     * @return {@code true} if any of the required fields are non-empty, {@code false} otherwise.
     */
    private boolean validateEntityForm() {
        List<EditorItem> emptyRequiredFields = new ArrayList<>();
        boolean hasRequiredFields = false;
        boolean hasNonEmptyRequiredField = false;
        for (EditorItem editorItem : mEditorModel.get(EDITOR_FIELDS)) {
            if (!isEditable(editorItem)) {
                continue;
            }
            if (editorItem.model.get(IS_REQUIRED)) {
                hasRequiredFields = true;
                if (TextUtils.isEmpty(editorItem.model.get(VALUE))
                        || TextUtils.getTrimmedLength(editorItem.model.get(VALUE)) == 0) {
                    // Collect all required fields that are empty.
                    emptyRequiredFields.add(editorItem);
                } else {
                    // Stop the process if at least 1 required field is non-empty.
                    hasNonEmptyRequiredField = true;
                    break;
                }
            }
        }
        // Run other validators in any case so set the corresponding error messages.
        final boolean isFormValid = validateForm(mEditorModel.get(EDITOR_FIELDS));
        if (hasRequiredFields && !hasNonEmptyRequiredField) {
            for (EditorItem editorItem : emptyRequiredFields) {
                editorItem.model.set(
                        ERROR_MESSAGE, getRequiredFieldErrorMessage(editorItem.model.get(LABEL)));
            }
            if (mRequiredSourceNotice != null) {
                mRequiredSourceNotice.model.set(NOTICE_VISIBLE, true);
            }
            return false;
        }
        if (isFormValid) {
            // Hide the error messages even though the editor is about to be hidden as well.
            resetErrorMessages();
        }
        return isFormValid;
    }

    private void resetErrorMessages() {
        if (mRequiredSourceNotice != null) {
            mRequiredSourceNotice.model.set(NOTICE_VISIBLE, false);
        }
        for (EditorItem editorItem : mEditorModel.get(EDITOR_FIELDS)) {
            if (!isEditable(editorItem)) {
                continue;
            }
            editorItem.model.set(ERROR_MESSAGE, "");
        }
    }

    private void commitChanges() {
        for (Map.Entry<AttributeType, PropertyModel> entry : mAttributeFields.entrySet()) {
            String sanitizedValue = entry.getValue().get(VALUE).trim();
            if (TextUtils.isEmpty(sanitizedValue)
                    && !mEntityInstance.hasAttribute(entry.getKey())) {
                // Do not populate the EntityInstance with empty attribute values if they didn't
                // exist before.
                continue;
            }
            mEntityInstance.setAttributeValue(
                    entry.getKey(), sanitizedValue, VerificationStatus.USER_VERIFIED);
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
                    EditorItem dateItem = getDateDropdown(mEntityInstance, attributeType);
                    editorFields.add(dateItem);
                    mAttributeFields.put(attributeType, dateItem.model);
                    break;
                default:
                    assert false
                            : "Unhandled entity attribute data type: "
                                    + attributeType.getDataType();
            }
        }

        maybeAddRequiredFieldsNoticeItem(editorFields);
        maybeAddEntitySourceNoticeItem(
                editorFields,
                mEntityInstance.getRecordType(),
                mEntityInstance.isMaskedServerEntity());
        return editorFields;
    }

    private EditorItem getTextFieldItem(
            EntityInstance entityInstance, AttributeType attributeType) {
        String value = getStringAttribute(entityInstance, attributeType);
        PropertyModel itemModel =
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(LABEL, attributeType.getTypeNameAsString())
                        .with(TEXT_FIELD_TYPE, attributeType.getFieldType())
                        .with(VALUE, value)
                        .with(
                                IS_REQUIRED,
                                entityInstance.getEntityType().isRequiredAttribute(attributeType))
                        .build();
        itemModel.set(VALUE_CHANGED_CALLBACK, (unused) -> onFieldValueChanged(itemModel));
        return new EditorItem(TEXT_INPUT, itemModel, /* isFullLine= */ true);
    }

    private EditorItem getCountryDropdownItem(
            EntityInstance entityInstance, AttributeType attributeType) {
        String value = getStringAttribute(entityInstance, attributeType);
        if (TextUtils.isEmpty(value)) {
            value = mPersonalDataManager.getDefaultCountryCodeForNewAddress();
        }
        PropertyModel itemModel =
                new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                        .with(LABEL, attributeType.getTypeNameAsString())
                        .with(
                                DROPDOWN_KEY_VALUE_LIST,
                                AutofillProfileBridge.getSupportedCountries())
                        .with(
                                IS_REQUIRED,
                                entityInstance.getEntityType().isRequiredAttribute(attributeType))
                        .with(VALUE, value)
                        .build();
        itemModel.set(VALUE_CHANGED_CALLBACK, (unused) -> onFieldValueChanged(itemModel));
        return new EditorItem(DROPDOWN, itemModel, /* isFullLine= */ true);
    }

    private EditorItem getDateDropdown(EntityInstance entityInstance, AttributeType attributeType) {
        @Nullable AttributeInstance attribute = entityInstance.getAttribute(attributeType);
        String attributeValue = "";
        if (attribute != null) {
            assert attribute.getAttributeValue() instanceof AttributeInstance.DateValue;
            attributeValue =
                    ((AttributeInstance.DateValue) attribute.getAttributeValue()).toString();
        }
        PropertyModel itemModel =
                new PropertyModel.Builder(DATE_ALL_KEYS)
                        .with(LABEL, attributeType.getTypeNameAsString())
                        .with(
                                IS_REQUIRED,
                                entityInstance.getEntityType().isRequiredAttribute(attributeType))
                        .with(VALUE, attributeValue)
                        .with(
                                VALIDATOR,
                                new DateFieldValidator(
                                        mContext.getString(
                                                R.string
                                                        .autofill_ai_entity_editor_invalid_date_error_message)))
                        .build();
        itemModel.set(VALUE_CHANGED_CALLBACK, (unused) -> onFieldValueChanged(itemModel));
        return new EditorItem(DATE, itemModel, /* isFullLine= */ true);
    }

    private void onFieldValueChanged(PropertyModel itemModel) {
        // Reset error messages on required fields if the required field value changes.
        if (itemModel.get(IS_REQUIRED)) {
            for (EditorItem item : mEditorModel.get(EDITOR_FIELDS)) {
                if (isEditable(item) && item.model.get(IS_REQUIRED)) {
                    item.model.set(ERROR_MESSAGE, "");
                }
            }
            // Hide the notice as well.
            if (mRequiredSourceNotice != null) {
                mRequiredSourceNotice.model.set(NOTICE_VISIBLE, false);
            }
        }
    }

    private String getRequiredFieldErrorMessage(String label) {
        return mContext.getString(R.string.autofill_ai_entity_editor_required_field_error_message)
                .replace("$1", label);
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

    private void maybeAddRequiredFieldsNoticeItem(ListModel<EditorItem> editorFields) {
        List<EditorItem> requiredFields = new ArrayList<>();
        for (EditorItem editorItem : editorFields) {
            if (editorItem.model.get(IS_REQUIRED)) {
                requiredFields.add(editorItem);
            }
        }
        String notice = getRequiredFieldsNotice(requiredFields);
        if (!TextUtils.isEmpty(notice)) {
            mRequiredSourceNotice = getRequiredFieldsNoticeItem(notice);
            editorFields.add(mRequiredSourceNotice);
        }
    }

    private String getRequiredFieldsNotice(List<EditorItem> requiredFields) {
        switch (requiredFields.size()) {
            case 0:
                return "";
            case 1:
                final String noticeText1 =
                        mContext.getString(
                                R.string
                                        .autofill_ai_entity_editor_single_required_field_error_message);
                return noticeText1.replace("$1", requiredFields.get(0).model.get(LABEL));
            case 2:
                final String noticeText2 =
                        mContext.getString(
                                R.string
                                        .autofill_ai_entity_editor_two_required_fields_error_message);
                return noticeText2
                        .replace("$1", requiredFields.get(0).model.get(LABEL))
                        .replace("$2", requiredFields.get(1).model.get(LABEL));
            case 3:
                final String noticeText3 =
                        mContext.getString(
                                R.string
                                        .autofill_ai_entity_editor_three_required_fields_error_message);
                return noticeText3
                        .replace("$1", requiredFields.get(0).model.get(LABEL))
                        .replace("$2", requiredFields.get(1).model.get(LABEL))
                        .replace("$3", requiredFields.get(2).model.get(LABEL));
            default:
                return "";
        }
    }

    private EditorItem getRequiredFieldsNoticeItem(String noticeText) {
        return new EditorItem(
                NOTICE,
                new PropertyModel.Builder(NOTICE_ALL_KEYS)
                        .with(NOTICE_TEXT, noticeText)
                        .with(SHOW_BACKGROUND, false)
                        // Required fields are indicated by an asterisk (*) and
                        // announced separately by screen readers. Don't announce
                        // the message itself.
                        .with(IMPORTANT_FOR_ACCESSIBILITY, false)
                        .with(NOTICE_VISIBLE, false)
                        .with(TEXT_APPEARANCE, R.style.TextAppearance_ErrorCaption)
                        .build(),
                /* isFullLine= */ true);
    }

    private void maybeAddEntitySourceNoticeItem(
            ListModel<EditorItem> editorFields,
            @RecordType int recordType,
            boolean isPrivateEntity) {
        CharSequence sourceNotice = getEntitySourceNotice(recordType, isPrivateEntity);
        if (TextUtils.isEmpty(sourceNotice)) {
            return;
        }
        editorFields.add(
                new EditorItem(
                        NOTICE,
                        new PropertyModel.Builder(NOTICE_ALL_KEYS)
                                .with(NOTICE_TEXT, sourceNotice)
                                .with(SHOW_BACKGROUND, true)
                                .with(IMPORTANT_FOR_ACCESSIBILITY, true)
                                .with(NOTICE_VISIBLE, true)
                                .build(),
                        /* isFullLine= */ true));
    }

    private CharSequence getEntitySourceNotice(
            @RecordType int recordType, boolean isPrivateEntity) {
        switch (recordType) {
            case RecordType.LOCAL:
                return mContext.getString(
                        R.string.autofill_ai_save_or_update_local_entity_source_notice);
            case RecordType.SERVER_WALLET:
                String email = getUserEmail();
                if (email == null) {
                    return "";
                }
                String walletTitle = mContext.getString(R.string.autofill_google_wallet_title);
                String sourceNotice =
                        mContext.getString(
                                        R.string
                                                .autofill_ai_save_or_update_entity_in_wallet_source_notice)
                                .replace("$1", walletTitle)
                                .replace("$2", walletTitle)
                                .replace("$3", email);
                return SpanApplier.applySpans(
                        sourceNotice,
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        mContext,
                                        view -> mDelegate.onOpenGoogleWallet(isPrivateEntity))));
        }
        assert false : "Invalid entity record type: " + recordType;
        return "";
    }

    private @Nullable String getUserEmail() {
        return AccountInfo.getEmailFrom(mIdentityManager.getPrimaryAccountInfo());
    }

    private void onOpenHelpAndFeedback(Activity activity) {
        HelpAndFeedbackLauncherFactory.getForProfile(mProfile)
                .show(activity, activity.getString(R.string.help_context_autofill), null);
    }
}
