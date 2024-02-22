// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for SharedImageTiles component. */
class SharedImageTilesViewBinder {

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (SharedImageTilesProperties.IS_LOADING == propertyKey) {
            // TODO(b/324909919): Set loading state for shared_image_tiles view.
        }
    }
}
