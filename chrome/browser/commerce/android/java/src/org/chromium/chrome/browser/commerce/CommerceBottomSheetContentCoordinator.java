// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.CallbackController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/** Coordinator for building a commerce bottom sheet content. */
public class CommerceBottomSheetContentCoordinator implements CommerceBottomSheetContentController {
    private static final long CONTENT_PROVIDER_TIMEOUT_MS = 200;

    /** Supported content types, the content is prioritized based on this order. */
    @IntDef({ContentType.PRICE_TRACKING, ContentType.DISCOUNTS, ContentType.PRICE_INSIGHTS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContentType {
        int PRICE_TRACKING = 0;
        int DISCOUNTS = 1;
        int PRICE_INSIGHTS = 2;
    }

    private List<CommerceBottomSheetContentProvider> mContentProviders = new ArrayList<>();
    private final CommerceBottomSheetContentMediator mMediator;
    // TODO(crbug.com/372820957): This can be a RecyclerView, but there's an issue with the
    // ButtonCompat that uses compound drawables within a RecyclerView displayed in the bottom
    // sheet. So we use a ListView here instead.
    private ListView mContentListView;
    private View mCommerceBottomSheetContentContainer;
    private ModelList mModelList;

    private final Context mContext;
    private CallbackController mCallbackController;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    public CommerceBottomSheetContentCoordinator(
            Context context, @NonNull BottomSheetController bottomSheetController) {
        mContext = context;
        mModelList = new ModelList();
        ModelListAdapter adapter = new ModelListAdapter(mModelList);
        adapter.registerType(
                0,
                new LayoutViewBuilder(R.layout.commerce_bottom_sheet_content_item_container),
                CommerceBottomSheetContentBinder::bind);

        mCommerceBottomSheetContentContainer =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.commerce_bottom_sheet_content_container, /* root= */ null);
        mContentListView =
                mCommerceBottomSheetContentContainer.findViewById(R.id.commerce_content_list_view);
        mContentListView.setAdapter(adapter);

        bottomSheetController.addObserver(
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(int reason) {
                        mMediator.onBottomSheetClosed();
                        for (CommerceBottomSheetContentProvider provider : mContentProviders) {
                            provider.hideContentView();
                        }
                    }
                });

        mMediator =
                new CommerceBottomSheetContentMediator(
                        mModelList,
                        mContentProviders.size(),
                        bottomSheetController,
                        mCommerceBottomSheetContentContainer);
        initContentProviders();
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
        // TODO(b/362360807): Instantiate all the CommerceBottomSheetContentProvider here.
    }

    public ListView getListViewForTesting() {
        return mContentListView;
    }

    public ModelList getModelListForTesting() {
        return mModelList;
    }

    public View getContentViewForTesting() {
        return mCommerceBottomSheetContentContainer;
    }
}
