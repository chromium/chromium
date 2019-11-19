// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.view.View;

import org.chromium.chrome.browser.tab.Tab;

/** Delegate for {@link SplashController}. */
public interface SplashDelegate {
    /** Builds the splash view. */
    View buildSplashView();

    /**
     * Called when splash screen has been hidden.
     * @param tab
     * @param reason Reason that the splash screen was hidden.
     * @param startTimestamp Time that the splash screen was shown.
     * @param endTimestap Time that the splash screen was hidden.
     */
    void onSplashHidden(Tab tab, @SplashController.SplashHidesReason int reason,
            long startTimestamp, long endTimestamp);

    /** Returns whether to wait for a subsequent page load to hide the splash screen. */
    boolean shouldWaitForSubsequentPageLoadToHideSplash();
}
