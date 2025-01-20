// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.COLOR_STYLE;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.ICON_TILES;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.IS_LOADING;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.REMAINING_TILES;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.TYPE;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for SharedImageTiles component. */
class SharedImageTilesViewBinder {

    public static void bind(
            PropertyModel model, SharedImageTilesView view, PropertyKey propertyKey) {
        if (IS_LOADING == propertyKey) {
            // TODO(b/324909919): Set loading state for shared_image_tiles view.
        } else if (COLOR_STYLE == propertyKey) {
            view.setColorStyle(model.get(COLOR_STYLE));
        } else if (TYPE == propertyKey) {
            view.setType(model.get(TYPE));
        } else if (ICON_TILES == propertyKey) {
            view.resetIconTiles(model.get(ICON_TILES));

            // Re-style the component.
            view.setType(model.get(TYPE));
            view.setColorStyle(model.get(COLOR_STYLE));
        } else if (REMAINING_TILES == propertyKey) {
            if (model.get(REMAINING_TILES) > 0) {
                view.showCountTile(model.get(REMAINING_TILES));
            }
        }
    }
}
