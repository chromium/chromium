// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import androidx.annotation.ColorInt;

/**
 * A class that can be observed to be notified of changes to the visual state of the keyboard
 * accessory view.
 */
public interface AccessorySheetVisualStateProvider {
    /** An observer watching for changes to the visual state of the keyboard accessory sheet. */
    interface Observer {
        /**
         * Called when the visual state of the accessory sheet changes.
         *
         * @param visible True if the accessory sheet is showing, false otherwise.
         * @param color The background color of the accessory sheet.
         */
        void onAccessorySheetStateChanged(boolean visible, @ColorInt int color);
    }

    /**
     * Add an observer to be notified of visual changes to the accessory sheet.
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
