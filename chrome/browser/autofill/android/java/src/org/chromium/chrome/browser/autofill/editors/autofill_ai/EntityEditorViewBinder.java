// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CONFIRMATION_TEXT;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DELETE_CONFIRMATION_TITLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.VISIBLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link EntityEditorProperties} changes in a {@link PropertyModel} to
 * the suitable method in {@link EntityEditorView}.
 */
@NullMarked
public class EntityEditorViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link EntityEditorView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    public static void bindEditorDialogView(
            PropertyModel model, EntityEditorView view, PropertyKey propertyKey) {
        if (propertyKey == EDITOR_TITLE) {
            view.setEditorTitle(model.get(EDITOR_TITLE));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == DONE_RUNNABLE) {
            view.setDoneRunnable(model.get(DONE_RUNNABLE));
        } else if (propertyKey == CANCEL_RUNNABLE) {
            view.setCancelRunnable(model.get(CANCEL_RUNNABLE));
        } else if (propertyKey == DELETE_CONFIRMATION_TITLE) {
            view.setDeleteConfirmationTitle(model.get(DELETE_CONFIRMATION_TITLE));
        } else if (propertyKey == DELETE_CONFIRMATION_TEXT) {
            view.setDeleteConfirmationText(model.get(DELETE_CONFIRMATION_TEXT));
        } else if (propertyKey == DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID) {
            view.setDeleteConfirmationPrimaryButtonText(
                    model.get(DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID));
        } else if (propertyKey == DELETE_CALLBACK) {
            view.setDeleteCallback(model.get(DELETE_CALLBACK));
        } else if (propertyKey == ALLOW_DELETE) {
            view.setAllowDelete(model.get(ALLOW_DELETE));
        } else if (propertyKey == EDITOR_FIELDS) {
            view.setEditorFields(model.get(EDITOR_FIELDS));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }
}
