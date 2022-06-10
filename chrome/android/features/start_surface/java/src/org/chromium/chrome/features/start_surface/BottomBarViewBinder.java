// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_CLICKLISTENER;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_SELECTED_TAB_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_BOTTOM_BAR_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The view binder of the bottom bar. */
class BottomBarViewBinder {
    public static void bind(PropertyModel model, BottomBarView view, PropertyKey propertyKey) {
        if (BOTTOM_BAR_CLICKLISTENER == propertyKey) {
            view.setOnClickListener(model.get(BOTTOM_BAR_CLICKLISTENER));
        } else if (BOTTOM_BAR_SELECTED_TAB_POSITION == propertyKey) {
            view.selectTabAt(model.get(BOTTOM_BAR_SELECTED_TAB_POSITION));
        } else if (IS_BOTTOM_BAR_VISIBLE == propertyKey) {
            view.setVisibility(model.get(IS_SHOWING_OVERVIEW) && model.get(IS_BOTTOM_BAR_VISIBLE));
        } else if (IS_SHOWING_OVERVIEW == propertyKey) {
            view.setVisibility(model.get(IS_SHOWING_OVERVIEW) && model.get(IS_BOTTOM_BAR_VISIBLE));
        }
    }
}
