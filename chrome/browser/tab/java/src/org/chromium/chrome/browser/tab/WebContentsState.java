// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import java.nio.ByteBuffer;

/** Contains the state for a WebContents. */
public class WebContentsState {
    /**
     * Version number of the format used to save the WebContents navigation history, as returned by
     * TabStateJni.get().getContentsStateAsByteBuffer(). Version labels:
     *   0 - Chrome m18
     *   1 - Chrome m25
     *   2 - Chrome m26+
     */
    public static final int CONTENTS_STATE_CURRENT_VERSION = 2;

    /** mBuffer should not be modified once it is set */
    private final ByteBuffer mBuffer;

    private int mVersion;
    private static WebContentsState sEmptyWebContentsState;

    public WebContentsState(ByteBuffer buffer) {
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
    public String getDisplayTitleFromState() {
        return WebContentsStateBridge.getDisplayTitleFromState(this);
    }

    /** @return URL currently being displayed in the saved state's current entry. */
    public String getVirtualUrlFromState() {
        return WebContentsStateBridge.getVirtualUrlFromState(this);
    }

    public static WebContentsState getTempWebContentsState() {
        if (sEmptyWebContentsState == null) {
            byte[] bytes = new byte[0];
            ByteBuffer buf = ByteBuffer.wrap(bytes);
            sEmptyWebContentsState = new WebContentsState(buf);
            sEmptyWebContentsState.setVersion(-1);
        }
        return sEmptyWebContentsState;
    }
}
