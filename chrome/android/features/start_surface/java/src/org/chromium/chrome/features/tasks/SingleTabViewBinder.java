// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.chromium.chrome.features.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.START_MARGIN;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TITLE;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

// The view binder of the single tab view.
class SingleTabViewBinder {
    public static void bind(PropertyModel model, SingleTabView view, PropertyKey propertyKey) {
        if (propertyKey == CLICK_LISTENER) {
            view.setOnClickListener(model.get(CLICK_LISTENER));
        } else if (propertyKey == FAVICON) {
            view.setFavicon(model.get(FAVICON));
        } else if (propertyKey == IS_VISIBLE) {
            view.setVisibility(model.get(IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == TITLE) {
            view.setTitle(model.get(TITLE));
        } else if (propertyKey == START_MARGIN) {
            MarginLayoutParams marginLayoutParams = (MarginLayoutParams) view.getLayoutParams();
            int margin = model.get(START_MARGIN);
            marginLayoutParams.setMarginStart(margin);
            marginLayoutParams.setMarginEnd(margin);
            view.setLayoutParams(marginLayoutParams);
        } else {
            assert false : "Unsupported property key";
        }
    }
}