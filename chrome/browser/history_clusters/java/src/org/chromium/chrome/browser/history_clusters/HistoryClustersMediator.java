// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.history_clusters.HistoryClustersToolbarProperties.QueryState;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

class HistoryClustersMediator extends EmptyBottomSheetObserver implements SearchDelegate {
    private final HistoryClustersBridge mHistoryClustersBridge;
    private final Context mContext;
    private final Resources mResources;
    private final ModelList mModelList;
    private final PropertyModel mBottomSheetToolbarModel;
    private final PropertyModel mToolbarModel;
    private final RoundedIconGenerator mIconGenerator;
    private final LargeIconBridge mLargeIconBridge;
    private final int mFaviconSize;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private Promise<HistoryClustersResult> mPromise;
    private Supplier<Intent> mHistoryActivityIntentFactory;

    /**
     * Create a new HistoryClustersMediator.
     * @param historyClustersBridge Provider of history clusters data.
     * @param largeIconBridge Bridge for fetching site icons.
     * @param context Android context from which UI configuration should be derived.
     * @param resources Android resources object from which strings, colors etc. should be fetched.
     * @param modelList Model list to which fetched cluster data should be pushed to.
     * @param bottomSheetToolbarModel Model for properties affecting the bottom sheet toolbar.
     * @param toolbarModel Model for properties affecting the "full page" toolbar shown in the
     *         history activity.
     * @param bottomSheetController Controller for interacting with the bottom sheet system, e.g. to
     *         request to show our content.
     * @param bottomSheetContent {@link BottomSheetContent} instance that tells the BottomSheet
     * @param historyActivityIntentFactory Supplier of an intent that targets the History activity.
     */
    HistoryClustersMediator(@NonNull HistoryClustersBridge historyClustersBridge,
            LargeIconBridge largeIconBridge, @NonNull Context context, @NonNull Resources resources,
            @NonNull ModelList modelList, @NonNull PropertyModel bottomSheetToolbarModel,
            @NonNull PropertyModel toolbarModel,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull BottomSheetContent bottomSheetContent,
            Supplier<Intent> historyActivityIntentFactory) {
        mHistoryClustersBridge = historyClustersBridge;
        mLargeIconBridge = largeIconBridge;
        mModelList = modelList;
        mContext = context;
        mResources = resources;
        mBottomSheetToolbarModel = bottomSheetToolbarModel;
        mToolbarModel = toolbarModel;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mHistoryActivityIntentFactory = historyActivityIntentFactory;
        mFaviconSize = mResources.getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mIconGenerator = FaviconUtils.createCircularIconGenerator(mContext);
    }

    // BottomSheetObserver
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        mModelList.clear();
        mBottomSheetController.removeObserver(this);
    }

    // SearchDelegate implementation.
    @Override
    public void onSearchTextChanged(String query) {
        mModelList.clear();
        query(query);
    }

    @Override
    public void onEndSearch() {
        mModelList.clear();
        query("");
    }

    void destroy() {
        mLargeIconBridge.destroy();
    }

    void startSearch(String query) {
        mToolbarModel.set(HistoryClustersToolbarProperties.QUERY_STATE, QueryState.forQuery(query));
    }

    void query(String query) {
        mPromise = mHistoryClustersBridge.queryClusters(query);
        mPromise.then(this::queryComplete);
    }

    void showBottomSheet(String query) {
        mBottomSheetToolbarModel.set(HistoryClustersBottomSheetToolbarProperties.QUERY_TEXT,
                formatQueryForDisplay(query));
        query(query);
        mPromise.then((Callback<HistoryClustersResult>) (unused) -> requestShowBottomSheet(query));
    }

    private void requestShowBottomSheet(String query) {
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, true)) {
            mBottomSheetController.addObserver(this);
            mBottomSheetToolbarModel.set(
                    HistoryClustersBottomSheetToolbarProperties.OPEN_ACTIVITY_BUTTON_CLICK_LISTENER,
                    (unused) -> openHistoryClustersInNewActivity(query));
        }
    }

    private void openHistoryClustersInNewActivity(String query) {
        Intent historyActivityIntent = mHistoryActivityIntentFactory.get();
        historyActivityIntent.putExtra(HistoryClustersIntent.EXTRA_SHOW_HISTORY_CLUSTERS, true);
        historyActivityIntent.putExtra(HistoryClustersIntent.EXTRA_HISTORY_CLUSTERS_QUERY, query);
        mContext.startActivity(historyActivityIntent);
    }

    private void queryComplete(HistoryClustersResult result) {
        for (HistoryCluster cluster : result.getClusters()) {
            for (ClusterVisit visit : cluster.getVisits()) {
                PropertyModel visitModel =
                        new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
                visitModel.set(HistoryClustersItemProperties.TITLE, visit.getTitle());
                visitModel.set(HistoryClustersItemProperties.URL, visit.getGURL().getHost());
                if (mLargeIconBridge != null) {
                    mLargeIconBridge.getLargeIconForUrl(visit.getGURL(), mFaviconSize,
                            (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
                                    int iconType) -> {
                                Drawable drawable = FaviconUtils.getIconDrawableWithoutFilter(icon,
                                        visit.getGURL(), fallbackColor, mIconGenerator, mResources,
                                        mFaviconSize);
                                visitModel.set(
                                        HistoryClustersItemProperties.ICON_DRAWABLE, drawable);
                            });
                }

                ListItem visitItem = new ListItem(ItemType.VISIT, visitModel);
                mModelList.add(visitItem);
            }
        }
    }

    private String formatQueryForDisplay(String query) {
        return new StringBuilder()
                .append(mResources.getString(R.string.quotation_mark_prefix))
                .append(query)
                .append(mResources.getString(R.string.quotation_mark_suffix))
                .toString();
    }
}
