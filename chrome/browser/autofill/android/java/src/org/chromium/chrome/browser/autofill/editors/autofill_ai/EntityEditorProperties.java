// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties defined here reflect the visible state of the {@link EntityEditorView}. */
@NullMarked
public class EntityEditorProperties {
    public static final ReadableObjectPropertyKey<String> EDITOR_TITLE =
            new ReadableObjectPropertyKey<>("editor_title");

    public static final WritableBooleanPropertyKey VISIBLE =
            new WritableBooleanPropertyKey("visible");

    public static final PropertyKey[] ALL_KEYS = {
        EDITOR_TITLE, VISIBLE,
    };

    private EntityEditorProperties() {}
}
