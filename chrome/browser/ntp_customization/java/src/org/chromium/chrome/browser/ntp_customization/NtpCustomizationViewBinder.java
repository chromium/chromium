// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LAYOUT_TO_DISPLAY;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.NTP_CARDS_BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.NTP_CARDS_OPTION_CLICK_LISTENER;

import android.view.View;
import android.widget.ViewFlipper;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class NtpCustomizationViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == NTP_CARDS_OPTION_CLICK_LISTENER) {
            View ntpCards = view.findViewById(R.id.new_tab_page_cards_list_item_container);
            ntpCards.setOnClickListener(model.get(NTP_CARDS_OPTION_CLICK_LISTENER));
        } else if (propertyKey == NTP_CARDS_BACK_PRESS_HANDLER) {
            View backButton = view.findViewById(R.id.ntp_cards_back_button);
            backButton.setOnClickListener(model.get(NTP_CARDS_BACK_PRESS_HANDLER));
        } else if (propertyKey == LAYOUT_TO_DISPLAY) {
            ((ViewFlipper) view).setDisplayedChild(model.get(LAYOUT_TO_DISPLAY));
        } else {
            assert false : "Unsupported property key";
        }
    }
}
