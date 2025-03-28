// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Class responsible for binding a back press handler to the back button in a bottom sheet. */
public class BottomSheetViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == BACK_PRESS_HANDLER) {
            View backButton = view.findViewById(R.id.back_button);
            backButton.setOnClickListener(model.get(BACK_PRESS_HANDLER));
        }
    }
}
