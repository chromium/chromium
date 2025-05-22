// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class CommerceBottomSheetContentBinder {
    public static void bind(PropertyModel model, LinearLayout view, PropertyKey propertyKey) {
        if (propertyKey == CommerceBottomSheetContentProperties.CUSTOM_VIEW) {
            // Before attaching the custom view, ensure it's detached from any previous parent to
            // avoid potential conflicts.
            View customView = model.get(CommerceBottomSheetContentProperties.CUSTOM_VIEW);
            ViewGroup parent = (ViewGroup) customView.getParent();
            if (parent != null) {
                parent.removeView(customView);
            }

            FrameLayout container = view.findViewById(R.id.content_view_container);
            container.addView(
                    customView,
                    new LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT));
        } else if (propertyKey == CommerceBottomSheetContentProperties.TITLE) {
            TextView titleView = view.findViewById(R.id.title);
            titleView.setText(model.get(CommerceBottomSheetContentProperties.TITLE));
        } else if (propertyKey == CommerceBottomSheetContentProperties.HAS_TITLE) {
            TextView titleView = view.findViewById(R.id.title);
            titleView.setVisibility(
                    model.get(CommerceBottomSheetContentProperties.HAS_TITLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == CommerceBottomSheetContentProperties.HAS_CUSTOM_PADDING) {
            LinearLayout itemContainer = view.findViewById(R.id.item_container);
            if (model.get(CommerceBottomSheetContentProperties.HAS_CUSTOM_PADDING)) {
                itemContainer.setPadding(0, 0, 0, 0);
            } else {
                int padding =
                        itemContainer
                                .getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.content_item_container_padding);
                itemContainer.setPadding(padding, padding, padding, padding);
            }
        }
    }
}
