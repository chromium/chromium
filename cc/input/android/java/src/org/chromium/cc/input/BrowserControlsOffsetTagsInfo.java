// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cc.input;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

/** Java counterpart to the native cc::BrowserControlsOffsetTagsInfo. */
@DoNotMock("This is a simple value object.")
public final class BrowserControlsOffsetTagsInfo {
    public OffsetTag mBottomControlsOffsetTag;
    public OffsetTag mContentOffsetTag;
    public OffsetTag mTopControlsOffsetTag;
    public int mBottomControlsHeight;
    public int mBottomControlsAdditionalHeight;
    public int mTopControlsHeight;
    public int mTopControlsHairlineHeight;

    public BrowserControlsOffsetTagsInfo() {
        mBottomControlsOffsetTag = OffsetTag.createRandom();
        mContentOffsetTag = OffsetTag.createRandom();
        mTopControlsOffsetTag = OffsetTag.createRandom();
    }

    /**
     * Creates a new object containing the necessary information for viz to move the browser
     * controls. Only the OffsetTags are set, the other fields should be set properly before being
     * passed to the renderer.
     *
     * @param topControls An OffsetTag enabling viz to move the top controls. Note: For now, this
     *     tag is only used when AndroidBcivZeroBrowserFrames is enabled. When the flag is disabled,
     *     the top controls will be tagged with the content OffsetTag.
     * @param content An OffsetTag enabling viz to move the viewport of the rendered web contents.
     * @param bottomControls An OffsetTag enabling viz to move the bottom controls.
     */
    public BrowserControlsOffsetTagsInfo(
            OffsetTag topControls, OffsetTag content, OffsetTag bottomControls) {
        mBottomControlsOffsetTag = bottomControls;
        mContentOffsetTag = content;
        mTopControlsOffsetTag = topControls;
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

    @CalledByNative
    public int getBottomControlsHeight() {
        return mBottomControlsHeight;
    }

    @CalledByNative
    public int getBottomControlsAdditionalHeight() {
        return mBottomControlsAdditionalHeight;
    }

    @CalledByNative
    public int getTopControlsHeight() {
        return mTopControlsHeight;
    }

    @CalledByNative
    public int getTopControlsHairlineHeight() {
        return mTopControlsHairlineHeight;
    }
}
