// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import androidx.annotation.IntDef;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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
    private final @NotchPosition int mNotchPosition;

    @IntDef({
        NotchPosition.TOP,
        NotchPosition.BOTTOM,
        NotchPosition.HIDDEN,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NotchPosition {
        int TOP = 0;
        int BOTTOM = 1;
        int HIDDEN = 2;
    }

    private KeyboardAccessoryStyle(
            boolean isDocked,
            @Px int horizontalOffset,
            @Px int verticalOffset,
            @Px int maxWidth,
            @NotchPosition int notchPosition) {
        this.mIsDocked = isDocked;
        this.mHorizontalOffset = horizontalOffset;
        this.mVerticalOffset = verticalOffset;
        this.mMaxWidth = maxWidth;
        this.mNotchPosition = notchPosition;
    }

    /**
     * Creates a style for a docked keyboard accessory.
     *
     * @param verticalOffset The vertical offset in pixels from the bottom.
     * @return A new {@link KeyboardAccessoryStyle} instance for a docked accessory.
     */
    public static KeyboardAccessoryStyle createDockedKeyboardAccessoryStyle(
            @Px int verticalOffset) {
        return new KeyboardAccessoryStyle(true, 0, verticalOffset, 0, NotchPosition.HIDDEN);
    }

    /**
     * Creates a style for an undocked (floating) keyboard accessory.
     *
     * @param horizontalOffset The horizontal offset in pixels from the left.
     * @param verticalOffset The vertical offset in pixels from the top.
     * @param maxWidth The maximum width in pixels. 0 means no max width.
     * @param notchPosition Position of the notch used for the dynamic positioning. Only TOP and
     *     BOTTOM is allowed.
     * @return A new {@link KeyboardAccessoryStyle} instance for an undocked accessory.
     */
    public static KeyboardAccessoryStyle createUndockedKeyboardAccessoryStyle(
            @Px int horizontalOffset,
            @Px int verticalOffset,
            @Px int maxWidth,
            @NotchPosition int notchPosition) {
        assert notchPosition != NotchPosition.HIDDEN;
        return new KeyboardAccessoryStyle(
                false, horizontalOffset, verticalOffset, maxWidth, notchPosition);
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

    /**
     * @return Returns the position (TOP/BOTTOM/HIDDEN) of the notch pointing to the field.
     */
    public @NotchPosition int getNotchPosition() {
        return mNotchPosition;
    }
}
