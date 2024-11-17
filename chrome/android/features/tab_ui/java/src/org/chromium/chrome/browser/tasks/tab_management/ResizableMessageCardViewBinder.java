// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for TabGridSecondaryItem. */
class ResizableMessageCardViewBinder {
    public static void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        if (ResizableMessageCardViewProperties.WIDTH == propertyKey) {
            View resizableView = view.findViewById(R.id.resizable_view);
            var params = resizableView.getLayoutParams();
            params.width = model.get(ResizableMessageCardViewProperties.WIDTH);
            resizableView.setLayoutParams(params);
        } else {
            MessageCardViewBinder.bind(
                    model, view.findViewById(R.id.tab_grid_message_item), propertyKey);
        }
    }
}
