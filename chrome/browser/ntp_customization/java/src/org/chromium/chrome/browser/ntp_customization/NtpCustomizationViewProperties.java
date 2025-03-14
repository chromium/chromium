// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class NtpCustomizationViewProperties {
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            NTP_CARDS_OPTION_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            NTP_CARDS_BACK_PRESS_HANDLER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey LAYOUT_TO_DISPLAY =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                NTP_CARDS_OPTION_CLICK_LISTENER, NTP_CARDS_BACK_PRESS_HANDLER, LAYOUT_TO_DISPLAY,
            };
}
