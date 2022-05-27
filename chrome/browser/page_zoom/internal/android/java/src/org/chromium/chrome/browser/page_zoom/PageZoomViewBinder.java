// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_zoom;

import android.view.View;
import android.widget.SeekBar;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for the page zoom feature.
 */
class PageZoomViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (PageZoomProperties.MAXIMUM_ZOOM == propertyKey) {
            ((SeekBar) view.findViewById(R.id.page_zoom_slider))
                    .setMax(model.get(PageZoomProperties.MAXIMUM_ZOOM));
        } else if (PageZoomProperties.CURRENT_ZOOM == propertyKey) {
            ((SeekBar) view.findViewById(R.id.page_zoom_slider))
                    .setProgress(model.get(PageZoomProperties.CURRENT_ZOOM));
        } else if (PageZoomProperties.DECREASE_ZOOM_CALLBACK == propertyKey) {
            view.findViewById(R.id.page_zoom_decrease_zoom_button)
                    .setOnClickListener(v
                            -> model.get(PageZoomProperties.DECREASE_ZOOM_CALLBACK).onResult(null));
        } else if (PageZoomProperties.INCREASE_ZOOM_CALLBACK == propertyKey) {
            view.findViewById(R.id.page_zoom_increase_zoom_button)
                    .setOnClickListener(v
                            -> model.get(PageZoomProperties.INCREASE_ZOOM_CALLBACK).onResult(null));
        }
    }
}