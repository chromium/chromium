// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cc.input;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Java counterpart to the native cc::BrowserControlsOffsetTags. */
@DoNotMock("This is a simple value object.")
@NullMarked
public final class BrowserControlsOffsetTags {
    private final @Nullable OffsetTag mTopControlsOffsetTag;
    private final @Nullable OffsetTag mContentOffsetTag;
    private final @Nullable OffsetTag mBottomControlsOffsetTag;

    public BrowserControlsOffsetTags() {
        mTopControlsOffsetTag = OffsetTag.createRandom();
        mContentOffsetTag = OffsetTag.createRandom();
        mBottomControlsOffsetTag = OffsetTag.createRandom();
    }

    public BrowserControlsOffsetTags(
            @Nullable OffsetTag topControls,
            @Nullable OffsetTag content,
            @Nullable OffsetTag bottomControls) {
        mTopControlsOffsetTag = topControls;
        mContentOffsetTag = content;
        mBottomControlsOffsetTag = bottomControls;

        // TOOD(peilinwang) Enforce that either both tags exist or are both empty
        // after the NoBrowserFramesWithAdditionalCaptures BCIV experiment ramps up.
        if (mTopControlsOffsetTag != null) {
            assert mContentOffsetTag != null;
        }
    }

    public boolean hasTags() {
        return mBottomControlsOffsetTag != null
                || mContentOffsetTag != null
                || mTopControlsOffsetTag != null;
    }

    @CalledByNative
    public @Nullable OffsetTag getBottomControlsOffsetTag() {
        return mBottomControlsOffsetTag;
    }

    @CalledByNative
    public @Nullable OffsetTag getContentOffsetTag() {
        return mContentOffsetTag;
    }

    @CalledByNative
    public @Nullable OffsetTag getTopControlsOffsetTag() {
        return mTopControlsOffsetTag;
    }
}
