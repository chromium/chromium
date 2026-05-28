// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;

/** Concrete test implementation of {@link TabBottomSheetContent} for automated testing. */
@NullMarked
public class TestTabBottomSheetContent extends TabBottomSheetContent {
    public TestTabBottomSheetContent(
            View contentView, float fullHeightRatio, @ColorInt int backgroundColor) {
        super(contentView, fullHeightRatio, backgroundColor);
    }

    @Override
    public @Nullable GlowSpec getSheetBackgroundGlowSpecOverride() {
        return null;
    }
}
