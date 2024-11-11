// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.media;

import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for {@link MediaCapturePickerDialog}. */
public class MediaCapturePickerItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (MediaCapturePickerItemProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(MediaCapturePickerItemProperties.CLICK_LISTENER));
        } else if (MediaCapturePickerItemProperties.TAB_NAME == propertyKey) {
            TextView titleView = (TextView) view.findViewById(R.id.tab_title);
            String text = model.get(MediaCapturePickerItemProperties.TAB_NAME);
            titleView.setText(text);
        } else if (MediaCapturePickerItemProperties.SELECTED == propertyKey) {
            if (model.get(MediaCapturePickerItemProperties.SELECTED)) {
                view.setBackgroundColor(
                        ChromeColors.getSurfaceColor(
                                view.getContext(), R.dimen.default_elevation_4));
            } else {
                view.setBackgroundResource(android.R.color.transparent);
            }
        }
    }
}
