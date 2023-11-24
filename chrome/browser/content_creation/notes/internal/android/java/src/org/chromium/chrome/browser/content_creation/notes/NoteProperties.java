// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.graphics.Typeface;

import org.chromium.components.content_creation.notes.models.NoteTemplate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class NoteProperties {
    /** The view type used by the recycler view to show the notes. */
    public static final int NOTE_VIEW_TYPE = 1;

    /** Whether this note is the first one. */
    static final WritableObjectPropertyKey<Boolean> IS_FIRST = new WritableObjectPropertyKey<>();

    /** Whether this note is the last one. */
    static final WritableObjectPropertyKey<Boolean> IS_LAST = new WritableObjectPropertyKey<>();

    /** The template definition.*/
    static final WritableObjectPropertyKey<NoteTemplate> TEMPLATE =
            new WritableObjectPropertyKey<>();

    /** The Typeface instance that has been loaded for the associated template. */
    static final WritableObjectPropertyKey<Typeface> TYPEFACE = new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {IS_FIRST, IS_LAST, TEMPLATE, TYPEFACE};
}
