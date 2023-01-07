// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tab.Tab;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Delegates actions when back navigation gesture is made.
 */
public interface BackActionDelegate {
    /**
     * Type of actions triggered by back navigation gesture.
     */
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

    /**
     * Performs an action upon back gesture.
     */
    void onBackGesture();

    /**
     * Returns whether back gesture navigation is possible. When {@code true}, this can override
     * the default navigability criteria based on navigation history. When {@code false}, the
     * default criteria will be used instead.
     */
    boolean isNavigable();
}
