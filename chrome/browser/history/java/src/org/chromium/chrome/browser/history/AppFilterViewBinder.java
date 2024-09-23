// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder object for the appfilter sheet content model and the view. */
class AppFilterViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey key) {
        if (AppFilterProperties.ICON == key) {
            ImageView icon = view.findViewById(R.id.start_icon);
            icon.setImageDrawable(model.get(AppFilterProperties.ICON));
            icon.setScaleType(ImageView.ScaleType.FIT_CENTER);
        } else if (AppFilterProperties.LABEL == key) {
            ((TextView) view.findViewById(R.id.title))
                    .setText(model.get(AppFilterProperties.LABEL));
            view.findViewById(R.id.description).setVisibility(View.GONE);
        } else if (AppFilterProperties.SELECTED == key) {
            ImageView checkMark = view.findViewById(R.id.end_button);
            checkMark.setImageResource(R.drawable.ic_check_googblue_24dp);
            boolean selected = model.get(AppFilterProperties.SELECTED);
            checkMark.setVisibility(selected ? View.VISIBLE : View.INVISIBLE);
        } else if (AppFilterProperties.CLICK_LISTENER == key) {
            view.setOnClickListener(model.get(AppFilterProperties.CLICK_LISTENER));
        } else if (AppFilterProperties.CLOSE_BUTTON_CALLBACK == key) {
            view.setOnClickListener(model.get(AppFilterProperties.CLOSE_BUTTON_CALLBACK));
        }
    }
}
