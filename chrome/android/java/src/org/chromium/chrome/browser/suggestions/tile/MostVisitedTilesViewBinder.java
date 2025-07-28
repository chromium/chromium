// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_VISIBLE;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Model-to-View binder for most visited list. Handles view manipulations. */
@NullMarked
final class MostVisitedTilesViewBinder {
    /** The view holder holds the most visited container layout and most visited tiles layout. */
    public static class ViewHolder {
        public final View mvContainerLayout;
        public final MostVisitedTilesLayout mvTilesLayout;

        ViewHolder(View mvContainerLayout, MostVisitedTilesLayout mvTilesLayout) {
            this.mvContainerLayout = mvContainerLayout;
            this.mvTilesLayout = mvTilesLayout;
        }
    }

    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (IS_VISIBLE == propertyKey) {
            viewHolder.mvContainerLayout.setVisibility(
                    model.get(IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (HORIZONTAL_INTERVAL_PADDINGS == propertyKey) {
            viewHolder.mvTilesLayout.setIntervalMargins(model.get(HORIZONTAL_INTERVAL_PADDINGS));
        } else if (HORIZONTAL_EDGE_PADDINGS == propertyKey) {
            viewHolder.mvTilesLayout.setEdgeMargins(model.get(HORIZONTAL_EDGE_PADDINGS));
        }
    }
}
