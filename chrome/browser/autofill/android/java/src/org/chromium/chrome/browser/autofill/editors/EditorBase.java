// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.EditableOption;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The base class for an editor controller.
 *
 * @param <T> the class which extends EditableOption
 */
@NullMarked
public abstract class EditorBase<T extends EditableOption> {
    protected EditorDialogView mEditorDialog;
    protected Context mContext;
    protected @Nullable PropertyModel mEditorModel;
    protected @Nullable PropertyModelChangeProcessor<PropertyModel, EditorDialogView, PropertyKey>
            mEditorMCP;

    /**
     * Sets the user interface to be used for editing contact information.
     *
     * @param editorDialog The user interface to be used.
     */
    @Initializer
    public void setEditorDialog(EditorDialogView editorDialog) {
        mEditorDialog = editorDialog;
        mContext = mEditorDialog.getContext();
    }

    /**
     * Shows the user interface for editing the given information.
     *
     * @param toEdit The information to edit. Can be null if the user is adding new information
     *     instead of editing an existing one.
     * @param doneCallback The callback to invoke when confirming the edit dialog, with the complete
     *     and valid information.
     * @param cancelCallback The callback to invoke when cancelling the edit dialog. Can be called
     *     with null (`toEdit` was null), incomplete information (`toEdit` was incomplete), invalid
     *     information (`toEdit` was invalid), or even with complete and valid information (`toEdit`
     *     was both complete and valid to begin with).
     */
    protected abstract void showEditPrompt(
            @Nullable T toEdit, Callback<T> doneCallback, Callback<@Nullable T> cancelCallback);

    public @Nullable PropertyModel getEditorModelForTesting() {
        return mEditorModel;
    }

    protected void reset() {
        if (mEditorMCP != null) {
            mEditorMCP.destroy();
            mEditorMCP = null;
        }
        mEditorModel = null;
    }
}
