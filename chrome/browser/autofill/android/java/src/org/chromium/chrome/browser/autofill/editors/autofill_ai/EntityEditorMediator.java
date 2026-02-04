// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.EDITOR_TITLE;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Entity Editor. */
@NullMarked
class EntityEditorMediator {
    private @Nullable PropertyModel mEditorModel;

    EntityEditorMediator() {}

    @EnsuresNonNull("mEditorModel")
    PropertyModel getEditorModel(EntityType entityType) {
        if (mEditorModel == null) {
            mEditorModel =
                    new PropertyModel.Builder(EntityEditorProperties.ALL_KEYS)
                            .with(EDITOR_TITLE, entityType.addEntityTypeString)
                            .build();
        }
        return mEditorModel;
    }
}
