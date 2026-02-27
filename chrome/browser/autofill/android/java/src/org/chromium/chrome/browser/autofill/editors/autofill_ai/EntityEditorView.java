// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.editors.common.EditorViewBase;

/** The entity editor dialog. Can be used for editing passports, identity cards, etc. */
@NullMarked
public class EntityEditorView extends EditorViewBase {
    /**
     * Builds the editor dialog.
     *
     * @param activity The activity on top of which the UI should be displayed.
     * @param profile The Profile being edited.
     */
    public EntityEditorView(Activity activity) {
        super(activity);
    }

    @Override
    protected void initFocus() {
        // TODO: crbug.com/476755159 - Implement.
    }
}
