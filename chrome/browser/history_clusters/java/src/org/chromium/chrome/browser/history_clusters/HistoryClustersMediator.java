// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.Function;
import org.chromium.base.Promise;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.history_clusters.HistoryClustersToolbarProperties.QueryState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

class HistoryClustersMediator implements SearchDelegate {
    private final HistoryClustersBridge mHistoryClustersBridge;
    private final Context mContext;
    private final Resources mResources;
    private final ModelList mModelList;
    private final PropertyModel mToolbarModel;
    private final RoundedIconGenerator mIconGenerator;
    private final LargeIconBridge mLargeIconBridge;
    private final int mFaviconSize;
    private final Supplier<Tab> mTabSupplier;
    private Promise<HistoryClustersResult> mPromise;
    private Supplier<Intent> mHistoryActivityIntentFactory;
    private final boolean mIsSeparateActivity;
    private Function<GURL, Intent> mOpenUrlIntentCreator;
    private CallbackController mCallbackController = new CallbackController();

    /**
     * Create a new HistoryClustersMediator.
     * @param historyClustersBridge Provider of history clusters data.
     * @param largeIconBridge Bridge for fetching site icons.
     * @param context Android context from which UI configuration should be derived.
     * @param resources Android resources object from which strings, colors etc. should be fetched.
     * @param modelList Model list to which fetched cluster data should be pushed to.
     * @param toolbarModel Model for properties affecting the "full page" toolbar shown in the
     *         history activity.
     * @param historyActivityIntentFactory Supplier of an intent that targets the History activity.
     * @param tabSupplier Supplier of the currently active tab. Null in cases where there isn't a
     *         tab, e.g. when we're operating in a dedicated history activity.
     * @param isSeparateActivity Whether the Journeys UI this mediator supports is running in a
     *         separate activity (as opposed to in a tab). This informs, e.g. whether viewing a url
     *         should launch an intent or directly navigate a tab.
     * @param openUrlIntentCreator Function that creates an intent that opens the given url in the
     *         correct main browsing activity.
     */
    HistoryClustersMediator(@NonNull HistoryClustersBridge historyClustersBridge,
            LargeIconBridge largeIconBridge, @NonNull Context context, @NonNull Resources resources,
            @NonNull ModelList modelList, @NonNull PropertyModel toolbarModel,
            Supplier<Intent> historyActivityIntentFactory, @Nullable Supplier<Tab> tabSupplier,
            boolean isSeparateActivity, Function<GURL, Intent> openUrlIntentCreator) {
        mHistoryClustersBridge = historyClustersBridge;
        mLargeIconBridge = largeIconBridge;
        mModelList = modelList;
        mContext = context;
        mResources = resources;
        mToolbarModel = toolbarModel;
        mHistoryActivityIntentFactory = historyActivityIntentFactory;
        mTabSupplier = tabSupplier;
        mFaviconSize = mResources.getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mIconGenerator = FaviconUtils.createCircularIconGenerator(mContext);
        mIsSeparateActivity = isSeparateActivity;
        mOpenUrlIntentCreator = openUrlIntentCreator;
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
        mCallbackController.destroy();
    }

    void startSearch(String query) {
        mToolbarModel.set(HistoryClustersToolbarProperties.QUERY_STATE, QueryState.forQuery(query));
    }

    void query(String query) {
        mPromise = mHistoryClustersBridge.queryClusters(query);
        mPromise.then(mCallbackController.makeCancelable(this::queryComplete));
    }

    void openHistoryClustersUi(String query) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        if (isTablet) {
            Tab currentTab = mTabSupplier.get();
            if (currentTab == null) return;
            Uri journeysUri =
                    new Uri.Builder()
                            .scheme(UrlConstants.CHROME_SCHEME)
                            .authority(UrlConstants.HISTORY_HOST)
                            .path(HistoryClustersConstants.JOURNEYS_PATH)
                            .appendQueryParameter(
                                    HistoryClustersConstants.HISTORY_CLUSTERS_QUERY_KEY, query)
                            .build();
            LoadUrlParams loadUrlParams = new LoadUrlParams(journeysUri.toString());
            currentTab.loadUrl(loadUrlParams);
            return;
        }

        Intent historyActivityIntent = mHistoryActivityIntentFactory.get();
        historyActivityIntent.putExtra(HistoryClustersConstants.EXTRA_SHOW_HISTORY_CLUSTERS, true);
        historyActivityIntent.putExtra(
                HistoryClustersConstants.EXTRA_HISTORY_CLUSTERS_QUERY, query);
        mContext.startActivity(historyActivityIntent);
    }

    private void queryComplete(HistoryClustersResult result) {
        for (HistoryCluster cluster : result.getClusters()) {
            for (ClusterVisit visit : cluster.getVisits()) {
                PropertyModel visitModel =
                        new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
                visitModel.set(HistoryClustersItemProperties.TITLE, visit.getTitle());
                visitModel.set(HistoryClustersItemProperties.URL, visit.getGURL().getHost());
                visitModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                        (v) -> navigateToItemUrl(visit.getGURL()));
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

    @VisibleForTesting
    void navigateToItemUrl(GURL gurl) {
        Context appContext = ContextUtils.getApplicationContext();
        if (mIsSeparateActivity) {
            appContext.startActivity(mOpenUrlIntentCreator.apply(gurl));
            return;
        }

        Tab currentTab = mTabSupplier.get();
        if (currentTab == null) return;

        LoadUrlParams loadUrlParams = new LoadUrlParams(gurl);
        currentTab.loadUrl(loadUrlParams);
    }
}
