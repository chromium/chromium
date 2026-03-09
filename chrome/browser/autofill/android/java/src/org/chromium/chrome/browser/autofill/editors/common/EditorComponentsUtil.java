// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common;

import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.isEditable;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.FOCUSED;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.ui.modelutil.ListModel;

@NullMarked
public class EditorComponentsUtil {

    public static void scrollToFieldWithErrorMessage(ListModel<EditorItem> editorFields) {
        // Check if a field with an error is already focused.
        for (EditorItem item : editorFields) {
            if (!isEditable(item)) {
                continue;
            }
            if (item.model.get(FOCUSED) && item.model.get(ERROR_MESSAGE) != null) {
                // Hack: Although the field is focused, it may be off screen. Toggle FOCUSED in
                // order to scroll the field into view.
                item.model.set(FOCUSED, false);
                item.model.set(FOCUSED, true);
                return;
            }
        }

        // Focus first field with an error.
        for (EditorItem item : editorFields) {
            if (!isEditable(item)) {
                continue;
            }
            if (item.model.get(ERROR_MESSAGE) != null) {
                // Hack: some fields do not clear the FOCUSED property.
                // TODO: crbug.com/490311866 - Remove this hack.
                item.model.set(FOCUSED, false);
                item.model.set(FOCUSED, true);
                break;
            }
        }
    }
}
