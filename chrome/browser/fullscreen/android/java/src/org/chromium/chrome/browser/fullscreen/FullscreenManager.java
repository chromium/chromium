// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

/** An interface for observing and changing fullscreen mode. */
@NullMarked
public interface FullscreenManager {
    /**
     * A delegate for different behaviours of system callbacks. Used for Exclusive Access Manager
     * transition.
     */
    interface FullscreenManagerDelegate {
        /**
         * The callback for action when exit fullscreen e.g. when multi window mode was entered.
         *
         * @param tab {@link Tab} current active tab.
         */
        void onExitFullscreen(@Nullable Tab tab);
    }

    /** A listener that gets notified of changes to the fullscreen state. */
    interface Observer {
        /**
         * Called when entering fullscreen mode.
         * @param tab The tab whose content is entering fullscreen mode.
         * @param options Options to adjust fullscreen mode.
         */
        default void onEnterFullscreen(Tab tab, FullscreenOptions options) {}

        /**
         * Called when exiting fullscreen mode.
         * @param tab The tab whose content is exiting fullscreen mode.
         */
        default void onExitFullscreen(Tab tab) {}
    }

    /**
     * @param observer The {@link Observer} to be notified of fullscreen changes.
     */
    void addObserver(Observer observer);

    /**
     * @param listener The {@link Observer} to no longer be notified of fullscreen changes.
     */
    void removeObserver(Observer listener);

    /**
     * @return Whether the application is in persistent fullscreen mode.
     */
    boolean getPersistentFullscreenMode();

    /** Returns target display id for which full screen was requested. */
    long getFullscreenTargetDisplay();

    /**
     * @return Supplier of whether the activity is in persistent fullscreen mode.
     */
    ObservableSupplier<Boolean> getPersistentFullscreenModeSupplier();

    /**
     * Exits persistent fullscreen mode.  In this mode, the browser controls will be
     * permanently hidden until this mode is exited.
     */
    void exitPersistentFullscreenMode();

    /**
     * Enter fullscreen.
     * @param tab {@link Tab} that goes into fullscreen.
     * @param options Fullscreen options.
     */
    void onEnterFullscreen(Tab tab, FullscreenOptions options);

    /**
     * Exit fullscreen.
     *
     * @param tab {@link Tab} that goes out of fullscreen.
     */
    void onExitFullscreen(Tab tab);

    /** Sets the custom FullscreenManagerDelegate. */
    void setFullscreenManagerDelegate(FullscreenManagerDelegate delegate);
}
