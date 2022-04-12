// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

class HistoryClustersMediator {
    private final HistoryClustersBridge mHistoryClustersBridge;
    private final Context mContext;
    private final Resources mResources;
    private final ModelList mModelList;
    private final RoundedIconGenerator mIconGenerator;
    private final LargeIconBridge mLargeIconBridge;
    private final int mFaviconSize;
    private Promise<HistoryClustersResult> mPromise;

    /**
     * Create a new HistoryClustersMediator.
     * @param historyClustersBridge Provider of history clusters data.
     * @param largeIconBridge Bridge for fetching site icons.
     * @param context Android context from which UI configuration should be derived.
     * @param resources Android resources object from which strings, colors etc. should be fetched.
     * @param modelList Model list to which fetched cluster data should be pushed to.
     */
    HistoryClustersMediator(@NonNull HistoryClustersBridge historyClustersBridge,
            LargeIconBridge largeIconBridge, @NonNull Context context, @NonNull Resources resources,
            @NonNull ModelList modelList) {
        mHistoryClustersBridge = historyClustersBridge;
        mLargeIconBridge = largeIconBridge;
        mModelList = modelList;
        mContext = context;
        mResources = resources;
        mFaviconSize = mResources.getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mIconGenerator = FaviconUtils.createCircularIconGenerator(mContext);
    }

    void destroy() {
        mLargeIconBridge.destroy();
    }

    void query(String query) {
        mPromise = mHistoryClustersBridge.queryClusters(query);
        mPromise.then(this::queryComplete);
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
}
