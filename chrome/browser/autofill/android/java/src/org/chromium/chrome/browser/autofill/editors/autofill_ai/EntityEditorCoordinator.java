// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the Entity Editor. */
@NullMarked
public class EntityEditorCoordinator {
    private final EntityEditorMediator mMediator;
    private final EntityEditorView mEditorView;
    private @Nullable PropertyModel mEditorModel;

    public EntityEditorCoordinator(Activity activity) {
        mMediator = new EntityEditorMediator();
        mEditorView = new EntityEditorView(activity);
    }

    public void showEditorDialog() {
        mEditorModel = mMediator.getEditorModel();
        PropertyModelChangeProcessor.create(
                mEditorModel, mEditorView, EntityEditorViewBinder::bindEditorDialogView);
    }

    EntityEditorView getEntityEditorViewForTest() {
        return mEditorView;
    }
}
