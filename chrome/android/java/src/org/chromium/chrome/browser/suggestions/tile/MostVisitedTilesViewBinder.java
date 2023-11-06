// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_CONTAINER_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_MVT_LAYOUT_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_NTP_AS_HOME_SURFACE_ON_TABLET;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_SURFACE_POLISH_ENABLED;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.PLACEHOLDER_VIEW;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.UPDATE_INTERVAL_PADDINGS_TABLET;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Model-to-View binder for most visited list. Handles view manipulations. */
final class MostVisitedTilesViewBinder {
    /**
     * The view holder holds the most visited container layout and most visited tiles layout.
     */
    public static class ViewHolder {
        public final View mvContainerLayout;
        public final ViewGroup mvTilesLayout;

        ViewHolder(View mvContainerLayout, ViewGroup mvTilesLayout) {
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
        } else if (HORIZONTAL_INTERVAL_PADDINGS == propertyKey) {
            assert viewHolder.mvTilesLayout instanceof MostVisitedTilesCarouselLayout;
            ((MostVisitedTilesCarouselLayout) viewHolder.mvTilesLayout)
                    .setIntervalPaddings(model.get(HORIZONTAL_INTERVAL_PADDINGS));
        } else if (HORIZONTAL_EDGE_PADDINGS == propertyKey) {
            assert viewHolder.mvTilesLayout instanceof MostVisitedTilesCarouselLayout;
            ((MostVisitedTilesCarouselLayout) viewHolder.mvTilesLayout)
                    .setEdgePaddings(model.get(HORIZONTAL_EDGE_PADDINGS));
        } else if (IS_NTP_AS_HOME_SURFACE_ON_TABLET == propertyKey) {
            ((MostVisitedTilesLayout) viewHolder.mvTilesLayout)
                    .setIsNtpAsHomeSurfaceOnTablet(model.get(IS_NTP_AS_HOME_SURFACE_ON_TABLET));
        } else if (IS_SURFACE_POLISH_ENABLED == propertyKey) {
            assert viewHolder.mvTilesLayout instanceof MostVisitedTilesCarouselLayout;
            ((MostVisitedTilesCarouselLayout) viewHolder.mvTilesLayout)
                    .setIsSurfacePolishEnabled(model.get(IS_SURFACE_POLISH_ENABLED));
        } else if (UPDATE_INTERVAL_PADDINGS_TABLET == propertyKey) {
            assert viewHolder.mvTilesLayout instanceof MostVisitedTilesCarouselLayout;
            ((MostVisitedTilesCarouselLayout) viewHolder.mvTilesLayout)
                    .updateIntervalPaddingsTablet(model.get(UPDATE_INTERVAL_PADDINGS_TABLET));
        }
    }
}
