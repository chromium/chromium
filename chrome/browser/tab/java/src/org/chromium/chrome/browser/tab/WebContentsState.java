// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;

/** Contains the state for a WebContents. */
@NullMarked
public class WebContentsState {
    /**
     * Version number of the format used to save the WebContents navigation history, as returned by
     * TabStateJni.get().getContentsStateAsByteBuffer(). Version labels:
     *   0 - Chrome m18
     *   1 - Chrome m25
     *   2 - Chrome m26+
     */
    public static final int CONTENTS_STATE_CURRENT_VERSION = 2;

    /**
     * mBuffer should not be modified once it is set. Also, it is required to be a "direct" buffer
     * which is allocated outside the JVM heap, so that it can be accessed via the JNI direct buffer
     * methods, which means it has to be allocated with ByteBuffer.allocateDirect() or similar.
     */
    private final ByteBuffer mBuffer;

    private int mVersion;
    private @Nullable String mFallbackUrlForRestorationFailure;
    private static @Nullable WebContentsState sEmptyWebContentsState;

    public WebContentsState(ByteBuffer buffer) {
        assert buffer.isDirect();
        mBuffer = buffer;
        sEmptyWebContentsState = null;
    }

    public ByteBuffer buffer() {
        return mBuffer;
    }

    public int version() {
        return mVersion;
    }

    public void setVersion(int version) {
        mVersion = version;
    }

    /** @return Title currently being displayed in the saved state's current entry. */
    public @Nullable String getDisplayTitleFromState() {
        return WebContentsStateBridge.getDisplayTitleFromState(this);
    }

    /** @return URL currently being displayed in the saved state's current entry. */
    public @Nullable String getVirtualUrlFromState() {
        return WebContentsStateBridge.getVirtualUrlFromState(this);
    }

    /** Get the URL to be loaded if restoring the serialized web content state fails. */
    public @Nullable String getFallbackUrlForRestorationFailure() {
        return mFallbackUrlForRestorationFailure;
    }

    /** Set the URL to be loaded if restoring the serialized web content state fails. */
    public void setFallbackUrlForRestorationFailure(String fallbackUrlForRestorationFailure) {
        mFallbackUrlForRestorationFailure = fallbackUrlForRestorationFailure;
    }

    public static WebContentsState getTempWebContentsState() {
        if (sEmptyWebContentsState == null) {
            sEmptyWebContentsState = new WebContentsState(ByteBuffer.allocateDirect(0));
            sEmptyWebContentsState.setVersion(-1);
        }
        return sEmptyWebContentsState;
    }
}
