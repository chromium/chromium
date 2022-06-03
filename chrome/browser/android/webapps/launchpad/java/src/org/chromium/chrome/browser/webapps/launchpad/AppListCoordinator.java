// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.browser_ui.widget.tile.TileViewBinder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/**
 * The coordinator to display the list of installed web apps in launchpad.
 */
class AppListCoordinator {
    /** The Mediator responsible for managing the list of apps to be displayed. */
    private AppListMediator mMediator;

    /** The coodinaator for showing the app management menu. */
    private AppManagementMenuCoordinator mMenuCoordinator;

    /**
     * The number of columns to show.
     * TODO(eirage): The number of column should not be fixed, but change with screen size.
     */
    private final int mColumns = 4;

    /**
     * The SimpleRecyclerViewAdapter expects each tile to have a unique id, but
     * in our case there is only one type.
     */
    static final int DEFAULT_TILE_TYPE = 0;

    /**
     * Creates and initialize the AppListCoordinator. It set up the adapter for the RecyclerView
     * and creates the AppListMediator for creating and updating the app list.
     * @param view The RecyclerView to hold the list of web apps.
     * @param items The list of LaunchpadItems to be displayed.
     */
    AppListCoordinator(RecyclerView view, AppManagementMenuCoordinator menuCoordinator,
            List<LaunchpadItem> items) {
        GridLayoutManager layoutManager = new GridLayoutManager(view.getContext(), mColumns);
        view.setLayoutManager(layoutManager);

        ModelList modelList = new ModelList();
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);
        adapter.registerType(
                DEFAULT_TILE_TYPE, AppListCoordinator::buildTile, TileViewBinder::bind);
        view.setAdapter(adapter);

        mMediator = new AppListMediator(view.getContext(), this, modelList, items);
        mMenuCoordinator = menuCoordinator;
    }

    void destroy() {
        mMediator.destroy();
        mMediator = null;
    }

    boolean showMenu(LaunchpadItem item) {
        mMenuCoordinator.show(item);
        return true;
    }

    private static TileView buildTile(ViewGroup parent) {
        TileView tile =
                (TileView) LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.launchpad_tile_view, parent, false /* attachToRoot */);
        tile.setClickable(true);
        return tile;
    }
}
