// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.view.LayoutInflater;
import android.view.View;

import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

@NullMarked
public class CustomSearchEngineListCoordinator {
    private final ModelList mModelList = new ModelList();
    private final SimpleRecyclerViewAdapter mAdapter;

    public CustomSearchEngineListCoordinator() {
        mAdapter = new SimpleRecyclerViewAdapter(mModelList);
        mAdapter.registerType(
                0,
                parent ->
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.custom_search_engine_item, parent, false),
                CustomSearchEngineViewBinder::bind);

        // Todo: Replace with Template URL data.
    }

    public void onViewBound(View rootView) {
        RecyclerView recyclerView;
        if (rootView instanceof RecyclerView) {
            recyclerView = (RecyclerView) rootView;
        } else {
            // rootView should be bound to recycler view in
            // CustomSearchEngineListPreference::onBindViewHolder.
            return;
        }
        if (recyclerView.getAdapter() == mAdapter) return;

        LinearLayoutManager layoutManager = new LinearLayoutManager(recyclerView.getContext());
        recyclerView.setLayoutManager(layoutManager);
        DividerItemDecoration divider =
                new DividerItemDecoration(
                        recyclerView.getContext(), layoutManager.getOrientation());
        recyclerView.addItemDecoration(divider);
        recyclerView.setAdapter(mAdapter);
    }
}
