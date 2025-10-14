// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.content.Context;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * Groups 2 or 3 {@link ChipView}.
 *
 * <p>Keyboard Accessory chips are displayed in a {@link RecyclerView}. By default, the Keyboard
 * Accessory chips aren't limited by width. This class is used to limit the width of the first chip
 * or the first 2 chips so that the next one is partially displayed on the screen. This is done to
 * hint the user that the Keyboard Accessory UI is scrollable.
 */
@NullMarked
class KeyboardAccessoryChipGroup extends LinearLayout {

    public KeyboardAccessoryChipGroup(Context context) {
        super(context);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        // TODO: crbug.com/450830784 - Conditionally limit the width of the first 2 chips.
    }
}
