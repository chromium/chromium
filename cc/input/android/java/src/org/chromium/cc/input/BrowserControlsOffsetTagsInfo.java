// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cc.input;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

/** Java counterpart to the native cc::BrowserControlsOffsetTagsInfo. */
@DoNotMock("This is a simple value object.")
public final class BrowserControlsOffsetTagsInfo {
    public OffsetTag mTopControlsOffsetTag;
    public int mTopControlsHeight;

    public BrowserControlsOffsetTagsInfo(OffsetTag topControlsOffsetTag) {
        mTopControlsOffsetTag = topControlsOffsetTag;
    }

    @CalledByNative
    public OffsetTag getTopControlsOffsetTag() {
        return mTopControlsOffsetTag;
    }

    @CalledByNative
    public int getTopControlsHeight() {
        return mTopControlsHeight;
    }
}
