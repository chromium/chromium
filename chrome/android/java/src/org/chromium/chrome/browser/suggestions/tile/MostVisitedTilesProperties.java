// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** View Properties related to displaying a most visited list. */
public final class MostVisitedTilesProperties {
    private MostVisitedTilesProperties() {}

    public static final PropertyModel.WritableBooleanPropertyKey IS_CONTAINER_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_MVT_LAYOUT_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<View> PLACEHOLDER_VIEW =
            new WritableObjectPropertyKey<>();

    // We need to skip the equality check here since there are some cases when the view removes all
    // child views and then adds them back with the same paddings, which could not be set without
    // skipping the check.
    public static final PropertyModel.WritableObjectPropertyKey<Integer>
            HORIZONTAL_INTERVAL_PADDINGS = new WritableObjectPropertyKey<>(true);
    public static final PropertyModel.WritableObjectPropertyKey<Integer> HORIZONTAL_EDGE_PADDINGS =
            new WritableObjectPropertyKey<>(true);

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_CONTAINER_VISIBLE,
                IS_MVT_LAYOUT_VISIBLE,
                PLACEHOLDER_VIEW,
                HORIZONTAL_INTERVAL_PADDINGS,
                HORIZONTAL_EDGE_PADDINGS,
            };
}
