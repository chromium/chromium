// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.app.Activity;
import android.view.ViewGroup;

import java.util.List;

/**
 * Displays and manages the UI for browsing installed web apps. This is the top level coordinator
 * for the launchpad ui.
 */
class LaunchpadCoordinator {
    private final ViewGroup mMainView;

    /**
     * Creates a new LaunchpadCoordinator.
     * @param context The context associated with the LaunchpadCoordinator.
     * @param items The list of LaunchpadItems to be displayed.
     */
    LaunchpadCoordinator(Activity activity, List<LaunchpadItem> items) {
        mMainView = (ViewGroup) activity.getLayoutInflater().inflate(
                R.layout.launchpad_page_layout, null);
    }

    /**
     * @return The view that shows the main launchpad UI.
     */
    ViewGroup getView() {
        return mMainView;
    }

    void destroy() {}
}
