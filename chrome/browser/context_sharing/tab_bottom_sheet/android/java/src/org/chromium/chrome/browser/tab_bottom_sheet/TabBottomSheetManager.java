// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.View;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
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
    }

    /** Attempts to close the Tab BottomSheet. */
    void tryToCloseBottomSheet(boolean animate);

    /**
     * Sets whether the bottom sheet can not be suppressed.
     *
     * <p>This is only used when the bottom sheet is already showing, and another sheet wishes to be
     * shown. This does not affect the priority of the bottom sheet.
     *
     * @param canNotBeSuppressed Whether the bottom sheet can not be suppressed.
     */
    void setCanNotBeSuppressed(boolean canNotBeSuppressed);

    /**
     * Sets the peek view to be displayed.
     *
     * @param peekView The peek view to be displayed.
     */
    void setPeekView(View peekView);

    /**
     * Removes the peek view if it matches the provided view.
     *
     * @param peekView The peek view to be removed.
     */
    void removePeekView(View peekView);

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
     * Sets the supplier for the active playback tab from ReadAloud.
     *
     * @param activePlaybackTabSupplier The supplier.
     */
    void setReadAloudActivePlaybackTabSupplier(
            NullableObservableSupplier<Tab> activePlaybackTabSupplier);
}
