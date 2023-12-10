// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

/** Delegate for minimizing the Custom Tab. */
public interface CustomTabMinimizeDelegate {
    /** Minimize the Custom Tab into picture-in-picture. */
    void minimize();

    /** Dismiss the currently minimized Custom Tab. */
    void dismiss();

    /** Returns whether the Custom Tab is currently minimized. */
    boolean isMinimized();

    /** Observer that observes the minimization events. */
    interface Observer {
        /**
         * Called when the minimization of the Custom Tab changed.
         *
         * @param minimized Whether the Custom Tab was minimized or un-minimized.
         */
        void onMinimizationChanged(boolean minimized);
    }

    /** Adds an {@link Observer}. */
    void addObserver(Observer observer);

    /** Removes an {@link Observer} */
    void removeObserver(Observer observer);
}
