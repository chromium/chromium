// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyModel;

/** Manager interface for the tab bottom sheet. */
@NullMarked
public interface TabBottomSheetManager extends Destroyable {

    // Interface for the native to communicate with the tab bottom sheet manager.
    interface NativeInterfaceDelegate {
        // Method called when the bottom sheet is closed.
        void onBottomSheetClosed();

        // Called when the bottom sheet is opened, or when the bottom sheet state changes.
        void onBottomSheetOpened(boolean isExpanded);

        // Method called when the bottom sheet is suppressed.
        void onBottomSheetSuppressed();
    }

    /** Attempts to close the Tab BottomSheet. */
    void tryToCloseBottomSheet(boolean animate);

    /** Sets the model for the peek view. */
    void setPeekViewModel(PropertyModel model);

    /** Removes the model for the peek view. */
    void removePeekViewModel();

    /**
     * Sets whether the bottom sheet is expanded.
     *
     * @param expanded Whether the bottom sheet should be expanded.
     */
    void setSheetExpanded(boolean expanded);

    /** Returns whether the bottom sheet is initialized. */
    boolean isSheetInitialized();

    /** Returns whether the bottom sheet is showing. */
    boolean isSheetShowing();

    /**
     * @return Whether the bottom sheet is currently in peek mode.
     */
    boolean isInPeekMode();

    /**
     * Initializes the ReadAloud integration.
     *
     * @param activePlaybackTabSupplier The active playback tab supplier.
     * @param stopPlaybackCallback Callback to stop ReadAloud playback.
     */
    void initReadAloudIntegration(
            NullableObservableSupplier<Tab> activePlaybackTabSupplier,
            Runnable stopPlaybackCallback);
}
