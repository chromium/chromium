// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.app.Activity;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/**
 * Displays and manages the UI for browsing installed web apps. This is the top level coordinator
 * for the launchpad ui.
 */
class LaunchpadCoordinator {
    private final Activity mActivity;
    /** Main view for the Launchpad UI. */
    private final ViewGroup mMainView;
    /** The coordinator for displaying the app list in Launchpad. */
    private AppListCoordinator mAppListCoordinator;
    /** The coordinator for displaying the app management dialog. */
    private AppManagementMenuCoordinator mAppManagementMenuCoordinator;

    /**
     * Creates a new LaunchpadCoordinator.
     * @param activity The activity associated with the LaunchpadCoordinator.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param items The list of LaunchpadItems to be displayed.
     * @param isSeparateActivity Whether the launchpad UI will be shown in a separate activity than
     *                           the main Chrome activity.
     */
    LaunchpadCoordinator(Activity activity, Supplier<ModalDialogManager> modalDialogManagerSupplier,
            SettingsLauncher settingsLauncher, List<LaunchpadItem> items,
            boolean isSeparateActivity) {
        mActivity = activity;
        mMainView = (ViewGroup) activity.getLayoutInflater().inflate(
                R.layout.launchpad_page_layout, null);

        RecyclerView appListRecyclerView = getView().findViewById(R.id.launchpad_recycler);

        mAppManagementMenuCoordinator = new AppManagementMenuCoordinator(
                activity, modalDialogManagerSupplier, settingsLauncher);
        mAppListCoordinator =
                new AppListCoordinator(appListRecyclerView, mAppManagementMenuCoordinator, items);

        initializeActionBar(isSeparateActivity);

        View shadow = mMainView.findViewById(R.id.shadow);
        appListRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                shadow.setVisibility(
                        appListRecyclerView.canScrollVertically(-1) ? View.VISIBLE : View.GONE);
            }
        });
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

    private void initializeActionBar(boolean isSeparateActivity) {
        Toolbar toolbar = (Toolbar) mMainView.findViewById(R.id.toolbar);
        toolbar.setTitle(R.string.launchpad_title);
        toolbar.inflateMenu(R.menu.launchpad_action_bar_menu);
        if (!isSeparateActivity) toolbar.getMenu().removeItem(R.id.close_menu_id);
        toolbar.setOnMenuItemClickListener(this::onMenuItemClick);
    }

    private boolean onMenuItemClick(MenuItem menuItem) {
        if (menuItem.getItemId() == R.id.close_menu_id) {
            mActivity.finish();
            return true;
        }
        return false;
    }
}
