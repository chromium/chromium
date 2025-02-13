// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cc.input;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/** Java counterpart to the native cc::BrowserControlsOffsetTagModifications. */
@DoNotMock("This is a simple value object.")
@NullMarked
public final class BrowserControlsOffsetTagModifications {
    private final BrowserControlsOffsetTags mTags;

    // Renderer must move the top and bottom controls further to ensure that parts of the controls
    // that are drawn over the web contents disappear when controls are scrolled off screen.
    // Currently this only includes the shadow of the top and bottom controls.
    private final int mTopControlsAdditionalHeight;
    private final int mBottomControlsAdditionalHeight;

    public BrowserControlsOffsetTagModifications(
            BrowserControlsOffsetTags tags,
            int topControlsAdditionalHeight,
            int bottomControlsAdditionalHeight) {
        mTags = tags;
        mTopControlsAdditionalHeight = topControlsAdditionalHeight;
        mBottomControlsAdditionalHeight = bottomControlsAdditionalHeight;
    }

    @CalledByNative
    public BrowserControlsOffsetTags getTags() {
        return mTags;
    }

    @CalledByNative
    public int getTopControlsAdditionalHeight() {
        return mTopControlsAdditionalHeight;
    }

    @CalledByNative
    public int getBottomControlsAdditionalHeight() {
        return mBottomControlsAdditionalHeight;
    }
}
