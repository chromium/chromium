// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import org.chromium.ui.util.TokenHolder;

/** Allows for manipulating visibility of the browser controls, as well as retrieving state. */
public interface BrowserControlsVisibilityManager extends BrowserControlsStateProvider {
    /**
     * @return The visibility delegate that allows browser UI to control the browser control
     *         visibility.
     */
    BrowserStateBrowserControlsVisibilityDelegate getBrowserVisibilityDelegate();

    /**
     * Shows the Android browser controls view.
     * @param animate Whether a slide-in animation should be run.
     */
    void showAndroidControls(boolean animate);

    /**
     * Attempts to restore the controls to the position that they should be at corresponding
     * to any set constraints, if they've been forced to a different position.
     * Only actionable if {@link #offsetOverridden()} is true.
     */
    void restoreControlsPositions();

    /**
     * Indicates whether the browser controls offsets are currently overridden by a Java-side
     * animation started by #showAndroidControls(boolean).
     * @return {@code true} if browser controls offsets are overridden by animation.
     */
    boolean offsetOverridden();

    /**
     * Forces the Android controls to hide an replaces the provided oldToken. While there are
     * acquired tokens the browser controls Android view will always be hidden, otherwise they will
     * show/hide based on position.
     *
     * Note: this only affects the Android controls.
     *
     * @param oldToken the oldToken to clear. Pass {@link TokenHolder#INVALID_TOKEN} if no token is
     *         being held.
     */
    int hideAndroidControlsAndClearOldToken(int oldToken);

    /** Release a hiding token returned from {@link #hideAndroidControlsAndClearOldToken(int)}. */
    void releaseAndroidControlsHidingToken(int token);
}
