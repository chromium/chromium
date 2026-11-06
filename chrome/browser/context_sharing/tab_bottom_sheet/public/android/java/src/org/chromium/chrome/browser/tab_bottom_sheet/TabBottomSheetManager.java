// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import androidx.annotation.ColorInt;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

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

        /** Returns the tab the bottom sheet was shown for, or null if unknown. */
        @Nullable Tab getTab();
    }

    /** Attempts to close the Tab BottomSheet. */
    void tryToCloseBottomSheet(boolean animate);



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
     * Returns the background color of the bottom sheet that will come back in peek mode on {@code
     * tab} once the tab switcher finishes hiding, or null if no sheet will come back then.
     */
    @Nullable
    @ColorInt
    Integer getSheetColorToReshowAfterTabSwitcher(Tab tab);

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
