// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.splashscreen;

/** Observer interface for WebApp activity splashscreen. */
public interface SplashscreenObserver {
    /** Called when the activity's translucency is removed. */
    void onTranslucencyRemoved();

    /**
     * Called when the splash screen is hidden.
     *
     * @param startTimestamp Time that the splash screen was shown.
     * @param endTimestamp Time that the splash screen was hidden.
     */
    void onSplashscreenHidden(long startTimestamp, long endTimestamp);
}
