// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

/**
 * Interface to control the OverviewMode.
 */
public interface OverviewModeController extends OverviewModeBehavior {
    /**
     * Hide the overview.
     * @param animate Whether we should animate while hiding.
     */
    void hideOverview(boolean animate);

    /**
     * Show the overview.
     * @param animate Whether we should animate while showing.
     */
    void showOverview(boolean animate);
}