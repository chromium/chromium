// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.chrome.browser.history_clusters.HistoryCluster.MatchPosition;
import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

class HistoryClustersMediator extends RecyclerView.OnScrollListener implements SearchDelegate {
    @VisibleForTesting

    // The number of items past the last visible one we want to have loaded at any give point.
    static final int REMAINING_ITEM_BUFFER_SIZE = 25;

    interface Clock {
        long currentTimeMillis();
    }

    private final HistoryClustersBridge mHistoryClustersBridge;
    private final Context mContext;
    private final Resources mResources;
    private final ModelList mModelList;
    private final PropertyModel mToolbarModel;
    private final RoundedIconGenerator mIconGenerator;
    private final LargeIconBridge mLargeIconBridge;
    private final int mFaviconSize;
    private Promise<HistoryClustersResult> mPromise;
    private final HistoryClustersDelegate mDelegate;
    private final CallbackController mCallbackController = new CallbackController();
    private final Clock mClock;
    private final TemplateUrlService mTemplateUrlService;
    private final SelectionDelegate mSelectionDelegate;
    private ListItem mToggleItem;
    private ListItem mPrivacyDisclaimerItem;
    private ListItem mClearBrowsingDataItem;
    private QueryState mQueryState = QueryState.forQueryless();

    /**
     * Create a new HistoryClustersMediator.
     * @param historyClustersBridge Provider of history clusters data.
     * @param largeIconBridge Bridge for fetching site icons.
     * @param context Android context from which UI configuration should be derived.
     * @param resources Android resources object from which strings, colors etc. should be fetched.
     * @param modelList Model list to which fetched cluster data should be pushed to.
     * @param toolbarModel Model for properties affecting the "full page" toolbar shown in the
     *         history activity.
     * @param historyClustersDelegate Delegate that provides functionality that must be implemented
     *         externally, e.g. populating intents targeting activities we can't reference directly.
     * @param clock Provider of the current time in ms relative to the unix epoch.
     * @param templateUrlService Service that allows us to generate a URL for a given search query.
     * @param selectionDelegate Delegate that gives us information about the currently selected
     *         items in the list we're displaying.
     */
    HistoryClustersMediator(@NonNull HistoryClustersBridge historyClustersBridge,
            LargeIconBridge largeIconBridge, @NonNull Context context, @NonNull Resources resources,
            @NonNull ModelList modelList, @NonNull PropertyModel toolbarModel,
            HistoryClustersDelegate historyClustersDelegate, Clock clock,
            TemplateUrlService templateUrlService, SelectionDelegate selectionDelegate) {
        mHistoryClustersBridge = historyClustersBridge;
        mLargeIconBridge = largeIconBridge;
        mModelList = modelList;
        mContext = context;
        mResources = resources;
        mToolbarModel = toolbarModel;
        mDelegate = historyClustersDelegate;
        mFaviconSize = mResources.getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mIconGenerator = FaviconUtils.createCircularIconGenerator(mContext);
        mClock = clock;
        mTemplateUrlService = templateUrlService;
        mSelectionDelegate = selectionDelegate;

        PropertyModel toggleModel = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        mToggleItem = new ListItem(ItemType.TOGGLE, toggleModel);

        PropertyModel privacyDisclaimerModel =
                new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        mPrivacyDisclaimerItem = new ListItem(ItemType.PRIVACY_DISCLAIMER, privacyDisclaimerModel);
        mDelegate.shouldShowPrivacyDisclaimerSupplier().addObserver(show -> ensureHeaders());

        PropertyModel clearBrowsingDataModel =
                new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        mClearBrowsingDataItem = new ListItem(ItemType.CLEAR_BROWSING_DATA, clearBrowsingDataModel);
        mDelegate.shouldShowClearBrowsingDataSupplier().addObserver(show -> ensureHeaders());
    }

    // SearchDelegate implementation.
    @Override
    public void onSearchTextChanged(String query) {
        mModelList.clear();
        startQuery(query);
    }

    @Override
    public void onEndSearch() {
        setQueryState(QueryState.forQueryless());
    }

    // OnScrollListener implementation
    @Override
    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        if (layoutManager.findLastVisibleItemPosition()
                > (mModelList.size() - REMAINING_ITEM_BUFFER_SIZE)) {
            mPromise.then(result -> {
                if (result.canLoadMore()) {
                    continueQuery(result.getQuery());
                }
            });
        }
    }

    void destroy() {
        mLargeIconBridge.destroy();
        mCallbackController.destroy();
    }

    void setQueryState(QueryState queryState) {
        mQueryState = queryState;
        mToolbarModel.set(HistoryClustersToolbarProperties.QUERY_STATE, queryState);
        if (!queryState.isSearching()) {
            mModelList.clear();
            startQuery("");
        }
    }

    @VisibleForTesting
    void startQuery(String query) {
        mPromise = mHistoryClustersBridge.queryClusters(query);
        mPromise.then(mCallbackController.makeCancelable(this::queryComplete));
    }

    void continueQuery(String query) {
        mPromise = mHistoryClustersBridge.loadMoreClusters(query);
        mPromise.then(mCallbackController.makeCancelable(this::queryComplete));
    }

    void openHistoryClustersUi(String query) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        if (isTablet) {
            Tab currentTab = mDelegate.getTab();
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

        Intent historyActivityIntent = mDelegate.getHistoryActivityIntent();
        historyActivityIntent.putExtra(HistoryClustersConstants.EXTRA_SHOW_HISTORY_CLUSTERS, true);
        historyActivityIntent.putExtra(
                HistoryClustersConstants.EXTRA_HISTORY_CLUSTERS_QUERY, query);
        mContext.startActivity(historyActivityIntent);
    }

    void onRelatedSearchesChipClicked(String searchQuery) {
        if (!mTemplateUrlService.isLoaded()) {
            return;
        }

        navigateToUrl(
                new GURL(mTemplateUrlService.getUrlForSearchQuery(searchQuery)), false, false);
    }

    void navigateToUrl(GURL gurl, boolean inIncognito, boolean createNewTab) {
        Context appContext = ContextUtils.getApplicationContext();
        if (mDelegate.isSeparateActivity()) {
            appContext.startActivity(mDelegate.getOpenUrlIntent(gurl, inIncognito, createNewTab));
            return;
        }

        Tab currentTab = mDelegate.getTab();
        if (currentTab == null) return;

        if (createNewTab) {
            TabCreator tabCreator = mDelegate.getTabCreator(currentTab.isIncognito());
            assert tabCreator != null;
            tabCreator.createNewTab(
                    new LoadUrlParams(gurl), TabLaunchType.FROM_CHROME_UI, currentTab);
        } else {
            LoadUrlParams loadUrlParams = new LoadUrlParams(gurl);
            currentTab.loadUrl(loadUrlParams);
        }
    }

    private void queryComplete(HistoryClustersResult result) {
        boolean isQueryLess = !mQueryState.isSearching();
        if (isQueryLess) {
            ensureHeaders();
        }

        for (HistoryCluster cluster : result.getClusters()) {
            PropertyModel clusterModel = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
            clusterModel.set(HistoryClustersItemProperties.TITLE,
                    applyBolding(cluster.getLabel(), cluster.getMatchPositions()));
            Drawable journeysDrawable =
                    AppCompatResources.getDrawable(mContext, R.drawable.ic_journeys);
            clusterModel.set(HistoryClustersItemProperties.ICON_DRAWABLE, journeysDrawable);
            ListItem clusterItem = new ListItem(ItemType.CLUSTER, clusterModel);
            mModelList.add(clusterItem);
            if (isQueryLess) {
                clusterModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                        (v) -> setQueryState(QueryState.forQuery(cluster.getLabel())));
                clusterModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, null);
                clusterModel.set(HistoryClustersItemProperties.LABEL, null);
                continue;
            }

            List<ListItem> visitsAndRelatedSearches =
                    new ArrayList<>(cluster.getVisits().size() + 1);
            for (ClusterVisit visit : cluster.getVisits()) {
                PropertyModel visitModel =
                        new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
                visitModel.set(HistoryClustersItemProperties.TITLE,
                        new SpannableString(
                                applyBolding(visit.getTitle(), visit.getTitleMatchPositions())));
                visitModel.set(HistoryClustersItemProperties.URL,
                        applyBolding(visit.getUrlForDisplay(), visit.getUrlMatchPositions()));
                visitModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                        (v) -> onClusterVisitClicked((SelectableItemView) v, visit));
                visitModel.set(HistoryClustersItemProperties.CLUSTER_VISIT, visit);
                visitModel.set(HistoryClustersItemProperties.VISIBILITY, View.VISIBLE);
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

                visitsAndRelatedSearches.add(new ListItem(ItemType.VISIT, visitModel));
            }

            List<String> relatedSearches = cluster.getRelatedSearches();
            if (!relatedSearches.isEmpty()) {
                PropertyModel relatedSearchesModel =
                        new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
                relatedSearchesModel.set(
                        HistoryClustersItemProperties.RELATED_SEARCHES, relatedSearches);
                relatedSearchesModel.set(HistoryClustersItemProperties.CHIP_CLICK_HANDLER,
                        this::onRelatedSearchesChipClicked);
                ListItem relatedSearchesItem =
                        new ListItem(ItemType.RELATED_SEARCHES, relatedSearchesModel);
                visitsAndRelatedSearches.add(relatedSearchesItem);
            }

            mModelList.addAll(visitsAndRelatedSearches);

            clusterModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                    v -> hideCluster(clusterModel, visitsAndRelatedSearches));
            Drawable chevron = UiUtils.getTintedDrawable(mContext,
                    R.drawable.ic_expand_more_black_24dp, R.color.default_icon_color_tint_list);
            clusterModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, chevron);
            clusterModel.set(
                    HistoryClustersItemProperties.LABEL, getTimeString(cluster.getTimestamp()));
        }
    }

    private void ensureHeaders() {
        if (mQueryState.isSearching()) {
            return;
        }

        int position = 0;
        boolean hasPrivacyDisclaimer = mModelList.indexOf(mPrivacyDisclaimerItem) > -1;
        boolean hasClearBrowsingData = mModelList.indexOf(mClearBrowsingDataItem) > -1;
        boolean hasToggleItem = mModelList.indexOf(mToggleItem) > -1;

        boolean shouldShowPrivacyDisclaimer =
                Boolean.TRUE.equals(mDelegate.shouldShowPrivacyDisclaimerSupplier().get());
        if (shouldShowPrivacyDisclaimer && !hasPrivacyDisclaimer) {
            mModelList.add(position++, mPrivacyDisclaimerItem);
        } else if (!shouldShowPrivacyDisclaimer && hasPrivacyDisclaimer) {
            mModelList.remove(mPrivacyDisclaimerItem);
        }

        boolean shouldShowClearBrowsingData =
                Boolean.TRUE.equals(mDelegate.shouldShowClearBrowsingDataSupplier().get());
        if (shouldShowClearBrowsingData && !hasClearBrowsingData) {
            mModelList.add(position++, mClearBrowsingDataItem);
        } else if (!shouldShowClearBrowsingData && hasClearBrowsingData) {
            mModelList.remove(mClearBrowsingDataItem);
        }

        if (!hasToggleItem) {
            mModelList.add(position++, mToggleItem);
        }
    }

    private void onClusterVisitClicked(SelectableItemView view, ClusterVisit clusterVisit) {
        if (mSelectionDelegate.isSelectionEnabled()) {
            view.onLongClick(view);
        } else {
            navigateToUrl(clusterVisit.getGURL(), false, false);
        }
    }

    @VisibleForTesting
    void hideCluster(PropertyModel clusterModel, List<ListItem> itemsToHide) {
        int indexOfFirstVisit = mModelList.indexOf(itemsToHide.get(0));
        clusterModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                (v) -> showCluster(clusterModel, itemsToHide, indexOfFirstVisit));
        Drawable chevron = UiUtils.getTintedDrawable(mContext, R.drawable.ic_expand_less_black_24dp,
                R.color.default_icon_color_tint_list);
        clusterModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, chevron);

        mModelList.removeRange(indexOfFirstVisit, itemsToHide.size());
        for (ListItem listItem : itemsToHide) {
            ClusterVisit clusterVisit =
                    listItem.model.get(HistoryClustersItemProperties.CLUSTER_VISIT);
            if (mSelectionDelegate.isItemSelected(clusterVisit)) {
                mSelectionDelegate.toggleSelectionForItem(clusterVisit);
            }
        }
    }

    @VisibleForTesting
    void showCluster(PropertyModel clusterModel, List<ListItem> itemsToShow, int insertionIndex) {
        clusterModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                (v) -> hideCluster(clusterModel, itemsToShow));
        Drawable chevron = UiUtils.getTintedDrawable(mContext, R.drawable.ic_expand_more_black_24dp,
                R.color.default_icon_color_tint_list);
        clusterModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, chevron);
        mModelList.addAll(itemsToShow, insertionIndex);
    }

    @VisibleForTesting
    String getTimeString(long timestampMillis) {
        long timeDeltaMs = mClock.currentTimeMillis() - timestampMillis;
        if (timeDeltaMs < 0) timeDeltaMs = 0;

        int daysElapsed = (int) TimeUnit.MILLISECONDS.toDays(timeDeltaMs);
        int hoursElapsed = (int) TimeUnit.MILLISECONDS.toHours(timeDeltaMs);
        int minutesElapsed = (int) TimeUnit.MILLISECONDS.toMinutes(timeDeltaMs);

        if (daysElapsed > 0) {
            return mResources.getQuantityString(R.plurals.n_days_ago, daysElapsed, daysElapsed);
        } else if (hoursElapsed > 0) {
            return mResources.getQuantityString(R.plurals.n_hours_ago, hoursElapsed, hoursElapsed);
        } else if (minutesElapsed > 0) {
            return mResources.getQuantityString(
                    R.plurals.n_minutes_ago, minutesElapsed, minutesElapsed);
        } else {
            return mResources.getString(R.string.just_now);
        }
    }

    @VisibleForTesting
    SpannableString applyBolding(String text, List<MatchPosition> matchPositions) {
        SpannableString spannableString = new SpannableString(text);
        for (MatchPosition matchPosition : matchPositions) {
            spannableString.setSpan(new StyleSpan(Typeface.BOLD), matchPosition.mMatchStart,
                    matchPosition.mMatchEnd, 0);
        }

        return spannableString;
    }
}
