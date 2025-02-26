// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.content.Context;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for building a commerce bottom sheet content. */
public class CommerceBottomSheetContentCoordinator implements CommerceBottomSheetContentController {
    private static final long CONTENT_PROVIDER_TIMEOUT_MS = 200;

    private List<CommerceBottomSheetContentProvider> mContentProviders = new ArrayList<>();
    private final CommerceBottomSheetContentMediator mMediator;
    private RecyclerView mContenRecyclerView;
    private View mCommerceBottomSheetContentContainer;
    private ModelList mModelList;

    private CallbackController mCallbackController;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Supplier<CommerceBottomSheetContentProvider>
            mPriceTrackingContentProviderSupplier;
    private final Supplier<ScrimManager> mScrimManagerSupplier;

    public CommerceBottomSheetContentCoordinator(
            Context context,
            @NonNull BottomSheetController bottomSheetController,
            final Supplier<ScrimManager> scrimSupplier,
            Supplier<CommerceBottomSheetContentProvider> priceTrackingContentProviderSupplier) {
        mModelList = new ModelList();
        mPriceTrackingContentProviderSupplier = priceTrackingContentProviderSupplier;

        mScrimManagerSupplier = scrimSupplier;
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(mModelList);
        adapter.registerType(
                0,
                new LayoutViewBuilder(R.layout.commerce_bottom_sheet_content_item_container),
                CommerceBottomSheetContentBinder::bind);

        mCommerceBottomSheetContentContainer =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.commerce_bottom_sheet_content_container, /* root= */ null);
        mContenRecyclerView =
                mCommerceBottomSheetContentContainer.findViewById(
                        R.id.commerce_content_recycler_view);
        mContenRecyclerView.setAdapter(adapter);
        mContenRecyclerView.addItemDecoration(
                new ItemDecoration() {
                    @Override
                    public void getItemOffsets(
                            @NonNull Rect outRect,
                            @NonNull View view,
                            @NonNull RecyclerView parent,
                            @NonNull State state) {
                        if (parent.getChildAdapterPosition(view) != 0) {
                            outRect.top =
                                    context.getResources()
                                            .getDimensionPixelOffset(
                                                    R.dimen.content_item_container_top_offset);
                        }
                    }
                });

        bottomSheetController.addObserver(
                new EmptyBottomSheetObserver() {
                    PropertyModel mScrimModel;

                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        if (newState == SheetState.FULL) {
                            mContenRecyclerView.suppressLayout(false);
                            if (!mMediator.isContentWrappingContent()) {
                                mScrimModel = bottomSheetController.createScrimParams();
                                mScrimManagerSupplier.get().showScrim(mScrimModel);
                            }
                        } else if (newState == SheetState.HALF) {
                            mContenRecyclerView.suppressLayout(true);
                        }
                    }

                    @Override
                    public void onSheetClosed(int reason) {
                        if (mScrimModel != null) {
                            mScrimManagerSupplier.get().hideScrim(mScrimModel, true);
                        }
                        mMediator.onBottomSheetClosed();
                        for (CommerceBottomSheetContentProvider provider : mContentProviders) {
                            provider.hideContentView();
                        }
                    }
                });

        initContentProviders();

        mMediator =
                new CommerceBottomSheetContentMediator(
                        mModelList,
                        mContentProviders.size(),
                        bottomSheetController,
                        mCommerceBottomSheetContentContainer);
    }

    @Override
    public void requestShowContent() {
        mCallbackController = new CallbackController();
        for (CommerceBottomSheetContentProvider provider : mContentProviders) {
            provider.requestContent(mCallbackController.makeCancelable(mMediator::onContentReady));
        }

        mHandler.postDelayed(
                () -> {
                    mCallbackController.destroy();
                    mMediator.timeOut();
                },
                CONTENT_PROVIDER_TIMEOUT_MS);
    }

    private void initContentProviders() {
        // TODO(362360807): Instantiate all the CommerceBottomSheetContentProvider here.
        mContentProviders.add(mPriceTrackingContentProviderSupplier.get());
    }

    public RecyclerView getRecyclerViewForTesting() {
        return mContenRecyclerView;
    }

    public ModelList getModelListForTesting() {
        return mModelList;
    }

    public View getContentViewForTesting() {
        return mCommerceBottomSheetContentContainer;
    }
}
