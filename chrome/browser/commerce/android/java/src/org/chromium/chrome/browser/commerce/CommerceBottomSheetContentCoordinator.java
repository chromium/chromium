// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.content.Context;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import java.util.function.Supplier;

/** Coordinator for building a commerce bottom sheet content. */
@NullMarked
public class CommerceBottomSheetContentCoordinator implements CommerceBottomSheetContentController {
    private static final long CONTENT_PROVIDER_TIMEOUT_MS = 200;

    private final List<CommerceBottomSheetContentProvider> mContentProviders = new ArrayList<>();
    private final CommerceBottomSheetContentMediator mMediator;
    private final RecyclerView mContentRecyclerView;
    private final View mCommerceBottomSheetContentContainer;
    private final ModelList mModelList;
    private @Nullable Long mSheetOpenTimeMs;

    @MonotonicNonNull private CallbackController mCallbackController;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Supplier<ScrimManager> mScrimManagerSupplier;

    public CommerceBottomSheetContentCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            final Supplier<ScrimManager> scrimSupplier,
            List<Supplier<CommerceBottomSheetContentProvider>> contentProviderSuppliers) {
        mModelList = new ModelList();

        mScrimManagerSupplier = scrimSupplier;
        SimpleRecyclerViewAdapter adapter =
                new SimpleRecyclerViewAdapter(mModelList) {
                    @Override
                    public void onViewRecycled(ViewHolder holder) {
                        super.onViewRecycled(holder);
                        ((ViewGroup) holder.itemView.findViewById(R.id.content_view_container))
                                .removeAllViews();
                    }
                };
        adapter.registerType(
                0,
                new LayoutViewBuilder(R.layout.commerce_bottom_sheet_content_item_container),
                CommerceBottomSheetContentBinder::bind);

        mCommerceBottomSheetContentContainer =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.commerce_bottom_sheet_content_container, /* root= */ null);
        mContentRecyclerView =
                mCommerceBottomSheetContentContainer.findViewById(
                        R.id.commerce_content_recycler_view);
        mContentRecyclerView.setAdapter(adapter);
        mContentRecyclerView.addItemDecoration(
                new ItemDecoration() {
                    @Override
                    public void getItemOffsets(
                            Rect outRect, View view, RecyclerView parent, State state) {
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
                    @Nullable PropertyModel mScrimModel;

                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        if (mSheetOpenTimeMs == null) {
                            mSheetOpenTimeMs = SystemClock.elapsedRealtime();
                        }
                        if (newState == SheetState.FULL) {
                            mContentRecyclerView.setNestedScrollingEnabled(true);
                            if (mScrimModel != null && !mMediator.isContentWrappingContent()) {
                                mScrimModel = bottomSheetController.createScrimParams();
                                mScrimManagerSupplier.get().showScrim(mScrimModel);
                            }
                        } else if (newState == SheetState.HALF) {
                            mContentRecyclerView.setNestedScrollingEnabled(false);
                        } else if (newState == SheetState.HIDDEN) {
                            if (mSheetOpenTimeMs != null) {
                                Long durationMs = SystemClock.elapsedRealtime() - mSheetOpenTimeMs;
                                RecordHistogram.recordTimesHistogram(
                                        "Commerce.BottomSheet.BrowsingTime", durationMs);
                            }
                            mSheetOpenTimeMs = null;
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

        initContentProviders(contentProviderSuppliers);

        mMediator =
                new CommerceBottomSheetContentMediator(
                        context,
                        mModelList,
                        mContentProviders.size(),
                        bottomSheetController,
                        mCommerceBottomSheetContentContainer);
    }

    @SuppressWarnings(
            "NullAway") // CallbackController#makeCancelable returns Callback<PropertyModel>
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

    private void initContentProviders(
            List<Supplier<CommerceBottomSheetContentProvider>> contentProviderSuppliers) {
        for (Supplier<CommerceBottomSheetContentProvider> contentProviderSupplier :
                contentProviderSuppliers) {
            if (contentProviderSupplier.get() != null) {
                mContentProviders.add(contentProviderSupplier.get());
            }
        }
    }

    public RecyclerView getRecyclerViewForTesting() {
        return mContentRecyclerView;
    }

    public ModelList getModelListForTesting() {
        return mModelList;
    }

    public View getContentViewForTesting() {
        return mCommerceBottomSheetContentContainer;
    }
}
