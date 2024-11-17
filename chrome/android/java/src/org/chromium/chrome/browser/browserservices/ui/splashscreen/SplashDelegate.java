// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.splashscreen;

import android.view.View;

import org.chromium.chrome.browser.tab.Tab;

/** Delegate for {@link SplashController}. */
public interface SplashDelegate {
    /** Builds the splash view. */
    View buildSplashView();

    /**
     * Called when splash screen has been hidden.
     *
     * @param startTimestamp Time that the splash screen was shown.
     * @param endTimestamp Time that the splash screen was hidden.
     */
    void onSplashHidden(Tab tab, long startTimestamp, long endTimestamp);

    /** Returns whether to wait for a subsequent page load to hide the splash screen. */
    boolean shouldWaitForSubsequentPageLoadToHideSplash();
}
