// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContentProvider;

/**
 * Concrete implementation of {@link TabBottomSheetContentProvider} for Glic. Returns specialized
 * {@link GlicBottomSheetContent} and handles agent task termination.
 */
@JNINamespace("glic")
@NullMarked
public class GlicBottomSheetContentProvider implements TabBottomSheetContentProvider {
    private final Profile mProfile;

    /** JNI static factory method to create the provider. */
    @CalledByNative
    private static GlicBottomSheetContentProvider createProvider(Profile profile) {
        return new GlicBottomSheetContentProvider(profile);
    }

    private GlicBottomSheetContentProvider(Profile profile) {
        mProfile = profile;
    }

    @Override
    public TabBottomSheetContent create(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            @IdRes int emptyPlaceholderContainerId,
            Runnable onBackPressed) {
        return new GlicBottomSheetContent(
                contentView,
                fullHeightRatio,
                backgroundColor,
                peekViewHeight,
                peekViewContainerId,
                emptyPlaceholderContainerId,
                onBackPressed,
                mProfile);
    }
}
