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

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContentProvider;

/**
 * Concrete implementation of {@link TabBottomSheetContentProvider} for Contextual Tasks.
 * Instantiates specialized {@link ContextualTaskBottomSheetContent} instances.
 */
@NullMarked
@JNINamespace("contextual_tasks")
public class ContextualTaskBottomSheetContentProvider implements TabBottomSheetContentProvider {

    /**
     * Instantiates the content provider from C++.
     *
     * @return A new instance of {@link ContextualTaskBottomSheetContentProvider}.
     */
    @CalledByNative
    private static ContextualTaskBottomSheetContentProvider createProvider() {
        return new ContextualTaskBottomSheetContentProvider();
    }

    private ContextualTaskBottomSheetContentProvider() {}

    @Override
    public TabBottomSheetContent create(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            @IdRes int emptyPlaceholderContainerId,
            Runnable onBackPressed) {
        return new ContextualTaskBottomSheetContent(
                contentView,
                fullHeightRatio,
                backgroundColor,
                peekViewHeight,
                peekViewContainerId,
                emptyPlaceholderContainerId,
                onBackPressed);
    }
}
