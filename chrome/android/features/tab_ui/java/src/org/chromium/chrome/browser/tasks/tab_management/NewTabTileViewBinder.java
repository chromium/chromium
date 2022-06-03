// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for NewTabTile component.
 */
class NewTabTileViewBinder {
    public static void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        assert view instanceof NewTabTileView;
        NewTabTileView newTabTileView = (NewTabTileView) view;

        if (NewTabTileViewProperties.ON_CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(NewTabTileViewProperties.ON_CLICK_LISTENER));
        } else if (NewTabTileViewProperties.THUMBNAIL_ASPECT_RATIO == propertyKey) {
            float ratio = model.get(NewTabTileViewProperties.THUMBNAIL_ASPECT_RATIO);
            newTabTileView.setAspectRatio(ratio);
        } else if (NewTabTileViewProperties.CARD_HEIGHT_INTERCEPT == propertyKey) {
            int intercept = model.get(NewTabTileViewProperties.CARD_HEIGHT_INTERCEPT);
            newTabTileView.setHeightIntercept(intercept);
        } else if (NewTabTileViewProperties.IS_INCOGNITO == propertyKey) {
            newTabTileView.updateColor(model.get(NewTabTileViewProperties.IS_INCOGNITO));
        }
    }
}
