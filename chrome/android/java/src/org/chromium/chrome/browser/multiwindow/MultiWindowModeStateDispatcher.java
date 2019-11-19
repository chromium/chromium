// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * Monitors multi-window mode state changes in the associated activity and dispatches changes
 * to registered observers.
 *
 * Also contains methods related to multi-window/multi-instance support that require interaction
 * with the backing activity.
 */
public interface MultiWindowModeStateDispatcher {
    /**
     * An observer to be notified when multi-window mode changes.
     */
    interface MultiWindowModeObserver {
        /**
         * @param isInMultiWindowMode Whether the activity backing this state dispatcher is
         *         currently in multi-window mode.
         */
        void onMultiWindowModeChanged(boolean isInMultiWindowMode);
    }

    /**
     * Add an observer to the list.
     * @return True if the observer list changed as a result of the call.
     */
    boolean addObserver(MultiWindowModeObserver observer);

    /**
     * Remove an observer from the list if it is in the list.
     * @return True if an element was removed as a result of this call.
     */
    boolean removeObserver(MultiWindowModeObserver observer);

    /**
     * @return Whether the activity associated with this state dispatcher is currently in
     *         multi-window mode.
     */
    boolean isInMultiWindowMode();

    /**
     * @return Whether the system currently supports multiple displays.
     */
    boolean isInMultiDisplayMode();

    /**
     * See {@link MultiWindowUtils#isOpenInOtherWindowSupported(Activity)}.
     * @return Whether open in other window is supported for the activity associated with this
     *         state dispatcher.
     */
    boolean isOpenInOtherWindowSupported();

    /**
     * Returns the activity to use when handling "open in other window" or "move to other window".
     * Returns null if the current activity doesn't support opening/moving tabs to another activity.
     */
    Class<? extends Activity> getOpenInOtherWindowActivity();

    /**
     * Generates an intent to use when handling "open in other window" or "move to other
     * window" on a multi-instance capable device.
     * @return An intent with the proper class, flags, and extras for opening a tab or link in
     *         the other window.
     */
    Intent getOpenInOtherWindowIntent();

    /**
     * Generates the activity options used when handling "open in other window" or "move to other
     * window" on a multi-instance capable device.
     *
     * @return The ActivityOptions needed to open the content in another display.
     */
    Bundle getOpenInOtherWindowActivityOptions();
}