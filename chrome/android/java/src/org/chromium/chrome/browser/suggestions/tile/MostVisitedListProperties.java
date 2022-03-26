// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** View Properties related to displaying a most visited list. */
public final class MostVisitedListProperties {
    private MostVisitedListProperties() {}

    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    // We need to skip the equality check here since there are some cases when the view removes all
    // child views and then adds them back with the same paddings, which could not be set without
    // skipping the check.
    public static final PropertyModel.WritableObjectPropertyKey<Integer> INTERVAL_PADDINGS =
            new WritableObjectPropertyKey<>(true);
    public static final PropertyModel.WritableObjectPropertyKey<Integer> EDGE_PADDINGS =
            new WritableObjectPropertyKey<>(true);

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {IS_VISIBLE, INTERVAL_PADDINGS, EDGE_PADDINGS};
}
