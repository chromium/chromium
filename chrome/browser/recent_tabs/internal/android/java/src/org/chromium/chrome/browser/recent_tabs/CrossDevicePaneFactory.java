// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;

import androidx.annotation.NonNull;

import java.util.function.DoubleConsumer;

/** A factory interface for building a {@link CrossDevicePane} instance. */
public class CrossDevicePaneFactory {
    /**
     * Create an instance of the {@link CrossDevicePane}.
     *
     * @param context Used to inflate UI.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     */
    public static CrossDevicePane create(
            @NonNull Context context, @NonNull DoubleConsumer onToolbarAlphaChange) {
        return new CrossDevicePaneImpl(context, onToolbarAlphaChange);
    }
}
