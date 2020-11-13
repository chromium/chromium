// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter.chips;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.ui.widget.ChipView;

/**
 * A generic visual representation of a Chip. Most of the visuals are immutable, but the selection
 * and enable states are not.
 */
public class Chip {
    /** An id to use for {@link #icon} when there is no icon on the chip. */
    public static final int INVALID_ICON_ID = ChipView.INVALID_ICON_ID;

    /** An id used to identify this chip. */
    public final int id;

    /** The resource id for the text to show in the chip. */
    public final @StringRes int text;

    /** The accessibility text to use for the chip. */
    public String contentDescription;

    /** The resource id for the icon to use in the chip. */
    public final @DrawableRes int icon;

    /** The {@link Runnable} to trigger when this chip is selected by the UI. */
    public final Runnable chipSelectedListener;

    /** Whether or not this Chip is enabled. */
    public boolean enabled;

    /** Whether or not this Chip is selected. */
    public boolean selected;

    /** Builds a new {@link Chip} instance.  These properties cannot be changed. */
    public Chip(int id, @StringRes int text, @DrawableRes int icon, Runnable chipSelectedListener) {
        this.id = id;
        this.text = text;
        this.icon = icon;
        this.chipSelectedListener = chipSelectedListener;
    }
}