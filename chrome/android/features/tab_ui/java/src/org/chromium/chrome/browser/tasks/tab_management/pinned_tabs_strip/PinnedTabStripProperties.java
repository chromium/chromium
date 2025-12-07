// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the pinned tabs strip. */
@NullMarked
class PinnedTabStripProperties {
    public static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey SCROLL_TO_POSITION = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<ObservableSupplierImpl<Boolean>>
            IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<PinnedTabStripAnimationManager>
            ANIMATION_MANAGER = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_VISIBLE,
                SCROLL_TO_POSITION,
                BACKGROUND_COLOR,
                IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER,
                ANIMATION_MANAGER
            };
}
