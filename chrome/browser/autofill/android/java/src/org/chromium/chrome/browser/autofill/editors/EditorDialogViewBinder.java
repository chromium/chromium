// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CUSTOM_DONE_BUTTON_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FOOTER_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.SHOW_REQUIRED_INDICATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TRIGGER_DONE_CALLBACK_BEFORE_CLOSE_ANIMATION;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VISIBLE;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link EditorProperties} changes in a {@link PropertyModel} to
 * the suitable method in {@link EditorDialogView}.
 */
public class EditorDialogViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
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
        } else if (propertyKey == FOOTER_MESSAGE) {
            view.setFooterMessage(model.get(FOOTER_MESSAGE));
        } else if (propertyKey == DELETE_CONFIRMATION_TITLE) {
            view.setDeleteConfirmationTitle(model.get(DELETE_CONFIRMATION_TITLE));
        } else if (propertyKey == DELETE_CONFIRMATION_TEXT) {
            view.setDeleteConfirmationText(model.get(DELETE_CONFIRMATION_TEXT));
        } else if (propertyKey == SHOW_REQUIRED_INDICATOR) {
            view.setShowRequiredIndicator(model.get(SHOW_REQUIRED_INDICATOR));
        } else if (propertyKey == TRIGGER_DONE_CALLBACK_BEFORE_CLOSE_ANIMATION) {
            view.setShouldTriggerDoneCallbackBeforeCloseAnimation(
                    model.get(TRIGGER_DONE_CALLBACK_BEFORE_CLOSE_ANIMATION));
        } else if (propertyKey == EDITOR_FIELDS) {
            view.setEditorFields(model.get(EDITOR_FIELDS), model.get(SHOW_REQUIRED_INDICATOR));
        } else if (propertyKey == DONE_RUNNABLE) {
            view.setDoneRunnable(model.get(DONE_RUNNABLE));
        } else if (propertyKey == CANCEL_RUNNABLE) {
            view.setCancelRunnable(model.get(CANCEL_RUNNABLE));
        } else if (propertyKey == ALLOW_DELETE) {
            view.setAllowDelete(model.get(ALLOW_DELETE));
        } else if (propertyKey == DELETE_RUNNABLE) {
            view.setDeleteRunnable(model.get(DELETE_RUNNABLE));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }
}
