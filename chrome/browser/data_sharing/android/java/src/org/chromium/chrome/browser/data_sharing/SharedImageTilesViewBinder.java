// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.chromium.chrome.browser.data_sharing.SharedImageTilesProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.browser.data_sharing.SharedImageTilesProperties.IS_LOADING;
import static org.chromium.chrome.browser.data_sharing.SharedImageTilesProperties.REMAINING_TILES;

import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for SharedImageTiles component. */
class SharedImageTilesViewBinder {

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (IS_LOADING == propertyKey) {
            // TODO(b/324909919): Set loading state for shared_image_tiles view.
        } else if (REMAINING_TILES == propertyKey) {
            TextView text = (TextView) view.findViewById(R.id.tiles_count);
            Resources res = view.getContext().getResources();
            String countText =
                    res.getString(
                            R.string.shared_image_tiles_count,
                            Integer.toString(model.get(REMAINING_TILES)));
            text.setText(countText);
        } else if (BACKGROUND_COLOR == propertyKey) {
            LinearLayout container = (LinearLayout) view.findViewById(R.id.tiles_count_container);
            if (container != null) {
                Drawable drawable = container.getBackground();
                drawable.setColorFilter(model.get(BACKGROUND_COLOR), PorterDuff.Mode.SRC_IN);
            }
        }
    }
}
