// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.navigation_pane.NavigationPaneAdapterFactory;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Coordinator for the history desktop navigation pane. */
@NullMarked
public class HistoryDesktopNavigationCoordinator {
    private final Context mContext;
    private final View mView;
    private final ModelList mModelList;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final RecyclerView mRecyclerView;
    private @Nullable HistoryDesktopNavigationMediator mMediator;

    /**
     * Constructs a {@link HistoryDesktopNavigationCoordinator}.
     *
     * @param context The current Activity context.
     * @param navigationPane The root view of the navigation pane.
     * @param onHistoryClicked Runnable executed when the history item is clicked.
     * @param onTabsFromOtherDevicesClicked Runnable executed when the tabs from other devices item
     *     is clicked.
     */
    public HistoryDesktopNavigationCoordinator(
            Context context,
            View navigationPane,
            Runnable onHistoryClicked,
            Runnable onTabsFromOtherDevicesClicked) {
        mContext = context;
        mView = navigationPane;
        mModelList = new ModelList();

        mAdapter = NavigationPaneAdapterFactory.createAdapter(mContext, mModelList);

        mRecyclerView = mView.findViewById(R.id.navigation_recycler_view);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mContext));
        mRecyclerView.setAdapter(mAdapter);

        mMediator =
                new HistoryDesktopNavigationMediator(
                        mContext, mModelList, onHistoryClicked, onTabsFromOtherDevicesClicked);
    }

    /** Destroys the coordinator and its resources. */
    public void destroy() {
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
    }
}
