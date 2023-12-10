// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

/** Options to control a fullscreen request. */
public class FullscreenOptions {
    /** Whether the navigation bar should be shown. */
    public final boolean showNavigationBar;

    /** Whether the status bar should be shown. */
    public final boolean showStatusBar;

    // Used by FullscreenHtmlApiHandler internally to indicate that the fullscreen request
    // associated with this option got canceled at the pending state.
    private boolean mCanceled;

    /**
     * Constructs FullscreenOptions.
     *
     * @param showNavigationBar Whether the navigation bar should be shown.
     * @param showStatusBar Whether the status bar should be shown.
     */
    public FullscreenOptions(boolean showNavigationBar, boolean showStatusBar) {
        this.showNavigationBar = showNavigationBar;
        this.showStatusBar = showStatusBar;
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
        return showNavigationBar == options.showNavigationBar
                && showStatusBar == options.showStatusBar;
    }

    @Override
    public String toString() {
        return "FullscreenOptions(showNavigationBar="
                + showNavigationBar
                + ",showStatusBar="
                + showStatusBar
                + ", canceled="
                + mCanceled
                + ")";
    }
}
