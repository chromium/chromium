// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cc.input;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

/** Java counterpart to the native cc::BrowserControlsOffsetTags. */
@DoNotMock("This is a simple value object.")
public final class BrowserControlsOffsetTags {
    private final OffsetTag mTopControlsOffsetTag;
    private final OffsetTag mContentOffsetTag;
    private final OffsetTag mBottomControlsOffsetTag;

    public BrowserControlsOffsetTags() {
        mTopControlsOffsetTag = OffsetTag.createRandom();
        mContentOffsetTag = OffsetTag.createRandom();
        mBottomControlsOffsetTag = OffsetTag.createRandom();
    }

    public BrowserControlsOffsetTags(
            OffsetTag topControls, OffsetTag content, OffsetTag bottomControls) {
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
    public OffsetTag getBottomControlsOffsetTag() {
        return mBottomControlsOffsetTag;
    }

    @CalledByNative
    public OffsetTag getContentOffsetTag() {
        return mContentOffsetTag;
    }

    @CalledByNative
    public OffsetTag getTopControlsOffsetTag() {
        return mTopControlsOffsetTag;
    }
}
