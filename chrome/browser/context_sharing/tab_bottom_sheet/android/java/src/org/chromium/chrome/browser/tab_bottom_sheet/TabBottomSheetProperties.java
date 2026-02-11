// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the "Auto Delete Archived Tabs Decision Promo" bottom sheet. */
@NullMarked
public class TabBottomSheetProperties {
    // Fusebox Properties
    public static final WritableObjectPropertyKey<Float> FUSEBOX_OFFSET =
            new WritableObjectPropertyKey<>("fusebox_offset");

    public static final PropertyKey[] ALL_KEYS = {FUSEBOX_OFFSET};

    /**
     * Creates a default model structure. Listeners will be populated by the Coordinator.
     *
     * @return A new {@link PropertyModel} instance.
     */
    public static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS).build();
    }
}
