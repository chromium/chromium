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
import org.chromium.chrome.browser.actor.ui.ActorControlCoordinator.TabSelectionDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

/** Interface providing specialized components for different client features. */
@NullMarked
public interface TabBottomSheetComponentProvider {
    /** Destroys the component provider and releases any resources. */
    @CalledByNative
    default void destroy() {}

    /**
     * Instantiates a new instance of {@link TabBottomSheetContent}.
     *
     * @param contentView The content view shown inside the bottom sheet.
     * @param fullHeightRatio The target height ratio of the sheet in full state.
     * @param backgroundColor The background color of the sheet.
     * @param peekViewHeight The height of the peek view in pixels.
     * @param peekViewContainerId The resource ID for the peek view container.
     * @param emptyPlaceholderContainerId The resource ID for the empty placeholder container.
     * @param onBackPressed Callback run when the back button/swipe is triggered.
     * @return A non-null custom or default {@link TabBottomSheetContent}.
     */
    TabBottomSheetContent createContent(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            @IdRes int emptyPlaceholderContainerId,
            Runnable onBackPressed);

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
