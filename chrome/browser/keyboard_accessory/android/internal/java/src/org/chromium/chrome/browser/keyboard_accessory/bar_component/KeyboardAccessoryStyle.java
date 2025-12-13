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
    private final int mHorizontalOffset;
    private final int mVerticalOffset;
    private final int mMaxWidth;

    public KeyboardAccessoryStyle(
            boolean isDocked, @Px int horizontalOffset, @Px int verticalOffset, @Px int maxWidth) {
        this.mIsDocked = isDocked;
        this.mHorizontalOffset = horizontalOffset;
        this.mVerticalOffset = verticalOffset;
        this.mMaxWidth = maxWidth;
    }

    /**
     * @return {@code true} if the accessory is docked to the bottom, {@code false} otherwise.
     */
    public boolean isDocked() {
        return mIsDocked;
    }

    /**
     * @return The horizontal offset in pixels, used only as a left margin for dynamically
     *     positioned, undocked-style keyboard accessory bars.
     */
    public @Px int getHorizontalOffset() {
        return mHorizontalOffset;
    }

    /**
     * @return the vertical offset in pixels. For a docked style, this is the margin from the
     *     bottom. For an undocked style, this is the margin from the top.
     */
    public @Px int getVerticalOffset() {
        return mVerticalOffset;
    }

    /**
     * @return The maximum width in pixels. This is primarily relevant for undocked styles. Docked
     *     styles typically match parent width. 0 means that the width has no max value.
     */
    public @Px int getMaxWidth() {
        return mMaxWidth;
    }
}
