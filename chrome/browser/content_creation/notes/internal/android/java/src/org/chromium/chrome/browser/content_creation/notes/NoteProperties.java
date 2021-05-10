// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import org.chromium.components.content_creation.notes.models.NoteTemplate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class NoteProperties {
    /** The view type used by the recycler view to show the notes. */
    public static final int NOTE_VIEW_TYPE = 1;

    /** The template definition.*/
    static final WritableObjectPropertyKey<NoteTemplate> TEMPLATE =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {TEMPLATE};
}