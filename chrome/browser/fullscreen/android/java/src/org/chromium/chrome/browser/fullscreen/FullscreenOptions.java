// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

/**
 * Options to control a fullscreen request.
 */
public class FullscreenOptions {
    private boolean mShowNavigationBar;

    // Used by FullscreenHtmlApiHandler internally to indicate that the fullscreen request
    // associated with this option got canceled at the pending state.
    private boolean mCanceled;

    /**
     * Constructs FullscreenOptions.
     *
     * @param showNavigationBar Whether the navigation bar should be shown.
     */
    public FullscreenOptions(boolean showNavigationBar) {
        mShowNavigationBar = showNavigationBar;
    }

    /**
     * @return Whether the navigation bar should be shown.
     */
    public boolean showNavigationBar() {
        return mShowNavigationBar;
    }

    void setCanceled() {
        mCanceled = true;
    }

    boolean canceled() {
        return mCanceled;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof FullscreenOptions)) {
            return false;
        }
        FullscreenOptions options = (FullscreenOptions) obj;
        return mShowNavigationBar == options.mShowNavigationBar;
    }
}
