// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class HistoryClustersViewBinder {
    public static void bindVisitView(PropertyModel model, View view, PropertyKey key) {
        HistoryClustersItemView itemView = (HistoryClustersItemView) view;
        if (key == HistoryClustersItemProperties.ICON_DRAWABLE) {
            itemView.setIconDrawable(model.get(HistoryClustersItemProperties.ICON_DRAWABLE));
        } else if (key == HistoryClustersItemProperties.TITLE) {
            itemView.setTitleText(model.get(HistoryClustersItemProperties.TITLE));
        } else if (key == HistoryClustersItemProperties.URL) {
            itemView.setHostText(model.get(HistoryClustersItemProperties.URL));
        }
    }
}