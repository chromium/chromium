// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Orchestrates the displaying of a list of interactable tab groups. */
public class TabGroupListCoordinator {
    @IntDef({RowType.TAB_GROUP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RowType {
        int TAB_GROUP = 0;
    }

    private final RecyclerView mRecyclerView;
    private final SimpleRecyclerViewAdapter mSimpleRecyclerViewAdapter;
    private TabGroupListMediator mTabGroupListMediator;

    public TabGroupListCoordinator(
            Context context, TabGroupModelFilter filter, ProfileProvider profileProvider) {
        ModelList modelList = new ModelList();

        ViewBuilder<TabGroupRowView> layoutBuilder =
                new LayoutViewBuilder<>(R.layout.tab_group_row);
        mSimpleRecyclerViewAdapter = new SimpleRecyclerViewAdapter(modelList);
        mSimpleRecyclerViewAdapter.registerType(
                RowType.TAB_GROUP, layoutBuilder, new TabGroupRowViewBinder());

        mRecyclerView = new RecyclerView(context);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(context));
        mRecyclerView.setAdapter(mSimpleRecyclerViewAdapter);
        mRecyclerView.setItemAnimator(null);

        TabListFaviconProvider tabListFaviconProvider =
                new TabListFaviconProvider(
                        context, /* isTabStrip= */ false, R.dimen.default_favicon_corner_radius);
        // Native should always be initialized at this point.
        tabListFaviconProvider.initWithNative(profileProvider.getOriginalProfile());
        mTabGroupListMediator = new TabGroupListMediator(modelList, filter, tabListFaviconProvider);
    }

    /** Returns the root view of this component, allowing the parent to anchor in the hierarchy. */
    public View getView() {
        return mRecyclerView;
    }

    /** Permanently cleans up this component. */
    public void destroy() {
        mTabGroupListMediator.destroy();
        mSimpleRecyclerViewAdapter.destroy();
    }
}
