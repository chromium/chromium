// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tab.Tab;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Delegates actions when back navigation gesture is made. */
public interface BackActionDelegate {
    /** Type of actions triggered by back navigation gesture. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ActionType.NAVIGATE_BACK, ActionType.CLOSE_TAB, ActionType.EXIT_APP})
    public @interface ActionType {
        int NAVIGATE_BACK = 0;
        int CLOSE_TAB = 1;
        int EXIT_APP = 2;
    }

    /**
     * Returns the action to take for a back navigation on the given tab.
     * @param tab Tab where the navigation is taking place.
     */
    @ActionType
    int getBackActionType(Tab tab);

    /** Performs an action upon back gesture. */
    void onBackGesture();

    /**
     * Called when user performs a gesture but nothing is expected to occur, like trying to forward
     * a page which has empty forward history stack.
     */
    default void onGestureUnhandled() {}

    /**
     * Called when user is performing a gesture and then an action is waiting to occur later, like
     * the user is swiping and the page is expected to be navigated back if the gesture is finished.
     * This function does not guarantee the action will occur since the user might cancel the action
     * right after this method is called.
     */
    default void onGestureHandled() {}
}
