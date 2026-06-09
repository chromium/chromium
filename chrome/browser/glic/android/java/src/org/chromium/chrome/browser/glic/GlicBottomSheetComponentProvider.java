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
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetComponentProvider;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;

/**
 * Concrete implementation of {@link TabBottomSheetComponentProvider} for Glic. Returns specialized
 * components and handles agent task termination.
 */
@JNINamespace("glic")
@NullMarked
public class GlicBottomSheetComponentProvider implements TabBottomSheetComponentProvider {
    private final Profile mProfile;

    /** JNI static factory method to create the provider. */
    @CalledByNative
    private static GlicBottomSheetComponentProvider createProvider(Profile profile) {
        return new GlicBottomSheetComponentProvider(profile);
    }

    private GlicBottomSheetComponentProvider(Profile profile) {
        mProfile = profile;
    }

    @Override
    public TabBottomSheetContent createContent(
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
