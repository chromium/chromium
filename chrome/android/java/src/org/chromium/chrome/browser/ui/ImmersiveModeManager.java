// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

/**
 * Manages drawing an immersive UI mode. The lifecycle of the immersive mode manager is tied
 * to the activity. There is exactly one per activity and once created it lives until the activity
 * is destroyed.
 */
public interface ImmersiveModeManager {
    /**
     * An observer to be notified about changes related to immersive UI mode.
     */
    interface ImmersiveModeObserver {
        /**
         * Called when entering or exiting immersive mode.
         * @param inImmersiveMode Whether the UI is currently in immersive mode.
         */
        void onImmersiveModeChanged(boolean inImmersiveMode);

        /**
         * Called when the inset to apply to bottom anchored UI elements changes.
         * @param bottomUiInsetPx The inset to apply in pixels.
         */
        void onBottomUiInsetChanged(int bottomUiInsetPx);
    }

    /**
     * @return Whether immersive UI mode is supported. If not supported, observers may still be
     *         registered, but they will never be called.
     */
    boolean isImmersiveModeSupported();

    /**
     * @return The inset to apply to bottom anchored UI elements in pixels.
     */
    default int getBottomUiInsetPx() {
        return 0;
    }

    /**
     * Add an observer to be notified of changes related to immersive UI mode.
     * @param observer The observer to add.
     */
    void addObserver(ImmersiveModeObserver observer);

    /**
     * Remove a previously registered observer.
     * @param observer The observer to remove.
     */
    void removeObserver(ImmersiveModeObserver observer);

    /**
     * Called when the containing activity is destroyed to clean up state.
     */
    void destroy();
}
