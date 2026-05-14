// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.tip;

import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the NTP theme tip bottom sheet. */
@NullMarked
public class NtpThemeTipProperties {
    public static final WritableObjectPropertyKey<OnClickListener> CANCEL_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<OnClickListener> CUSTOMIZE_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {CANCEL_BUTTON_CLICK_LISTENER, CUSTOMIZE_BUTTON_CLICK_LISTENER};
}
