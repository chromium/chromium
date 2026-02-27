// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.address;

import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.CUSTOM_DONE_BUTTON_TEXT;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.DELETE_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.DELETE_CONFIRMATION_TEXT;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.DELETE_CONFIRMATION_TITLE;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.OPEN_HELP_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.SHOW_BUTTONS;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.VALIDATE_ON_SHOW;
import static org.chromium.chrome.browser.autofill.editors.address.EditorProperties.VISIBLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link EditorProperties} changes in a {@link PropertyModel} to the
 * suitable method in {@link EditorDialogView}.
 */
@NullMarked
public class EditorDialogViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link EditorDialogView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    public static void bindEditorDialogView(
            PropertyModel model, EditorDialogView view, PropertyKey propertyKey) {
        if (propertyKey == EDITOR_TITLE) {
            view.setEditorTitle(model.get(EDITOR_TITLE));
        } else if (propertyKey == CUSTOM_DONE_BUTTON_TEXT) {
            view.setCustomDoneButtonText(model.get(CUSTOM_DONE_BUTTON_TEXT));
        } else if (propertyKey == DELETE_CONFIRMATION_TITLE) {
            view.setDeleteConfirmationTitle(model.get(DELETE_CONFIRMATION_TITLE));
        } else if (propertyKey == DELETE_CONFIRMATION_TEXT) {
            view.setDeleteConfirmationText(model.get(DELETE_CONFIRMATION_TEXT));
        } else if (propertyKey == DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID) {
            view.setDeleteConfirmationPrimaryButtonText(
                    model.get(DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID));
        } else if (propertyKey == OPEN_HELP_CALLBACK) {
            view.setOpenHelpCallback(model.get(OPEN_HELP_CALLBACK));
        } else if (propertyKey == EDITOR_FIELDS) {
            view.setEditorFields(model.get(EDITOR_FIELDS));
        } else if (propertyKey == DONE_RUNNABLE) {
            view.setDoneRunnable(model.get(DONE_RUNNABLE));
        } else if (propertyKey == CANCEL_RUNNABLE) {
            view.setCancelRunnable(model.get(CANCEL_RUNNABLE));
        } else if (propertyKey == ALLOW_DELETE) {
            view.setAllowDelete(model.get(ALLOW_DELETE));
        } else if (propertyKey == DELETE_CALLBACK) {
            view.setDeleteCallback(model.get(DELETE_CALLBACK));
        } else if (propertyKey == VALIDATE_ON_SHOW) {
            view.setValidateOnShow(model.get(VALIDATE_ON_SHOW));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == SHOW_BUTTONS) {
            view.setShowButtons(model.get(SHOW_BUTTONS));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }
}
