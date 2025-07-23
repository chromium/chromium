// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Delegate interface for Hub bottom toolbar functionality.
 *
 * <p>This interface provides an abstraction layer for bottom toolbar operations, allowing
 * downstream implementations to provide custom bottom toolbar functionality while keeping the
 * upstream code generic and minimal.
 *
 * <p>Upstream provides an empty implementation that returns null/false for all operations, ensuring
 * no bottom toolbar functionality is active by default. Downstream implementations can provide full
 * bottom toolbar functionality by implementing this interface.
 */
@NullMarked
public interface HubBottomToolbarDelegate {
    /**
     * Initializes and returns the bottom toolbar view.
     *
     * <p>This method is responsible for inflating the bottom toolbar layout and attaching it to the
     * provided container.
     *
     * @param context The context for inflating layouts and accessing resources.
     * @param container The parent container where the bottom toolbar should be added.
     * @param paneManager The pane manager for observing pane changes and state.
     * @param hubColorMixer Mixes the Hub Overview Color.
     * @return The initialized HubBottomToolbarView, or null if bottom toolbar is not supported.
     */
    @Nullable HubBottomToolbarView initializeBottomToolbarView(
            Context context,
            ViewGroup container,
            PaneManager paneManager,
            HubColorMixer hubColorMixer);

    /**
     * Returns whether bottom toolbar functionality is enabled.
     *
     * @return True if bottom toolbar should be enabled, false otherwise.
     */
    boolean isBottomToolbarEnabled();

    /**
     * Returns an observable supplier for bottom toolbar visibility state.
     *
     * @return ObservableSupplier that emits Boolean values indicating toolbar visibility.
     */
    ObservableSupplier<Boolean> getBottomToolbarVisibilitySupplier();

    /** Cleans up resources and unregisters any observers or callbacks. */
    void destroy();
}
