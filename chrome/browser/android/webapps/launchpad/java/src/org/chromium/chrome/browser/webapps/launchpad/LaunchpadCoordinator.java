// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.app.Activity;
import android.view.ViewGroup;

/**
 * Displays and manages the UI for browsing installed web apps.
 */
class LaunchpadCoordinator {
    private ViewGroup mMainView;

    /**
     * Creates a new LaunchpadCoordinator.
     * @param activity The Activity associated with the LaunchpadCoordinator.
     */
    LaunchpadCoordinator(Activity activity) {
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
