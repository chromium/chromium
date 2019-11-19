// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

/**
 * Default implementation of an observer of events on the progress bar.
 */
public class OverlayContentProgressObserver {

    /**
     * Called when the progress bar would start showing (loading started).
     */
    public void onProgressBarStarted() {}

    /**
     * Called when progress has updated.
     * @param progress The current progress in the range [0,1].
     */
    public void onProgressBarUpdated(float progress) {}

    /**
     * Called when the progress bar has finished.
     */
    public void onProgressBarFinished() {}
}
