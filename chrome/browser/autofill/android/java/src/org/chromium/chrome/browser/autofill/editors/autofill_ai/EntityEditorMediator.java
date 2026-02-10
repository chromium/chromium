// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CONFIRMATION_TEXT;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CONFIRMATION_TITLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.NOTICE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.IMPORTANT_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.SHOW_BACKGROUND;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Entity Editor. */
@NullMarked
class EntityEditorMediator {
    private final Context mContext;
    private final Delegate mDelegate;
    private final IdentityManager mIdentityManager;
    private @Nullable PropertyModel mEditorModel;

    EntityEditorMediator(Context context, Delegate delegate, IdentityManager identityManager) {
        mContext = context;
        mDelegate = delegate;
        mIdentityManager = identityManager;
    }

    @EnsuresNonNull("mEditorModel")
    PropertyModel getEditorModel(EntityInstance entityInstance) {
        if (mEditorModel == null) {
            mEditorModel =
                    new PropertyModel.Builder(EntityEditorProperties.ALL_KEYS)
                            .with(
                                    EDITOR_TITLE,
                                    entityInstance.getEntityType().getAddEntityTypeString())
                            .with(DONE_RUNNABLE, this::onDone)
                            .with(CANCEL_RUNNABLE, this::onCancel)
                            .with(
                                    DELETE_CONFIRMATION_TITLE,
                                    entityInstance.getEntityType().getDeleteEntityTypeString())
                            .with(
                                    DELETE_CONFIRMATION_TEXT,
                                    mContext.getString(
                                            R.string
                                                    .autofill_ai_entity_editor_delete_local_entity_dialog_text))
                            .with(
                                    DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID,
                                    R.string.autofill_delete_suggestion_button)
                            .with(DELETE_RUNNABLE, () -> mDelegate.onDelete(entityInstance))
                            .with(ALLOW_DELETE, entityInstance.getRecordType() == RecordType.LOCAL)
                            .with(EDITOR_FIELDS, getEditorFields(entityInstance))
                            .build();
        }
        return mEditorModel;
    }

    private void onCancel() {
        assumeNonNull(mEditorModel).set(EntityEditorProperties.VISIBLE, false);
    }

    private void onDone() {
        assumeNonNull(mEditorModel).set(EntityEditorProperties.VISIBLE, false);
    }

    private ListModel<EditorItem> getEditorFields(EntityInstance entityInstance) {
        ListModel<EditorItem> editorFields = new ListModel<>();
        maybeAddEntitySourceNoticeItem(editorFields, entityInstance.getRecordType());
        return editorFields;
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
