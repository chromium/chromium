// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

/**
 * An interface for observing and changing tab fullscreen mode. Note that this is not browser
 * fullscreen mode. Tab fullscreen refers to a renderer-initiated fullscreen mode (eg: from an
 * extension or via the JS fullscreen API) i.e. NOT triggered by the browser. Browser fullscreen
 * refers to the user putting the browser itself into fullscreen mode from the browser UI. The
 * difference is that tab fullscreen has implications for how the contents of the tab render (eg: a
 * video element may grow to consume the whole tab), whereas browser fullscreen mode doesn't. Tab
 * fullscreen must be entered by the WebContents to ensure the rendering differences are handled
 * correctly. Chrome on Android currently only supports tab fullscreen, not browser fullscreen.
 */
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

    /** Returns whether the application is in persistent fullscreen mode. */
    boolean getPersistentFullscreenMode();

    /** Returns target display id for which full screen was requested. */
    long getFullscreenTargetDisplay();

    /** Returns a supplier of whether the activity is in persistent fullscreen mode. */
    NonNullObservableSupplier<Boolean> getPersistentFullscreenModeSupplier();

    /**
     * Exits persistent fullscreen mode. In this mode, the browser controls will be permanently
     * hidden until this mode is exited.
     */
    void exitPersistentFullscreenMode();

    /**
     * Enter fullscreen in response to a request from the WebContents. DO NOT CALL THIS TO ENTER
     * FULLSCREEN. This does not tell the WebContents to enter fullscreen, it happens in response to
     * the WebContents entering fullscreen.
     *
     * @param tab {@link Tab} that goes into fullscreen.
     * @param options Fullscreen options.
     */
    void onEnterFullscreen(Tab tab, FullscreenOptions options);

    /**
     * Exit fullscreen. It is possible to call this from browser code as unlike entering fullscreen,
     * exiting fullscreen does not require a WebContents initiated request.
     *
     * @param tab {@link Tab} that goes out of fullscreen.
     */
    void onExitFullscreen(Tab tab);

    /** Sets the custom FullscreenManagerDelegate. */
    void setFullscreenManagerDelegate(FullscreenManagerDelegate delegate);
}
