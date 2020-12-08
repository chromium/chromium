// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

/**
 * An object that contains the required fields to generate the context menu chip.
 */
public class ChipRenderParams {
    // The resource id for the chip title.
    public @StringRes int titleResourceId;

    // The resource id for the chip icon.
    public @DrawableRes int iconResourceId;

    // The callback to be called when the chip clicked.
    // A non-null ChipRenderParams will always have a non-null onClickCallback.
    public Runnable onClickCallback;

    // A callback to be called when the chip shown.
    public @Nullable Runnable onShowCallback;
}