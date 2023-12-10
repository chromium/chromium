// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop.toolbar;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for TargetView. */
class TargetViewProperties {
    // Visible related properties
    public static final WritableIntPropertyKey TARGET_VIEW_VISIBLE = new WritableIntPropertyKey();

    // View related properties
    public static final WritableBooleanPropertyKey TARGET_VIEW_ACTIVE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<TargetViewDragListener> ON_DRAG_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {TARGET_VIEW_VISIBLE, TARGET_VIEW_ACTIVE, ON_DRAG_LISTENER};
}
