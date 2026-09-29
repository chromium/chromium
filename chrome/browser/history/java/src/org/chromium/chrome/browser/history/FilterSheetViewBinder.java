// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.history.FilterSheetCoordinator.FilterItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder object for the filter sheet content model and the view. */
@NullMarked
class FilterSheetViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey key) {
        if (FilterSheetProperties.ITEM == key) {
            FilterItem item = model.get(FilterSheetProperties.ITEM);
            ImageView icon = view.findViewById(R.id.start_icon);
            icon.setImageDrawable(item.icon);
            icon.setVisibility(item.icon != null ? View.VISIBLE : View.GONE);
            icon.setScaleType(ImageView.ScaleType.FIT_CENTER);
            ((TextView) view.findViewById(R.id.title)).setText(item.label);
            view.findViewById(R.id.description).setVisibility(View.GONE);
        } else if (FilterSheetProperties.SELECTED == key) {
            ImageView checkMark = view.findViewById(R.id.end_button);
            checkMark.setImageResource(R.drawable.ic_check_googblue_24dp);
            boolean selected = model.get(FilterSheetProperties.SELECTED);
            checkMark.setVisibility(selected ? View.VISIBLE : View.INVISIBLE);
        } else if (FilterSheetProperties.CLICK_LISTENER == key) {
            view.setOnClickListener(model.get(FilterSheetProperties.CLICK_LISTENER));
        } else if (FilterSheetProperties.CLOSE_BUTTON_CALLBACK == key) {
            view.setOnClickListener(model.get(FilterSheetProperties.CLOSE_BUTTON_CALLBACK));
        }
    }
}
