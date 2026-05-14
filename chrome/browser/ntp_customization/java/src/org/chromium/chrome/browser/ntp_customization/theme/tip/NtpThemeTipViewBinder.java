// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.tip;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/** Binds the NTP theme tip bottom sheet properties to the view. */
@NullMarked
public class NtpThemeTipViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == NtpThemeTipProperties.CANCEL_BUTTON_CLICK_LISTENER) {
            ButtonCompat cancelButton = view.findViewById(R.id.cancel_button);
            cancelButton.setOnClickListener(
                    model.get(NtpThemeTipProperties.CANCEL_BUTTON_CLICK_LISTENER));
        } else if (key == NtpThemeTipProperties.CUSTOMIZE_BUTTON_CLICK_LISTENER) {
            ButtonCompat customizeButton = view.findViewById(R.id.customize_button);
            customizeButton.setOnClickListener(
                    model.get(NtpThemeTipProperties.CUSTOMIZE_BUTTON_CLICK_LISTENER));
        }
    }
}
