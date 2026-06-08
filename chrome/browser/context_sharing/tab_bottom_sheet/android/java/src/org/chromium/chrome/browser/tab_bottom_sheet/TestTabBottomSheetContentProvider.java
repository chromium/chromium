// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/** Concrete test implementation of {@link TabBottomSheetContentProvider} for automated testing. */
@NullMarked
public class TestTabBottomSheetContentProvider implements TabBottomSheetContentProvider {
    @CalledByNative
    public TestTabBottomSheetContentProvider() {}

    @Override
    public TabBottomSheetContent create(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            @IdRes int emptyPlaceholderContainerId,
            Runnable onBackPressed) {
        return new TestTabBottomSheetContent(
                contentView,
                fullHeightRatio,
                backgroundColor,
                peekViewHeight,
                peekViewContainerId,
                emptyPlaceholderContainerId,
                onBackPressed);
    }
}
