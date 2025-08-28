// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

/**
 * Encapsulates the styling properties for a keyboard accessory view.
 *
 * <p>This class defines the appearance and layout of the accessory, distinguishing between two
 * primary states:
 *
 * <p>Docked: Attached to the bottom, spanning the screen width.
 *
 * <p>Undocked (Floating): Appears as a floating element with a specified maximum width and a
 * vertical offset from the top of its container.
 */
@NullMarked
public class KeyboardAccessoryStyle {
    private final boolean mIsDocked;
    private final int mOffset;
    private final int mMaxWidth;

    public KeyboardAccessoryStyle(boolean isDocked, @Px int offset, @Px int maxWidth) {
        this.mIsDocked = isDocked;
        this.mOffset = offset;
        this.mMaxWidth = maxWidth;
    }

    /**
     * @return {@code true} if the accessory is docked to the bottom, {@code false} otherwise.
     */
    public boolean isDocked() {
        return mIsDocked;
    }

    /**
     * @return the vertical offset in pixels. For a docked style, this is the margin from the
     *     bottom. For an undocked style, this is the margin from the top.
     */
    public int getOffset() {
        return mOffset;
    }

    /**
     * @return The maximum width in pixels. This is primarily relevant for undocked styles. Docked
     *     styles typically match parent width. 0 means that the width has no max value.
     */
    public int getMaxWidth() {
        return mMaxWidth;
    }
}
