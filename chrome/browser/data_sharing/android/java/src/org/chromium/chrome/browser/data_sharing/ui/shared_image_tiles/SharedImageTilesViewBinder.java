// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.COLOR_THEME;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.ICON_TILES;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.IS_LOADING;
import static org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesProperties.REMAINING_TILES;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for SharedImageTiles component. */
class SharedImageTilesViewBinder {

    public static void bind(
            PropertyModel model, SharedImageTilesView view, PropertyKey propertyKey) {
        if (IS_LOADING == propertyKey) {
            // TODO(b/324909919): Set loading state for shared_image_tiles view.
        } else if (COLOR_THEME == propertyKey) {
            view.setColorTheme(model.get(COLOR_THEME));
        } else if (ICON_TILES == propertyKey) {
            view.resetIconTiles(model.get(ICON_TILES));
        } else if (REMAINING_TILES == propertyKey) {
            if (model.get(REMAINING_TILES) > 0) {
                view.showCountTile(model.get(REMAINING_TILES));
            }
        }
    }
}
