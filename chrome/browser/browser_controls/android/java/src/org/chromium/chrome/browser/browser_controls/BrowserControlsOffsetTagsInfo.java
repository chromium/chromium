// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsOffsetTags;
import org.chromium.cc.input.OffsetTag;
import org.chromium.ui.BrowserControlsOffsetTagConstraints;
import org.chromium.ui.OffsetTagConstraints;

@DoNotMock("This is a simple value object.")
@NullMarked
public final class BrowserControlsOffsetTagsInfo {
    private final BrowserControlsOffsetTags mTags;
    public @Nullable OffsetTagConstraints mTopControlsConstraints;
    public @Nullable OffsetTagConstraints mContentConstraints;
    public @Nullable OffsetTagConstraints mBottomControlsConstraints;
    public int mTopControlsAdditionalHeight;
    public int mBottomControlsAdditionalHeight;

    public BrowserControlsOffsetTagsInfo() {
        mTags =
                new BrowserControlsOffsetTags(
                        OffsetTag.createRandom(),
                        OffsetTag.createRandom(),
                        OffsetTag.createRandom());
    }

    /**
     * Creates a new object containing the necessary information for viz to move the browser
     * controls. Only the OffsetTags are set, the other fields should be set properly before being
     * passed to the renderer.
     *
     * @param topControls An OffsetTag enabling viz to move the top controls.
     * @param content An OffsetTag enabling viz to move the viewport of the rendered web contents.
     * @param bottomControls An OffsetTag enabling viz to move the bottom controls.
     */
    public BrowserControlsOffsetTagsInfo(
            @Nullable OffsetTag topControls,
            @Nullable OffsetTag content,
            @Nullable OffsetTag bottomControls) {
        mTags = new BrowserControlsOffsetTags(topControls, content, bottomControls);
    }

    public boolean hasTags() {
        return mTags.hasTags();
    }

    public BrowserControlsOffsetTags getTags() {
        return mTags;
    }

    public BrowserControlsOffsetTagConstraints getConstraints() {
        return new BrowserControlsOffsetTagConstraints(
                mTopControlsConstraints, mContentConstraints, mBottomControlsConstraints);
    }

    public @Nullable OffsetTag getTopControlsOffsetTag() {
        return mTags.getTopControlsOffsetTag();
    }

    public @Nullable OffsetTag getContentOffsetTag() {
        return mTags.getContentOffsetTag();
    }

    public @Nullable OffsetTag getBottomControlsOffsetTag() {
        return mTags.getBottomControlsOffsetTag();
    }

    public int getBottomControlsAdditionalHeight() {
        return mBottomControlsAdditionalHeight;
    }

    public int getTopControlsAdditionalHeight() {
        return mTopControlsAdditionalHeight;
    }
}
