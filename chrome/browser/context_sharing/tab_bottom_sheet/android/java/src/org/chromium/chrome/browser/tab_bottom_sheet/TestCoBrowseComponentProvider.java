// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;

import org.jni_zero.CalledByNative;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

/** Concrete test implementation of {@link CoBrowseComponentProvider} for automated testing. */
@NullMarked
public class TestCoBrowseComponentProvider implements CoBrowseComponentProvider {
    static boolean sUsePlaceholder;
    static @Nullable PeekViewManager sPeekViewManager;

    public static void setUsePlaceholderForTesting(boolean usePlaceholder) {
        ResettersForTesting.register(() -> sUsePlaceholder = false);
        sUsePlaceholder = usePlaceholder;
    }

    public static void setPeekViewManager(@Nullable PeekViewManager peekViewManager) {
        ResettersForTesting.register(() -> sPeekViewManager = null);
        sPeekViewManager = peekViewManager;
    }

    @CalledByNative
    public TestCoBrowseComponentProvider() {}

    @Override
    public boolean setupPlaceholderView(TextViewWithCompoundDrawables placeholder) {
        return sUsePlaceholder;
    }

    @Override
    public TabBottomSheetContent createContent(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            Runnable onBackPressed) {
        return new TestTabBottomSheetContent(
                contentView,
                fullHeightRatio,
                backgroundColor,
                peekViewHeight,
                peekViewContainerId,
                onBackPressed);
    }

    @Override
    public @Nullable PeekViewManager createPeekViewManager(
            TabBottomSheetManager tabBottomSheetManager,
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<Tab> tabSupplier,
            TabSelectionDelegate tabSelectionDelegate) {
        return sPeekViewManager;
    }
}
