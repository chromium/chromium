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
import org.chromium.chrome.browser.history_clusters.HistoryClusterView.ClusterViewAccessibilityState;
import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.MoreProgressButton.State;
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
import org.chromium.ui.util.AccessibilityUtil;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

class HistoryClustersMediator extends RecyclerView.OnScrollListener implements SearchDelegate {
    @VisibleForTesting

    // The number of items past the last visible one we want to have loaded at any give point.
    static final int REMAINING_ITEM_BUFFER_SIZE = 25;

    interface Clock {
        long currentTimeMillis();
    }

    private static class VisitMetadata {
        public final ListItem visitListItem;
        public final ListItem clusterListItem;
        public final List<ListItem> visitsAndRelatedSearches;

        private VisitMetadata(ListItem visitListItem, ListItem clusterListItem,
                List<ListItem> visitsAndRelatedSearches) {
            this.visitListItem = visitListItem;
            this.clusterListItem = clusterListItem;
            this.visitsAndRelatedSearches = visitsAndRelatedSearches;
        }
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
    private QueryState mQueryState;
    private final ListItem mMoreProgressItem;
    private final HistoryClustersMetricsLogger mMetricsLogger;
    private final Map<String, PropertyModel> mLabelToModelMap = new LinkedHashMap<>();
    private final Map<ClusterVisit, VisitMetadata> mVisitMetadataMap = new HashMap<>();
    private final AccessibilityUtil mAccessibilityUtil;
    private final boolean mIsScrollToLoadDisabled;

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
     * @param metricsLogger Object that records metrics about user interactions.
     * @param accessibilityUtil Utility object that tells us about the current accessibility state.
     */
    HistoryClustersMediator(@NonNull HistoryClustersBridge historyClustersBridge,
            LargeIconBridge largeIconBridge, @NonNull Context context, @NonNull Resources resources,
            @NonNull ModelList modelList, @NonNull PropertyModel toolbarModel,
            HistoryClustersDelegate historyClustersDelegate, Clock clock,
            TemplateUrlService templateUrlService, SelectionDelegate selectionDelegate,
            HistoryClustersMetricsLogger metricsLogger, AccessibilityUtil accessibilityUtil) {
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
        mMetricsLogger = metricsLogger;
        mAccessibilityUtil = accessibilityUtil;

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

        mIsScrollToLoadDisabled = mAccessibilityUtil.isAccessibilityEnabled()
                || AccessibilityUtil.isHardwareKeyboardAttached(mResources.getConfiguration());
        @State
        int buttonState = mIsScrollToLoadDisabled ? State.BUTTON : State.LOADING;
        PropertyModel moreProgressModel =
                new PropertyModel.Builder(HistoryClustersItemProperties.ALL_KEYS)
                        .with(HistoryClustersItemProperties.PROGRESS_BUTTON_STATE, buttonState)
                        .with(HistoryClustersItemProperties.CLICK_HANDLER,
                                (v) -> mPromise.then(this::continueQuery))
                        .build();
        mMoreProgressItem = new ListItem(ItemType.MORE_PROGRESS, moreProgressModel);
    }

    // SearchDelegate implementation.
    @Override
    public void onSearchTextChanged(String query) {
        resetModel();
        startQuery(query);
    }

    @Override
    public void onEndSearch() {
        setQueryState(QueryState.forQueryless());
    }

    // OnScrollListener implementation
    @Override
    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
        if (mIsScrollToLoadDisabled) return;
        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        if (layoutManager.findLastVisibleItemPosition()
                > (mModelList.size() - REMAINING_ITEM_BUFFER_SIZE)) {
            mPromise.then(this::continueQuery);
        }
    }

    void destroy() {
        mLargeIconBridge.destroy();
        mCallbackController.destroy();
    }

    void setQueryState(QueryState queryState) {
        if (mQueryState != null && !mQueryState.isSearching() && !queryState.isSearching()) {
            return;
        }

        mQueryState = queryState;
        mToolbarModel.set(HistoryClustersToolbarProperties.QUERY_STATE, queryState);
        if (!queryState.isSearching()) {
            resetModel();
            startQuery(mQueryState.getQuery());
        }
    }

    @VisibleForTesting
    void startQuery(String query) {
        if (mQueryState.isSearching()) {
            mMetricsLogger.incrementQueryCount();
        }

        mPromise = mHistoryClustersBridge.queryClusters(query);
        mPromise.then(mCallbackController.makeCancelable(this::queryComplete));
        ensureFooter(State.LOADING, true);
    }

    void continueQuery(HistoryClustersResult previousResult) {
        if (!previousResult.canLoadMore()) return;
        mPromise = mHistoryClustersBridge.loadMoreClusters(previousResult.getQuery());
        mPromise.then(mCallbackController.makeCancelable(this::queryComplete));
        ensureFooter(State.LOADING, true);
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

    void onRelatedSearchesChipClicked(String searchQuery, int index) {
        if (!mTemplateUrlService.isLoaded()) {
            return;
        }

        mMetricsLogger.recordRelatedSearchesClick(index);
        navigateToUrlInCurrentTab(
                new GURL(mTemplateUrlService.getUrlForSearchQuery(searchQuery)), false);
    }

    void openVisitsInNewTabs(List<ClusterVisit> visits, boolean isIncognito, boolean inTabGroup) {
        for (ClusterVisit visit : visits) {
            mMetricsLogger.recordVisitAction(
                    HistoryClustersMetricsLogger.VisitAction.CLICKED, visit);
        }

        if (mDelegate.isSeparateActivity()) {
            ArrayList<String> additionalUrls = new ArrayList<>(visits.size() - 1);
            for (int i = 1; i < visits.size(); i++) {
                additionalUrls.add(visits.get(i).getNormalizedUrl().getSpec());
            }

            Intent intent = mDelegate.getOpenUrlIntent(visits.get(0).getNormalizedUrl(),
                    isIncognito, true, inTabGroup, additionalUrls);
            ContextUtils.getApplicationContext().startActivity(intent);
        } else {
            Tab parent = createNewTab(visits.get(0).getNormalizedUrl(), isIncognito, null);
            for (int i = 1; i < visits.size(); i++) {
                createNewTab(visits.get(i).getNormalizedUrl(), false, parent);
            }
        }
    }

    void navigateToUrlInCurrentTab(GURL gurl, boolean inIncognito) {
        Context appContext = ContextUtils.getApplicationContext();
        if (mDelegate.isSeparateActivity()) {
            appContext.startActivity(
                    mDelegate.getOpenUrlIntent(gurl, inIncognito, false, false, null));
            return;
        }

        Tab currentTab = mDelegate.getTab();
        if (currentTab == null) return;
        LoadUrlParams loadUrlParams = new LoadUrlParams(gurl);
        currentTab.loadUrl(loadUrlParams);
    }

    void deleteVisits(List<ClusterVisit> visits) {
        for (int i = 0; i < visits.size(); i++) {
            ClusterVisit visit = visits.get(i);
            mDelegate.markVisitForRemoval(visit);
            mMetricsLogger.recordVisitAction(
                    HistoryClustersMetricsLogger.VisitAction.DELETED, visit);
            removeVisit(visit);
        }

        mDelegate.removeMarkedItems();
    }

    private void removeVisit(ClusterVisit visit) {
        VisitMetadata visitMetadata = mVisitMetadataMap.get(visit);
        if (visitMetadata == null) return;
        ListItem visitListItem = visitMetadata.visitListItem;
        assert mModelList.indexOf(visitListItem) != -1
                && visitMetadata.visitsAndRelatedSearches.indexOf(visitListItem) != -1;
        mModelList.remove(visitListItem);
        visitMetadata.visitsAndRelatedSearches.remove(visitListItem);
        if (visitMetadata.visitsAndRelatedSearches.size() == 1
                && visitMetadata.visitsAndRelatedSearches.get(0).type
                        == ItemType.RELATED_SEARCHES) {
            mModelList.remove(visitMetadata.visitsAndRelatedSearches.get(0));
            visitMetadata.visitsAndRelatedSearches.clear();
        }

        if (visitMetadata.visitsAndRelatedSearches.isEmpty()) {
            mModelList.remove(visitMetadata.clusterListItem);
        }

        mVisitMetadataMap.remove(visit);
    }

    private Tab createNewTab(GURL gurl, boolean incognito, Tab parentTab) {
        TabCreator tabCreator = mDelegate.getTabCreator(incognito);
        assert tabCreator != null;
        return tabCreator.createNewTab(
                new LoadUrlParams(gurl), TabLaunchType.FROM_CHROME_UI, parentTab);
    }

    private void queryComplete(HistoryClustersResult result) {
        boolean isQueryLess = !mQueryState.isSearching();
        if (isQueryLess) {
            ensureHeaders();
            for (Map.Entry<String, Integer> entry : result.getLabelCounts().entrySet()) {
                // Check if label exists in the model already
                // If not, create a new entry
                String rawLabel = entry.getKey();
                PropertyModel existingModel = mLabelToModelMap.get(rawLabel);
                if (existingModel == null) {
                    existingModel = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
                    mLabelToModelMap.put(rawLabel, existingModel);
                    Drawable journeysDrawable =
                            AppCompatResources.getDrawable(mContext, R.drawable.ic_journeys);
                    existingModel.set(
                            HistoryClustersItemProperties.ICON_DRAWABLE, journeysDrawable);
                    existingModel.set(HistoryClustersItemProperties.DIVIDER_VISIBLE, true);
                    existingModel.set(HistoryClustersItemProperties.TITLE,
                            getQuotedLabelFromRawLabel(rawLabel, result.getClusters()));
                    ListItem clusterItem = new ListItem(ItemType.CLUSTER, existingModel);
                    mModelList.add(clusterItem);
                    existingModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                            (v)
                                    -> setQueryState(QueryState.forQuery(
                                            rawLabel, mDelegate.getSearchEmptyString())));
                    existingModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, null);
                    existingModel.set(HistoryClustersItemProperties.ACCESSIBILITY_STATE,
                            ClusterViewAccessibilityState.CLICKABLE);
                }
                existingModel.set(HistoryClustersItemProperties.LABEL,
                        mResources.getQuantityString(R.plurals.history_clusters_n_matches,
                                entry.getValue(), entry.getValue()));
            }

            if (!mIsScrollToLoadDisabled && result.canLoadMore() && !result.isContinuation()) {
                continueQuery(result);
            }

            ensureFooter(State.BUTTON, result.canLoadMore());

            return;
        }

        for (HistoryCluster cluster : result.getClusters()) {
            PropertyModel clusterModel = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
            clusterModel.set(HistoryClustersItemProperties.TITLE,
                    applyBolding(cluster.getLabel(), cluster.getMatchPositions()));
            Drawable journeysDrawable =
                    AppCompatResources.getDrawable(mContext, R.drawable.ic_journeys);
            clusterModel.set(HistoryClustersItemProperties.ICON_DRAWABLE, journeysDrawable);
            clusterModel.set(HistoryClustersItemProperties.DIVIDER_VISIBLE, isQueryLess);
            clusterModel.set(HistoryClustersItemProperties.ACCESSIBILITY_STATE,
                    ClusterViewAccessibilityState.COLLAPSIBLE);
            ListItem clusterItem = new ListItem(ItemType.CLUSTER, clusterModel);
            mModelList.add(clusterItem);

            List<ListItem> visitsAndRelatedSearches =
                    new ArrayList<>(cluster.getVisits().size() + 1);
            ListItem relatedSearchesItem = null;
            List<String> relatedSearches = cluster.getRelatedSearches();
            if (!relatedSearches.isEmpty()) {
                PropertyModel relatedSearchesModel =
                        new PropertyModel.Builder(HistoryClustersItemProperties.ALL_KEYS)
                                .with(HistoryClustersItemProperties.RELATED_SEARCHES,
                                        relatedSearches)
                                .with(HistoryClustersItemProperties.CHIP_CLICK_HANDLER,
                                        (query)
                                                -> onRelatedSearchesChipClicked(
                                                        query, relatedSearches.indexOf(query)))
                                .build();
                relatedSearchesItem = new ListItem(ItemType.RELATED_SEARCHES, relatedSearchesModel);
            }

            for (int i = 0; i < cluster.getVisits().size(); i++) {
                ClusterVisit visit = cluster.getVisits().get(i);
                PropertyModel visitModel =
                        new PropertyModel.Builder(HistoryClustersItemProperties.ALL_KEYS)
                                .with(HistoryClustersItemProperties.TITLE,
                                        new SpannableString(applyBolding(
                                                visit.getTitle(), visit.getTitleMatchPositions())))
                                .with(HistoryClustersItemProperties.URL,
                                        applyBolding(visit.getUrlForDisplay(),
                                                visit.getUrlMatchPositions()))
                                .with(HistoryClustersItemProperties.CLICK_HANDLER,
                                        (v) -> onClusterVisitClicked((SelectableItemView) v, visit))
                                .with(HistoryClustersItemProperties.CLUSTER_VISIT, visit)
                                .with(HistoryClustersItemProperties.VISIBILITY, View.VISIBLE)
                                .with(HistoryClustersItemProperties.END_BUTTON_CLICK_HANDLER,
                                        (v) -> deleteVisits(Arrays.asList(visit)))
                                .build();
                if (mLargeIconBridge != null) {
                    mLargeIconBridge.getLargeIconForUrl(visit.getNormalizedUrl(), mFaviconSize,
                            (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
                                    int iconType) -> {
                                Drawable drawable = FaviconUtils.getIconDrawableWithoutFilter(icon,
                                        visit.getNormalizedUrl(), fallbackColor, mIconGenerator,
                                        mResources, mFaviconSize);
                                visitModel.set(
                                        HistoryClustersItemProperties.ICON_DRAWABLE, drawable);
                            });
                }

                ListItem listItem = new ListItem(ItemType.VISIT, visitModel);
                mVisitMetadataMap.put(
                        visit, new VisitMetadata(listItem, clusterItem, visitsAndRelatedSearches));
                visitsAndRelatedSearches.add(listItem);
            }

            if (relatedSearchesItem != null) {
                visitsAndRelatedSearches.add(relatedSearchesItem);
            }

            mModelList.addAll(visitsAndRelatedSearches);
            clusterModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                    v -> hideCluster(clusterItem, visitsAndRelatedSearches));
            Drawable chevron = UiUtils.getTintedDrawable(mContext,
                    R.drawable.ic_expand_more_black_24dp, R.color.default_icon_color_tint_list);
            clusterModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, chevron);
            clusterModel.set(
                    HistoryClustersItemProperties.LABEL, getTimeString(cluster.getTimestamp()));
        }

        ensureFooter(State.BUTTON, result.canLoadMore());
    }

    private void resetModel() {
        mModelList.clear();
        mLabelToModelMap.clear();
        mVisitMetadataMap.clear();
    }

    private String getQuotedLabelFromRawLabel(String rawLabel, List<HistoryCluster> clusters) {
        for (HistoryCluster cluster : clusters) {
            if (cluster.getRawLabel().equals(rawLabel)) {
                return cluster.getLabel();
            }
        }

        // This shouldn't happen, but the unquoted label is a graceful fallback in case it does.
        return rawLabel;
    }

    private void ensureHeaders() {
        if (mQueryState != null && mQueryState.isSearching()) {
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

    private void ensureFooter(@State int buttonState, boolean canLoadMore) {
        mMoreProgressItem.model.set(
                HistoryClustersItemProperties.PROGRESS_BUTTON_STATE, buttonState);
        boolean shouldShow = (buttonState == State.BUTTON && canLoadMore && mIsScrollToLoadDisabled)
                || buttonState == State.LOADING;
        int currentIndex = mModelList.indexOf(mMoreProgressItem);
        boolean showing = currentIndex != -1;
        if (showing) {
            mModelList.remove(mMoreProgressItem);
        }

        if (shouldShow) {
            mModelList.add(mMoreProgressItem);
        }
    }

    @VisibleForTesting
    void onClusterVisitClicked(SelectableItemView view, ClusterVisit clusterVisit) {
        if (mSelectionDelegate.isSelectionEnabled()) {
            view.onLongClick(view);
        } else {
            mMetricsLogger.recordVisitAction(
                    HistoryClustersMetricsLogger.VisitAction.CLICKED, clusterVisit);
            navigateToUrlInCurrentTab(clusterVisit.getNormalizedUrl(), false);
        }
    }

    @VisibleForTesting
    void hideCluster(ListItem clusterItem, List<ListItem> itemsToHide) {
        int indexOfFirstVisit = mModelList.indexOf(itemsToHide.get(0));
        PropertyModel clusterModel = clusterItem.model;
        clusterModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                (v) -> showCluster(clusterItem, itemsToHide));
        Drawable chevron = UiUtils.getTintedDrawable(mContext, R.drawable.ic_expand_less_black_24dp,
                R.color.default_icon_color_tint_list);
        clusterModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, chevron);
        clusterModel.set(HistoryClustersItemProperties.ACCESSIBILITY_STATE,
                ClusterViewAccessibilityState.EXPANDABLE);

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
    void showCluster(ListItem clusterItem, List<ListItem> itemsToShow) {
        PropertyModel clusterModel = clusterItem.model;
        clusterModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                (v) -> hideCluster(clusterItem, itemsToShow));
        Drawable chevron = UiUtils.getTintedDrawable(mContext, R.drawable.ic_expand_more_black_24dp,
                R.color.default_icon_color_tint_list);
        clusterModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, chevron);
        clusterModel.set(HistoryClustersItemProperties.ACCESSIBILITY_STATE,
                ClusterViewAccessibilityState.COLLAPSIBLE);
        int insertionIndex = mModelList.indexOf(clusterItem) + 1;
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
