// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.Supplier;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/**
 * Displays and manages the UI for browsing installed web apps. This is the top level coordinator
 * for the launchpad ui.
 */
class LaunchpadCoordinator {
    /** Main view for the Launchpad UI. */
    private final ViewGroup mMainView;
    /** The coordinator for displaying the app list in Launchpad. */
    private AppListCoordinator mAppListCoordinator;
    /** The coordinator for displaying the app management dialog. */
    private AppManagementMenuCoordinator mAppManagementMenuCoordinator;
    /**
     * Creates a new LaunchpadCoordinator.
     * @param activity The activity associated with the LaunchpadCoordinator.
     * @param items The list of LaunchpadItems to be displayed.
     */
    LaunchpadCoordinator(Activity activity, Supplier<ModalDialogManager> modalDialogManagerSupplier,
            List<LaunchpadItem> items) {
        mMainView = (ViewGroup) activity.getLayoutInflater().inflate(
                R.layout.launchpad_page_layout, null);

        RecyclerView appListRecyclerView = getView().findViewById(R.id.launchpad_recycler);

        mAppManagementMenuCoordinator =
                new AppManagementMenuCoordinator(activity, modalDialogManagerSupplier);
        mAppListCoordinator =
                new AppListCoordinator(appListRecyclerView, mAppManagementMenuCoordinator, items);
    }

    /**
     * @return The view that shows the main launchpad UI.
     */
    ViewGroup getView() {
        return mMainView;
    }

    void destroy() {
        mAppListCoordinator.destroy();
        mAppListCoordinator = null;
        mAppManagementMenuCoordinator.destroy();
        mAppManagementMenuCoordinator = null;
    }
}
