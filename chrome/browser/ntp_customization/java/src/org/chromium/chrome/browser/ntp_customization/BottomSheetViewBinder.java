// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_MVT_SWITCH_CHECKED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class responsible for binding a back press handler to the back button or removing the back button
 * in a bottom sheet.
 */
@NullMarked
public class BottomSheetViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        MaterialSwitchWithText mvtSwitch = view.findViewById(R.id.mvt_switch_button);
        if (propertyKey == BACK_PRESS_HANDLER) {
            View backButton = view.findViewById(R.id.back_button);
            backButton.setOnClickListener(model.get(BACK_PRESS_HANDLER));

            // If the the handler is null, removes the back button from the bottom sheet.
            if (model.get(BACK_PRESS_HANDLER) == null) {
                backButton.setVisibility(View.GONE);

                View titleView = view.findViewById(R.id.bottom_sheet_title);
                if (titleView != null) {
                    ViewGroup.MarginLayoutParams params =
                            (ViewGroup.MarginLayoutParams) titleView.getLayoutParams();
                    params.setMarginStart(
                            view.getResources()
                                    .getDimensionPixelSize(
                                            R.dimen
                                                    .ntp_customization_bottom_sheet_layout_padding_bottom));
                    titleView.setLayoutParams(params);
                }
            }
        } else if (propertyKey == IS_MVT_SWITCH_CHECKED) {
            mvtSwitch.setChecked(model.get(IS_MVT_SWITCH_CHECKED));
        } else if (propertyKey == MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER) {
            mvtSwitch.setOnCheckedChangeListener(model.get(MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER));
        }
    }
}
