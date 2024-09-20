// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Coordinator of the discounts bottom sheet content. */
public class DiscountsBottomSheetContentCoordinator {

    private ModelList mModelList;
    private View mDiscountsContentContainer;
    private RecyclerView mContentRecyclerView;

    public DiscountsBottomSheetContentCoordinator(@NonNull Context context) {
        mModelList = new ModelList();
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(mModelList);
        adapter.registerType(
                0,
                new LayoutViewBuilder(R.layout.discount_item_container),
                DiscountsBottomSheetContentViewBinder::bind);
        mDiscountsContentContainer =
                LayoutInflater.from(context)
                        .inflate(R.layout.discounts_content_container, /* root= */ null);
        mContentRecyclerView =
                mDiscountsContentContainer.findViewById(R.id.discounts_content_recycler_view);
        mContentRecyclerView.setAdapter(adapter);
    }

    RecyclerView getRecyclerViewForTesting() {
        return mContentRecyclerView;
    }

    ModelList getModelListForTesting() {
        return mModelList;
    }

    View getContentViewForTesting() {
        return mDiscountsContentContainer;
    }
}
