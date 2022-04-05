// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.IS_CONTAINER_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.IS_MVT_LAYOUT_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.PLACEHOLDER_VIEW;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Model-to-View binder for most visited list. Handles view manipulations. */
final class MostVisitedListViewBinder {
    /**
     * The view holder holds the most visited container layout and most visited tiles layout.
     */
    public static class ViewHolder {
        public final View mvContainerLayout;
        public final MvTilesLayout mvTilesLayout;

        ViewHolder(View mvContainerLayout, MvTilesLayout mvTilesLayout) {
            this.mvContainerLayout = mvContainerLayout;
            this.mvTilesLayout = mvTilesLayout;
        }
    }

    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (IS_CONTAINER_VISIBLE == propertyKey) {
            viewHolder.mvContainerLayout.setVisibility(
                    model.get(IS_CONTAINER_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (IS_MVT_LAYOUT_VISIBLE == propertyKey) {
            viewHolder.mvTilesLayout.setVisibility(
                    model.get(IS_MVT_LAYOUT_VISIBLE) ? View.VISIBLE : View.GONE);
            if (model.get(PLACEHOLDER_VIEW) == null) return;
            model.get(PLACEHOLDER_VIEW)
                    .setVisibility(model.get(IS_MVT_LAYOUT_VISIBLE) ? View.GONE : View.VISIBLE);
        } else if (INTERVAL_PADDINGS == propertyKey) {
            viewHolder.mvTilesLayout.setIntervalPaddings(model.get(INTERVAL_PADDINGS));
        } else if (EDGE_PADDINGS == propertyKey) {
            viewHolder.mvTilesLayout.setEdgePaddings(model.get(EDGE_PADDINGS));
        }
    }
}
