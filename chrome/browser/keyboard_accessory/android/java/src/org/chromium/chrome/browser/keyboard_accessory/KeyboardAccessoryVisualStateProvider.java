// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;

/**
 * A class that can be observed to be notified of changes to the visual state of the keyboard
 * accessory view.
 */
@NullMarked
public interface KeyboardAccessoryVisualStateProvider {
    /** An observer watching for changes to the visual state of the keyboard accessory. */
    interface Observer {
        /**
         * Called when the visual state of the keyboard accessory changes.
         *
         * @param visible True if the keyboard accessory is showing, false otherwise.
         * @param color The background color of the keyboard accessory.
         */
        void onKeyboardAccessoryVisualStateChanged(boolean visible, @ColorInt int color);
    }

    /**
     * Add an observer to be notified of visual changes to the keyboard accessory.
     *
     * @param observer The observer to add.
     */
    void addObserver(Observer observer);

    /**
     * Remove a previously added observer.
     *
     * @param observer The observer to remove.
     */
    void removeObserver(Observer observer);
}
