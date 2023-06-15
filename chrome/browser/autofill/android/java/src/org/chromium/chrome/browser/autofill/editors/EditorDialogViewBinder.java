// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;

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
        if (propertyKey == EDITOR_FIELDS) {
            view.setEditorFields(model.get(EDITOR_FIELDS));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }
}
