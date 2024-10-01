// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import static org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProperties.ALL_KEYS;
import static org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProperties.CUSTOM_VIEW;
import static org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProperties.HAS_TITLE;
import static org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProperties.TITLE;
import static org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProperties.TYPE;

import android.content.Context;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentCoordinator.ContentType;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Coordinator of the discounts bottom sheet content. */
public class DiscountsBottomSheetContentCoordinator implements CommerceBottomSheetContentProvider {

    private Context mContext;
    private ModelList mModelList;
    private View mDiscountsContentContainer;
    private RecyclerView mContentRecyclerView;
    private DiscountsBottomSheetContentMediator mMediator;

    public DiscountsBottomSheetContentCoordinator(
            @NonNull Context context, @NonNull Supplier<Tab> tabSupplier) {
        mContext = context;
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
        mContentRecyclerView.addItemDecoration(
                new ItemDecoration() {
                    @Override
                    public void getItemOffsets(
                            @NonNull Rect outRect,
                            @NonNull View view,
                            @NonNull RecyclerView parent,
                            @NonNull State state) {
                        // Avoid adding top padding to the first item in the list.
                        if (parent.getChildAdapterPosition(view) != 0) {
                            outRect.top =
                                    mContext.getResources()
                                            .getDimensionPixelOffset(
                                                    R.dimen.discount_item_divider_height);
                        }
                    }
                });

        mMediator = new DiscountsBottomSheetContentMediator(context, tabSupplier, mModelList);
    }

    @Override
    public void requestContent(Callback<PropertyModel> contentReadyCallback) {
        Callback<Boolean> showContentCallback =
                (hasDiscountsContent) -> {
                    contentReadyCallback.onResult(
                            hasDiscountsContent ? buildContentProviderModel() : null);
                };

        mMediator.requestShowContent(showContentCallback);
    }

    @Override
    public void hideContentView() {
        mMediator.closeContent();
    }

    private PropertyModel buildContentProviderModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(TYPE, ContentType.DISCOUNTS)
                .with(HAS_TITLE, true)
                .with(TITLE, mContext.getResources().getString(R.string.discount_container_title))
                .with(CUSTOM_VIEW, mDiscountsContentContainer)
                .build();
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
