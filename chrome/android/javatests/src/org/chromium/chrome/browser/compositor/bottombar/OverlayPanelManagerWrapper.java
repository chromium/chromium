// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;

/**
 * Wrapper for OverlayPanelManager to record and test calls to the show and hide functions. This
 * class should contain the absolute count for the number of times a panel is shown or hidden as it
 * is the only one that controls panel display.
 */
public class OverlayPanelManagerWrapper extends OverlayPanelManager {
    private int mRequestPanelShowCount;
    private int mPanelHideCount;

    @Override
    public void requestPanelShow(OverlayPanel panel, @StateChangeReason int reason) {
        mRequestPanelShowCount++;
        super.requestPanelShow(panel, reason);
    }

    @Override
    public void notifyPanelClosed(OverlayPanel panel, @StateChangeReason int reason) {
        mPanelHideCount++;
        super.notifyPanelClosed(panel, reason);
    }

    public int getRequestPanelShowCount() {
        return mRequestPanelShowCount;
    }

    public int getPanelHideCount() {
        return mPanelHideCount;
    }
}
