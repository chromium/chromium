// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.button;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Combination of display information and the event handling for pressing the button. */
@NullMarked
public interface FullButtonData extends DisplayButtonData {
    /**
     * Returns the {@link Callback<View>} that should be invoked when the button is pressed. The
     * button will be enabled as long as a {@link Callback<View>} is provided (see {@link
     * #canPress()}).
     */
    default @Nullable Callback<View> getOnPress() {
        return null;
    }

    /**
     * Returns the {@link Callback<View>} that should be invoked when the button is long-pressed. No
     * long-press action will be performed unless a {@link Callback<View>} is provided.
     */
    default @Nullable Callback<View> getOnLongPress() {
        return null;
    }

    /** Helper to determine whether the button is clickable. True if there is a Callback. */
    default boolean canPress() {
        return getOnPress() != null;
    }

    /** Dispatcher for press events. */
    default void onPress(View view) {
        Callback<View> onPressCallback = getOnPress();
        if (onPressCallback != null) {
            onPressCallback.onResult(view);
        }
    }

    /** Dispatcher for long-press events. */
    default void onLongPress(View view) {
        Callback<View> onLongPressCallback = getOnLongPress();
        if (onLongPressCallback != null) {
            onLongPressCallback.onResult(view);
        }
    }

    /**
     * Compares this button data with another for equality based on display-relevant properties.
     *
     * <p>This method exists because UI frameworks need to determine if button data has changed to
     * decide whether to update the display, but {@link Callback<View>} instances are not comparable
     * and not relevant to visual appearance.
     *
     * <p>Implementations should make a best effort comparison. ResourceButtonData and
     * DrawableButtonData objects might reference the same visual content but may still report
     * inequality due to different underlying implementations.
     *
     * @param o The object to compare with this button data
     * @return true if the display-relevant properties are equal, false otherwise
     */
    boolean buttonDataEquals(Object o);
}
