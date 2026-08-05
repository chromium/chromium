// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseComponentProvider;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseComponentProvider.TabSelectionDelegate;
import org.chromium.chrome.browser.tab_bottom_sheet.PeekViewManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;

/**
 * Concrete implementation of {@link CoBrowseComponentProvider} for Contextual Tasks. Instantiates
 * specialized components.
 */
@NullMarked
@JNINamespace("contextual_tasks")
public class ContextualTaskBottomSheetComponentProvider implements CoBrowseComponentProvider {

    /**
     * Instantiates the content provider from C++.
     *
     * @return A new instance of {@link ContextualTaskBottomSheetComponentProvider}.
     */
    @CalledByNative
    private static ContextualTaskBottomSheetComponentProvider createProvider() {
        return new ContextualTaskBottomSheetComponentProvider();
    }

    private ContextualTaskBottomSheetComponentProvider() {}

    @Override
    public TabBottomSheetContent createContent(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            Runnable onBackPressed) {
        return new ContextualTaskBottomSheetContent(
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
        return null;
    }
}
