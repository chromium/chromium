// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.LEFT_RIGHT_MARGINS;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Model-to-View binder for most visited list. Handles view manipulations. */
final class MostVisitedListViewBinder {
    public static void bind(PropertyModel model, MvTilesLayout view, PropertyKey propertyKey) {
        if (IS_VISIBLE == propertyKey) {
            view.setVisibility(model.get(IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (INTERVAL_PADDINGS == propertyKey) {
            view.setIntervalPaddings(model.get(INTERVAL_PADDINGS));
        } else if (EDGE_PADDINGS == propertyKey) {
            view.setEdgePaddings(model.get(EDGE_PADDINGS));
        } else if (LEFT_RIGHT_MARGINS == propertyKey) {
            view.setLeftAndRightMargins(model.get(LEFT_RIGHT_MARGINS));
        }
    }
}
