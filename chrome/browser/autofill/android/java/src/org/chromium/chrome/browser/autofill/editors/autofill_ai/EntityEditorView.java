// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.chrome.browser.autofill.editors.common.EditorViewBase;
import org.chromium.ui.modelutil.ListModel;

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
    protected void prepareEditor(ListModel<EditorItem> editorFields) {
        // TODO: crbug.com/476755159 - Implement.
    }

    @Override
    protected void onEntryAnimationStart() {
        // TODO: crbug.com/476755159 - Implement.
    }

    @Override
    protected void onEntryAnimationEnd() {
        // TODO: crbug.com/476755159 - Implement.
    }

    @Override
    protected void initFocus() {
        // TODO: crbug.com/476755159 - Implement.
    }

    @Override
    protected void recordDeletionHistogram(boolean deleted) {
        // TODO: crbug.com/476755159 - Record deletion histograms.
    }
}
