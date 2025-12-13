// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.core.util.Consumer;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties for displaying a single tab group row. */
@NullMarked
public class TabGroupListProperties {
    public static final ReadableObjectPropertyKey<Consumer<Boolean>> ON_IS_SCROLLED_CHANGED =
            new ReadableObjectPropertyKey<>();
    public static final ReadableBooleanPropertyKey ENABLE_CONTAINMENT =
            new ReadableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey EMPTY_STATE_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey SYNC_ENABLED = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_TABLET_OR_LANDSCAPE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        ON_IS_SCROLLED_CHANGED,
        ENABLE_CONTAINMENT,
        EMPTY_STATE_VISIBLE,
        SYNC_ENABLED,
        IS_TABLET_OR_LANDSCAPE
    };
}
