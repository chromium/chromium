// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import android.content.Context;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.chrome.browser.recent_tabs.ui.TabItemViewBinder.BindContext;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the detail screens (device select, review tabs) of the Restore Tabs on FRE promo.
 */
public class RestoreTabsDetailScreenCoordinator {
    private static final int RECYLCER_VIEW_DIRECTION_UP = -1;
    private static final int RECYLCER_VIEW_DIRECTION_DOWN = 1;
    private final RecyclerView mRecyclerView;
    private BindContext mBindContext;
    private FaviconHelper mFaviconHelper;

    /** The delegate of the class. */
    public interface Delegate {
        /** The user clicked on the select/deselect all tabs item. */
        void onChangeSelectionStateForAllTabs();

        /** The user clicked on restoring all selected tabs. */
        void onSelectedTabsChosen();
    }

    public RestoreTabsDetailScreenCoordinator(
            Context context, View view, PropertyModel model, Profile profile) {
        mFaviconHelper = new FaviconHelper();
        mBindContext =
                new BindContext(
                        new DefaultFaviconHelper(),
                        FaviconUtils.createCircularIconGenerator(context),
                        mFaviconHelper,
                        profile);

        mRecyclerView = view.findViewById(R.id.restore_tabs_detail_screen_recycler_view);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.addItemDecoration(
                new RestoreTabsDetailItemDecoration(
                        context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.restore_tabs_detail_sheet_spacing_vertical)));

        mRecyclerView.addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v, int l, int t, int r, int b, int oL, int oT, int oR, int oB) {
                        if (mRecyclerView.canScrollVertically(RECYLCER_VIEW_DIRECTION_UP)
                                || mRecyclerView.canScrollVertically(
                                        RECYLCER_VIEW_DIRECTION_DOWN)) {
                            view.findViewById(R.id.restore_tabs_bottom_toolbar_divider)
                                    .setVisibility(View.VISIBLE);
                        } else {
                            view.findViewById(R.id.restore_tabs_bottom_toolbar_divider)
                                    .setVisibility(View.GONE);
                        }
                    }
                });

        RestoreTabsDetailScreenViewBinder.ViewHolder viewHolder =
                new RestoreTabsDetailScreenViewBinder.ViewHolder(view, mBindContext);

        PropertyModelChangeProcessor.create(
                model, viewHolder, RestoreTabsDetailScreenViewBinder::bind);
    }

    public void destroy() {
        mFaviconHelper.destroy();
        mFaviconHelper = null;
        mBindContext.destroy();
        mBindContext = null;
    }
}
