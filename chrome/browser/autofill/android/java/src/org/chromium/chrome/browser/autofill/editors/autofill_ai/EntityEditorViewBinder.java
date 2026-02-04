// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.DONE_RUNNABLE;
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
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }
}
