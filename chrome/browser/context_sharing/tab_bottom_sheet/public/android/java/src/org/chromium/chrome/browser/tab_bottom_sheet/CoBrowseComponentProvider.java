// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;

import org.jni_zero.CalledByNative;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

/** Interface providing specialized components for different client features. */
@NullMarked
public interface CoBrowseComponentProvider {
    /** Delegate to handle tab selection. */
    interface TabSelectionDelegate {
        /** Switches to the specified tab. */
        void switchToTab(int tabId);
    }

    /** Destroys the component provider and releases any resources. */
    @CalledByNative
    default void destroy() {}

    /**
     * Sets up the placeholder view.
     *
     * @param placeholder The placeholder view to set up.
     * @return Whether the placeholder view was successfully set up.
     */
    default boolean setupPlaceholderView(TextViewWithCompoundDrawables placeholder) {
        return false;
    }

    /**
     * Instantiates a new instance of {@link TabBottomSheetContent}.
     *
     * @param contentView The content view shown inside the bottom sheet.
     * @param defaultHeightRatio The default height ratio of the sheet.
     * @param fullHeightRatio The full height ratio for the sheet.
     * @param backgroundColor The background color of the sheet.
     * @param peekViewHeight The height of the peek view in pixels.
     * @param peekViewContainerId The resource ID for the peek view container.
     * @param onBackPressed Callback run when the back button/swipe is triggered.
     * @return A custom or default {@link TabBottomSheetContent}, or null if not used.
     */
    default @Nullable TabBottomSheetContent createContent(
            View contentView,
            float defaultHeightRatio,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            Runnable onBackPressed) {
        return null;
    }

    /**
     * Instantiates a new instance of {@link PeekViewManager}.
     *
     * @param tabBottomSheetManager The bottom sheet manager.
     * @param profileSupplier The profile supplier.
     * @param tabSupplier The active tab supplier.
     * @param tabSelectionDelegate The tab selection delegate.
     * @return A nullable {@link PeekViewManager}.
     */
    default @Nullable PeekViewManager createPeekViewManager(
            TabBottomSheetManager tabBottomSheetManager,
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<Tab> tabSupplier,
            TabSelectionDelegate tabSelectionDelegate) {
        return null;
    }
}
