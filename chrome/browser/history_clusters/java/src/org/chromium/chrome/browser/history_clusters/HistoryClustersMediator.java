// Copyright 2022 The Chromium Authors
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
import android.os.Handler;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.lifetime.DestroyChecker;
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
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
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
    static final int MIN_EXPANDED_CLUSTER_SIZE = 2;
    static final long QUERY_DELAY_MS = 60;

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
    private final int mMinFaviconSize;
    private final int mDisplayedFaviconSize;
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
    private final ListItem mEmptyTextListItem;
    private final HistoryClustersMetricsLogger mMetricsLogger;
    private final Map<String, PropertyModel> mLabelToModelMap = new LinkedHashMap<>();
    private final Map<ClusterVisit, VisitMetadata> mVisitMetadataMap = new HashMap<>();
    private final Callback<String> mAnnounceForAccessibilityCallback;
    private final Handler mHandler;
    private final DestroyChecker mDestroyChecker = new DestroyChecker();
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
     * @param announceForAccessibilityCallback Callback that announces the given string for a11y.
     * @param handler Handler object on which deferred tasks can be posted.
     */
    HistoryClustersMediator(@NonNull HistoryClustersBridge historyClustersBridge,
            LargeIconBridge largeIconBridge, @NonNull Context context, @NonNull Resources resources,
            @NonNull ModelList modelList, @NonNull PropertyModel toolbarModel,
            HistoryClustersDelegate historyClustersDelegate, Clock clock,
            TemplateUrlService templateUrlService, SelectionDelegate selectionDelegate,
            HistoryClustersMetricsLogger metricsLogger,
            Callback<String> announceForAccessibilityCallback, Handler handler) {
        mHistoryClustersBridge = historyClustersBridge;
        mLargeIconBridge = largeIconBridge;
        mModelList = modelList;
        mContext = context;
        mResources = resources;
        mToolbarModel = toolbarModel;
        mDelegate = historyClustersDelegate;
        mMinFaviconSize = mResources.getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mDisplayedFaviconSize = mResources.getDimensionPixelSize(R.dimen.default_favicon_size);
        mIconGenerator = FaviconUtils.createCircularIconGenerator(mContext);
        mClock = clock;
        mTemplateUrlService = templateUrlService;
        mSelectionDelegate = selectionDelegate;
        mMetricsLogger = metricsLogger;
        mAnnounceForAccessibilityCallback = announceForAccessibilityCallback;
        mHandler = handler;

        mSelectionDelegate.addObserver(
                (selectedItems -> setSelectionActive(mSelectionDelegate.isSelectionEnabled())));

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

        mIsScrollToLoadDisabled = AccessibilityState.isTouchExplorationEnabled()
                || AccessibilityState.isPerformGesturesEnabled()
                || UiUtils.isHardwareKeyboardAttached();
        @State
        int buttonState = mIsScrollToLoadDisabled ? State.BUTTON : State.LOADING;
        PropertyModel moreProgressModel =
                new PropertyModel.Builder(HistoryClustersItemProperties.ALL_KEYS)
                        .with(HistoryClustersItemProperties.PROGRESS_BUTTON_STATE, buttonState)
                        .with(HistoryClustersItemProperties.CLICK_HANDLER,
                                (v)
                                        -> mPromise.then(mCallbackController.makeCancelable(
                                                                 this::continueQuery),
                                                this::onPromiseRejected))
                        .build();
        mMoreProgressItem = new ListItem(ItemType.MORE_PROGRESS, moreProgressModel);
        mEmptyTextListItem = new ListItem(ItemType.EMPTY_TEXT, new PropertyModel());
    }

    // SearchDelegate implementation.
    @Override
    public void onSearchTextChanged(String query) {
        mHandler.removeCallbacksAndMessages(null);
        mHandler.postDelayed(()
                                     -> setQueryState(QueryState.forQuery(
                                             query, mDelegate.getSearchEmptyString())),
                QUERY_DELAY_MS);
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
            mPromise.then(mCallbackController.makeCancelable(this::continueQuery),
                    this::onPromiseRejected);
        }
    }

    void destroy() {
        mHandler.removeCallbacksAndMessages(null);
        mLargeIconBridge.destroy();
        mCallbackController.destroy();
        mDestroyChecker.destroy();
    }

    void setQueryState(QueryState queryState) {
        if (mQueryState != null && !mQueryState.isSearching() && !queryState.isSearching()) {
            return;
        }

        mQueryState = queryState;
        mToolbarModel.set(HistoryClustersToolbarProperties.QUERY_STATE, queryState);
        resetModel();
        startQuery(mQueryState.getQuery());
    }

    @VisibleForTesting
    void startQuery(String query) {
        mDestroyChecker.checkNotDestroyed();
        if (mQueryState.isSearching()) {
            mMetricsLogger.incrementQueryCount();
        }

        if (mPromise != null && !mPromise.isFulfilled()) {
            mPromise.reject();
        }

        mPromise = mHistoryClustersBridge.queryClusters(query);
        mPromise.then(
                mCallbackController.makeCancelable(this::queryComplete), this::onPromiseRejected);
        ensureFooters(State.LOADING, true, null);
    }

    void continueQuery(HistoryClustersResult previousResult) {
        mDestroyChecker.checkNotDestroyed();
        if (!previousResult.canLoadMore()) return;
        mPromise = mHistoryClustersBridge.loadMoreClusters(previousResult.getQuery());
        mPromise.then(
                mCallbackController.makeCancelable(this::queryComplete), this::onPromiseRejected);
        ensureFooters(State.LOADING, true, null);
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
                            .path(mDelegate.isRenameEnabled()
                                            ? HistoryClustersConstants.GROUPS_PATH
                                            : HistoryClustersConstants.JOURNEYS_PATH)
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
            Tab parent = createNewTab(visits.get(0).getNormalizedUrl(), isIncognito, null,
                    TabLaunchType.FROM_CHROME_UI);
            @TabLaunchType
            int tabLaunchType = inTabGroup ? TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP
                                           : TabLaunchType.FROM_CHROME_UI;
            for (int i = 1; i < visits.size(); i++) {
                createNewTab(visits.get(i).getNormalizedUrl(), isIncognito, parent, tabLaunchType);
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

        if (visits.size() == 1) {
            announceForAccessibility(
                    mResources.getString(R.string.delete_message, visits.get(0).getTitle()));
        } else {
            announceForAccessibility(
                    mResources.getString(R.string.multiple_history_items_deleted, visits.size()));
        }

        mDelegate.removeMarkedItems();
    }

    void setSelectionActive(boolean active) {
        boolean showDeleteButton = !active;
        for (Map.Entry<ClusterVisit, VisitMetadata> entry : mVisitMetadataMap.entrySet()) {
            entry.getValue().visitListItem.model.set(
                    HistoryClustersItemProperties.END_BUTTON_VISIBLE, showDeleteButton);
        }
    }

    void onHistoryDeletedExternally() {
        if (mQueryState == null) return;
        mSelectionDelegate.clearSelection();
        resetModel();
        startQuery(mQueryState.getQuery());
    }

    private void removeVisit(ClusterVisit visit) {
        VisitMetadata visitMetadata = mVisitMetadataMap.get(visit);
        if (visitMetadata == null) return;
        ListItem visitListItem = visitMetadata.visitListItem;
        List<ListItem> visitsAndRelatedSearches = visitMetadata.visitsAndRelatedSearches;
        boolean deletedModelHadDivider =
                visitListItem.model.get(HistoryClustersItemProperties.DIVIDER_VISIBLE);
        assert mModelList.indexOf(visitListItem) != -1
                && visitsAndRelatedSearches.indexOf(visitListItem) != -1;
        mModelList.remove(visitListItem);
        visitsAndRelatedSearches.remove(visitListItem);
        if (visitsAndRelatedSearches.size() == 1
                && visitsAndRelatedSearches.get(0).type == ItemType.RELATED_SEARCHES) {
            mModelList.remove(visitsAndRelatedSearches.get(0));
            visitsAndRelatedSearches.clear();
        }

        if (visitsAndRelatedSearches.isEmpty()) {
            mModelList.remove(visitMetadata.clusterListItem);
        } else if (deletedModelHadDivider) {
            PropertyModel modelOfNewLastVisit =
                    visitsAndRelatedSearches.get(visitsAndRelatedSearches.size() - 1).model;
            modelOfNewLastVisit.set(HistoryClustersItemProperties.DIVIDER_VISIBLE, true);
            modelOfNewLastVisit.set(HistoryClustersItemProperties.DIVIDER_IS_THICK, true);
        }

        mVisitMetadataMap.remove(visit);
    }

    private Tab createNewTab(GURL gurl, boolean incognito, Tab parentTab, int tabLaunchType) {
        TabCreator tabCreator = mDelegate.getTabCreator(incognito);
        assert tabCreator != null;
        return tabCreator.createNewTab(new LoadUrlParams(gurl), tabLaunchType, parentTab);
    }

    private void queryComplete(HistoryClustersResult result) {
        if (result.isContinuation() && result.getClusters().size() > 0) {
            setDividerVisibilityForLastItem(true);
        }
        boolean showClustersAsSearchSuggestions =
                !mQueryState.isSearching() || mQueryState.getQuery().isEmpty();
        if (showClustersAsSearchSuggestions) {
            ensureHeaders();
            addClustersAsSearchSuggestions(result);
        } else {
            addExpandedClusters(result);
        }

        setDividerVisibilityForLastItem(false);
        ensureFooters(State.BUTTON, result.canLoadMore(), result);
    }

    private void addClustersAsSearchSuggestions(HistoryClustersResult result) {
        for (Map.Entry<String, Integer> entry : result.getLabelCounts().entrySet()) {
            // Check if label exists in the model already
            // If not, create a new entry
            String rawLabel = entry.getKey();
            PropertyModel existingModel = mLabelToModelMap.get(rawLabel);
            if (existingModel == null) {
                Drawable journeysDrawable =
                        AppCompatResources.getDrawable(mContext, R.drawable.ic_journeys);
                existingModel =
                        new PropertyModel.Builder(HistoryClustersItemProperties.ALL_KEYS)
                                .with(HistoryClustersItemProperties.ICON_DRAWABLE, journeysDrawable)
                                .with(HistoryClustersItemProperties.DIVIDER_VISIBLE, true)
                                .with(HistoryClustersItemProperties.DIVIDER_IS_THICK, false)
                                .with(HistoryClustersItemProperties.TITLE,
                                        getQuotedLabelFromRawLabel(rawLabel, result.getClusters()))
                                .with(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, null)
                                .with(HistoryClustersItemProperties.ACCESSIBILITY_STATE,
                                        ClusterViewAccessibilityState.CLICKABLE)
                                .with(HistoryClustersItemProperties.START_ICON_VISIBILITY,
                                        View.VISIBLE)
                                .with(HistoryClustersItemProperties.START_ICON_BACKGROUND_RES,
                                        R.drawable.selectable_rounded_rectangle)
                                .with(HistoryClustersItemProperties.CLICK_HANDLER,
                                        (v)
                                                -> setQueryState(QueryState.forQuery(rawLabel,
                                                        mDelegate.getSearchEmptyString())))
                                .build();
                mLabelToModelMap.put(rawLabel, existingModel);
                ListItem clusterItem = new ListItem(ItemType.CLUSTER, existingModel);
                mModelList.add(clusterItem);
            }
            existingModel.set(HistoryClustersItemProperties.LABEL,
                    mResources.getQuantityString(R.plurals.history_clusters_n_matches,
                            entry.getValue(), entry.getValue()));
        }

        if (!mIsScrollToLoadDisabled && result.canLoadMore() && !result.isContinuation()) {
            continueQuery(result);
        }
    }

    private void addExpandedClusters(HistoryClustersResult result) {
        List<HistoryCluster> clusters = result.getClusters();
        for (int clusterIdx = 0; clusterIdx < clusters.size(); clusterIdx++) {
            HistoryCluster cluster = clusters.get(clusterIdx);
            if (cluster.getVisits().size() < MIN_EXPANDED_CLUSTER_SIZE) {
                continue;
            }

            PropertyModel clusterModel =
                    new PropertyModel.Builder(HistoryClustersItemProperties.ALL_KEYS)
                            .with(HistoryClustersItemProperties.TITLE,
                                    applyBolding(cluster.getLabel(), cluster.getMatchPositions()))
                            .with(HistoryClustersItemProperties.DIVIDER_VISIBLE, false)
                            .with(HistoryClustersItemProperties.ACCESSIBILITY_STATE,
                                    ClusterViewAccessibilityState.COLLAPSIBLE)
                            .with(HistoryClustersItemProperties.START_ICON_VISIBILITY, View.GONE)
                            .build();
            ListItem clusterItem = new ListItem(ItemType.CLUSTER, clusterModel);
            mModelList.add(clusterItem);

            List<ListItem> visitsAndRelatedSearches =
                    new ArrayList<>(cluster.getVisits().size() + 1);

            for (int visitIdx = 0; visitIdx < cluster.getVisits().size(); visitIdx++) {
                ClusterVisit visit = cluster.getVisits().get(visitIdx);
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
                                .with(HistoryClustersItemProperties.DIVIDER_VISIBLE, false)
                                .with(HistoryClustersItemProperties.END_BUTTON_VISIBLE, true)
                                .build();
                if (mLargeIconBridge != null) {
                    mLargeIconBridge.getLargeIconForUrl(visit.getNormalizedUrl(), mMinFaviconSize,
                            (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
                                    int iconType) -> {
                                Drawable drawable = FaviconUtils.getIconDrawableWithoutFilter(icon,
                                        visit.getNormalizedUrl(), fallbackColor, mIconGenerator,
                                        mResources, mDisplayedFaviconSize);
                                visitModel.set(
                                        HistoryClustersItemProperties.ICON_DRAWABLE, drawable);
                            });
                }

                ListItem listItem = new ListItem(ItemType.VISIT, visitModel);
                mVisitMetadataMap.put(
                        visit, new VisitMetadata(listItem, clusterItem, visitsAndRelatedSearches));
                visitsAndRelatedSearches.add(listItem);
            }

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
                ListItem relatedSearchesItem =
                        new ListItem(ItemType.RELATED_SEARCHES, relatedSearchesModel);
                visitsAndRelatedSearches.add(relatedSearchesItem);
            }

            PropertyModel lastModelInList =
                    visitsAndRelatedSearches.get(visitsAndRelatedSearches.size() - 1).model;
            lastModelInList.set(HistoryClustersItemProperties.DIVIDER_VISIBLE, true);
            lastModelInList.set(HistoryClustersItemProperties.DIVIDER_IS_THICK, true);

            mModelList.addAll(visitsAndRelatedSearches);
            clusterModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                    v -> hideClusterContents(clusterItem, visitsAndRelatedSearches));
            Drawable chevron = UiUtils.getTintedDrawable(mContext,
                    R.drawable.ic_expand_less_black_24dp, R.color.default_icon_color_tint_list);
            clusterModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, chevron);
            clusterModel.set(
                    HistoryClustersItemProperties.LABEL, getTimeString(cluster.getTimestamp()));
        }
    }

    private void setDividerVisibilityForLastItem(boolean visible) {
        for (int i = mModelList.size() - 1; i >= 0; i--) {
            ListItem listItem = mModelList.get(i);
            if (listItem.type == ItemType.VISIT || listItem.type == ItemType.RELATED_SEARCHES
                    || listItem.type == ItemType.CLUSTER) {
                listItem.model.set(HistoryClustersItemProperties.DIVIDER_VISIBLE, visible);
                return;
            }
        }
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

    private void ensureFooters(
            @State int buttonState, boolean canLoadMore, HistoryClustersResult result) {
        boolean showVerticallyCentered = buttonState == State.LOADING && !mIsScrollToLoadDisabled;
        boolean shouldShowLoadIndicator =
                (buttonState == State.BUTTON && canLoadMore && mIsScrollToLoadDisabled)
                || buttonState == State.LOADING;
        int currentIndex = mModelList.indexOf(mMoreProgressItem);
        boolean showing = currentIndex != -1;
        if (showing) {
            mModelList.remove(mMoreProgressItem);
        }

        if (shouldShowLoadIndicator) {
            mModelList.add(mMoreProgressItem);
            mMoreProgressItem.model.set(
                    HistoryClustersItemProperties.SHOW_VERTICALLY_CENTERED, showVerticallyCentered);
            mMoreProgressItem.model.set(
                    HistoryClustersItemProperties.PROGRESS_BUTTON_STATE, buttonState);
        }

        boolean emptyTextShowing = mModelList.indexOf(mEmptyTextListItem) != -1;
        boolean shouldShowEmptyText = !mQueryState.isSearching() && result != null
                && !result.isContinuation() && result.getClusters().isEmpty();
        if (emptyTextShowing) {
            mModelList.remove(mEmptyTextListItem);
        }

        if (shouldShowEmptyText) {
            mModelList.add(mEmptyTextListItem);
        }
    }

    private void announceForAccessibility(String messsage) {
        mAnnounceForAccessibilityCallback.onResult(messsage);
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
    void hideClusterContents(ListItem clusterItem, List<ListItem> itemsToHide) {
        int indexOfFirstVisit = mModelList.indexOf(itemsToHide.get(0));
        PropertyModel clusterModel = clusterItem.model;
        clusterModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                (v) -> showClusterContents(clusterItem, itemsToHide));
        clusterModel.set(HistoryClustersItemProperties.DIVIDER_VISIBLE, true);
        clusterModel.set(HistoryClustersItemProperties.DIVIDER_IS_THICK, true);
        Drawable chevron = UiUtils.getTintedDrawable(mContext, R.drawable.ic_expand_more_black_24dp,
                R.color.default_icon_color_tint_list);
        clusterModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, chevron);
        clusterModel.set(HistoryClustersItemProperties.ACCESSIBILITY_STATE,
                ClusterViewAccessibilityState.EXPANDABLE);
        itemsToHide.get(itemsToHide.size() - 1)
                .model.set(HistoryClustersItemProperties.DIVIDER_VISIBLE, false);

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
    void showClusterContents(ListItem clusterItem, List<ListItem> itemsToShow) {
        PropertyModel clusterModel = clusterItem.model;
        clusterModel.set(HistoryClustersItemProperties.CLICK_HANDLER,
                (v) -> hideClusterContents(clusterItem, itemsToShow));
        clusterModel.set(HistoryClustersItemProperties.DIVIDER_VISIBLE, false);
        Drawable chevron = UiUtils.getTintedDrawable(mContext, R.drawable.ic_expand_less_black_24dp,
                R.color.default_icon_color_tint_list);
        clusterModel.set(HistoryClustersItemProperties.END_BUTTON_DRAWABLE, chevron);
        clusterModel.set(HistoryClustersItemProperties.ACCESSIBILITY_STATE,
                ClusterViewAccessibilityState.COLLAPSIBLE);
        PropertyModel lastModelInList = itemsToShow.get(itemsToShow.size() - 1).model;
        lastModelInList.set(HistoryClustersItemProperties.DIVIDER_VISIBLE, true);
        lastModelInList.set(HistoryClustersItemProperties.DIVIDER_IS_THICK, true);
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

    private void onPromiseRejected(Exception e) {}
}
