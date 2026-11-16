// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** PropertyModel keys for the critical actions section of a history row. */
@NullMarked
class CriticalActionsProperties {
    /** Whether the row's expand/collapse chevron is visible. */
    static final WritableBooleanPropertyKey EXPAND_ICON_VISIBLE = new WritableBooleanPropertyKey();

    /** Accessibility content description summarizing the action count on the chevron. */
    static final WritableObjectPropertyKey<@Nullable CharSequence> EXPAND_ICON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** Rotation of the chevron in degrees (0 when collapsed, 180 when expanded). */
    static final WritableFloatPropertyKey EXPAND_ICON_ROTATION = new WritableFloatPropertyKey();

    /** Whether the below-row "Gemini actions" panel container is visible. */
    static final WritableBooleanPropertyKey PANEL_VISIBLE = new WritableBooleanPropertyKey();

    /** Non-selection row click override when tapping the row should toggle critical actions. */
    static final WritableObjectPropertyKey<@Nullable Runnable> ROW_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();

    /** Click accessibility action label on the row ("Expand section" / "Collapse section"). */
    static final WritableObjectPropertyKey<@Nullable CharSequence> ROW_CLICK_ACCESSIBILITY_LABEL =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        EXPAND_ICON_VISIBLE,
        EXPAND_ICON_CONTENT_DESCRIPTION,
        EXPAND_ICON_ROTATION,
        PANEL_VISIBLE,
        ROW_CLICK_HANDLER,
        ROW_CLICK_ACCESSIBILITY_LABEL,
    };

    /** Localized label of an individual critical action row. */
    static final ReadableObjectPropertyKey<String> LABEL = new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ACTION_ITEM_KEYS = {LABEL};
}
