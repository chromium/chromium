// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.desktop_popup_header;

import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.graphics.Insets;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class DesktopPopupHeaderViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        TextView titleView = view.findViewById(R.id.desktop_popup_header_text_view);

        if (key == DesktopPopupHeaderProperties.IS_SHOWN) {
            view.setVisibility(
                    model.get(DesktopPopupHeaderProperties.IS_SHOWN) ? View.VISIBLE : View.GONE);
        } else if (key == DesktopPopupHeaderProperties.TITLE_TEXT) {
            titleView.setText(model.get(DesktopPopupHeaderProperties.TITLE_TEXT));
        } else if (key == DesktopPopupHeaderProperties.TITLE_VISIBLE) {
            titleView.setVisibility(
                    model.get(DesktopPopupHeaderProperties.TITLE_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (key == DesktopPopupHeaderProperties.TITLE_APPEARANCE) {
            titleView.setTextAppearance(model.get(DesktopPopupHeaderProperties.TITLE_APPEARANCE));
        } else if (key == DesktopPopupHeaderProperties.TITLE_SPACING) {
            Insets margins = model.get(DesktopPopupHeaderProperties.TITLE_SPACING);
            LinearLayout.LayoutParams params =
                    (LinearLayout.LayoutParams) titleView.getLayoutParams();
            params.setMargins(margins.left, margins.top, margins.right, margins.bottom);
            titleView.setLayoutParams(params);
        } else if (key == DesktopPopupHeaderProperties.BACKGROUND_COLOR) {
            view.setBackgroundColor(model.get(DesktopPopupHeaderProperties.BACKGROUND_COLOR));
        } else if (key == DesktopPopupHeaderProperties.HEADER_HEIGHT_PX) {
            ViewGroup.LayoutParams lp = view.getLayoutParams();
            lp.height = model.get(DesktopPopupHeaderProperties.HEADER_HEIGHT_PX);
            view.setLayoutParams(lp);
        }
    }
}
