// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.ICON_TILES;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.IS_LOADING;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.REMAINING_TILES;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.SHOW_MANAGE_TILE;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.VIEW_CONFIG;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for SharedImageTiles component. */
@NullMarked
class SharedImageTilesViewBinder {

    public static void bind(
            PropertyModel model, SharedImageTilesView view, PropertyKey propertyKey) {
        if (IS_LOADING == propertyKey) {
            // TODO(b/324909919): Set loading state for shared_image_tiles view.
        } else if (VIEW_CONFIG == propertyKey) {
            view.applyConfig(model.get(VIEW_CONFIG));
        } else if (ICON_TILES == propertyKey) {
            view.resetIconTiles(model.get(ICON_TILES));

            // Re-style the component.
            view.applyConfig(model.get(VIEW_CONFIG));
        } else if (REMAINING_TILES == propertyKey) {
            if (model.get(REMAINING_TILES) > 0) {
                view.showCountTile(model.get(REMAINING_TILES));
            }
        } else if (SHOW_MANAGE_TILE == propertyKey) {
            view.showManageTile(model.get(SHOW_MANAGE_TILE));
        }
    }
}
