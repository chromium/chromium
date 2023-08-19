// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.app.Activity;
import android.os.Handler;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout.LayoutParams;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.widget.MoreProgressButton;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Root component for the HistoryClusters UI component, which displays lists of related history
 * visits grouped into clusters.
 */
public class HistoryClustersCoordinator extends RecyclerView.OnScrollListener
        implements OnMenuItemClickListener, SnackbarController {
    private static class DisabledSelectionDelegate extends SelectionDelegate {
        @Override
        public boolean toggleSelectionForItem(Object o) {
            return false;
        }

        @Override
        public boolean isItemSelected(Object o) {
            return false;
        }

        @Override
        public boolean isSelectionEnabled() {
            return false;
        }
    }

    private final HistoryClustersMediator mMediator;
    private final ModelList mModelList;
    private final HistoryClustersDelegate mDelegate;
    private SimpleRecyclerViewAdapter mAdapter;
    private final Activity mActivity;
    private boolean mActivityViewInflated;
    private final PropertyModel mToolbarModel;
    private ViewGroup mActivityContentView;
    private HistoryClustersToolbar mToolbar;
    private SelectionDelegate<ClusterVisit> mSelectionDelegate;
    private SelectableListLayout mSelectableListLayout;
    private SelectionDelegate mDisabledSelectionDelegate = new DisabledSelectionDelegate();
    private RecyclerView mRecyclerView;
    private final HistoryClustersMetricsLogger mMetricsLogger;
    private final SnackbarManager mSnackbarManager;

    @VisibleForTesting
    HistoryClustersCoordinator(@NonNull Profile profile, @NonNull Activity activity,
            TemplateUrlService templateUrlService, HistoryClustersDelegate historyClustersDelegate,
            HistoryClustersMetricsLogger metricsLogger,
            SelectionDelegate<ClusterVisit> selectionDelegate, SnackbarManager snackbarManager) {
        mActivity = activity;
        mDelegate = historyClustersDelegate;
        mModelList = new ModelList();
        mToolbarModel = new PropertyModel.Builder(HistoryClustersToolbarProperties.ALL_KEYS)
                                .with(HistoryClustersToolbarProperties.QUERY_STATE,
                                        QueryState.forQueryless())
                                .build();
        mMetricsLogger = metricsLogger;
        mSelectionDelegate = selectionDelegate;
        mSnackbarManager = snackbarManager;

        mMediator = new HistoryClustersMediator(HistoryClustersBridge.getForProfile(profile),
                new LargeIconBridge(profile), mActivity, mActivity.getResources(), mModelList,
                mToolbarModel, mDelegate, System::currentTimeMillis, templateUrlService,
                mSelectionDelegate, mMetricsLogger, (message) -> {
                    if (mRecyclerView == null) return;
                    mRecyclerView.announceForAccessibility(message);
                }, new Handler());
    }

    /**
     * Construct a new HistoryClustersCoordinator.
     * @param profile The profile from which the coordinator should access history data.
     * @param activity Activity in which this UI resides.
     * @param historyClustersDelegate Delegate that provides functionality that must be implemented
     *         externally, e.g. populating intents targeting activities we can't reference directly.
     * @param accessibilityUtil Utility object that tells us about the current accessibility state.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     */
    public HistoryClustersCoordinator(@NonNull Profile profile, @NonNull Activity activity,
            TemplateUrlService templateUrlService, HistoryClustersDelegate historyClustersDelegate,
            SnackbarManager snackbarManager) {
        this(profile, activity, templateUrlService, historyClustersDelegate,
                new HistoryClustersMetricsLogger(templateUrlService), new SelectionDelegate<>(),
                snackbarManager);
    }

    public void destroy() {
        mMediator.destroy();
        mMetricsLogger.destroy();
        if (mActivityViewInflated) {
            mSelectableListLayout.onDestroyed();
        }
    }

    /** Called when the user toggles to or from the Journeys UI in the containing history page. */
    public void onToggled(boolean toggledTo) {
        if (toggledTo) {
            mMetricsLogger.setInitialState(HistoryClustersMetricsLogger.InitialState.SAME_DOCUMENT);
            mMediator.setQueryState(QueryState.forQueryless());
            updateInfoMenuItem(mDelegate.shouldShowPrivacyDisclaimerSupplier().get());
        } else {
            mMetricsLogger.incrementToggleCount();
        }
    }

    public void setInitialQuery(QueryState queryState) {
        mMetricsLogger.setInitialState(
                HistoryClustersMetricsLogger.InitialState.INDIRECT_NAVIGATION);
        mMediator.setQueryState(queryState);
    }

    /**
     * Opens the History Clusters UI. On phones this opens the History Activity; on tablets, it
     * navigates to a NativePage in the active tab.
     * @param query The preset query to populate when opening the UI.
     */
    public void openHistoryClustersUi(String query) {
        mMediator.openHistoryClustersUi(query);
    }

    public int getHistoryClustersIconResId() {
        return R.drawable.ic_journeys;
    }

    /** Gets the root view for a "full activity" presentation of the user's history clusters. */
    public ViewGroup getActivityContentView() {
        if (!mActivityViewInflated) {
            inflateActivityView();
        }

        return mActivityContentView;
    }

    /** Handles a back button press event, returning true if the event is handled. */
    public boolean onBackPressed() {
        return mSelectableListLayout.onBackPressed();
    }

    public BackPressHandler getBackPressHandler() {
        return mSelectableListLayout;
    }

    /** Called to notify the Journeys UI that history has been deleted by some other party. */
    public void onHistoryDeletedExternally() {
        mMediator.onHistoryDeletedExternally();
    }

    void inflateActivityView() {
        mAdapter = new SimpleRecyclerViewAdapter(mModelList);
        mAdapter.registerType(
                ItemType.VISIT, this::buildVisitView, HistoryClustersViewBinder::bindVisitView);
        mAdapter.registerType(ItemType.CLUSTER, this::buildClusterView,
                HistoryClustersViewBinder::bindClusterView);
        mAdapter.registerType(ItemType.RELATED_SEARCHES, this::buildRelatedSearchesView,
                HistoryClustersViewBinder::bindRelatedSearchesView);
        mAdapter.registerType(
                ItemType.TOGGLE, mDelegate::getToggleView, HistoryClustersViewBinder::noopBindView);
        mAdapter.registerType(ItemType.PRIVACY_DISCLAIMER, mDelegate::getPrivacyDisclaimerView,
                HistoryClustersViewBinder::noopBindView);
        mAdapter.registerType(ItemType.CLEAR_BROWSING_DATA, mDelegate::getClearBrowsingDataView,
                HistoryClustersViewBinder::noopBindView);
        mAdapter.registerType(ItemType.MORE_PROGRESS, this::buildMoreProgressView,
                HistoryClustersViewBinder::bindMoreProgressView);
        mAdapter.registerType(ItemType.EMPTY_TEXT, this::buildEmptyTextView,
                HistoryClustersViewBinder::noopBindView);

        LayoutInflater layoutInflater = LayoutInflater.from(mActivity);
        mActivityContentView = (ViewGroup) layoutInflater.inflate(
                R.layout.history_clusters_activity_content, null);

        mSelectableListLayout = mActivityContentView.findViewById(R.id.selectable_list);
        mSelectableListLayout.setEmptyViewText(R.string.history_manager_empty);
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(mAdapter);

        mRecyclerView.setLayoutManager(new LinearLayoutManager(
                mRecyclerView.getContext(), LinearLayoutManager.VERTICAL, false));
        mRecyclerView.addOnScrollListener(mMediator);
        mRecyclerView.addOnScrollListener(this);

        mToolbar = (HistoryClustersToolbar) mSelectableListLayout.initializeToolbar(
                R.layout.history_clusters_toolbar, mSelectionDelegate, R.string.menu_history,
                R.id.normal_menu_group, R.id.selection_mode_menu_group, this, true);
        mToolbar.initializeSearchView(
                mMediator, R.string.history_clusters_search_your_journeys, R.id.search_menu_id);
        mSelectableListLayout.configureWideDisplayStyle();
        mToolbar.setSearchEnabled(true);
        if (!mDelegate.isSeparateActivity()) {
            mToolbar.getMenu().removeItem(R.id.close_menu_id);
        }

        if (!mDelegate.areTabGroupsEnabled()) {
            mToolbar.getMenu().removeItem(R.id.selection_mode_open_in_tab_group);
        }

        mToolbar.setInfoMenuItem(R.id.info_menu_id);
        mDelegate.shouldShowPrivacyDisclaimerSupplier().addObserver(this::updateInfoMenuItem);

        PropertyModelChangeProcessor.create(
                mToolbarModel, mToolbar, HistoryClustersViewBinder::bindToolbar);
        PropertyModelChangeProcessor.create(
                mToolbarModel, mSelectableListLayout, HistoryClustersViewBinder::bindListLayout);

        mActivityViewInflated = true;
    }

    private View buildMoreProgressView(ViewGroup parent) {
        MoreProgressButton moreProgressButton =
                (MoreProgressButton) LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.more_progress_button, parent, false);
        moreProgressButton.setButtonText(moreProgressButton.getResources().getString(
                R.string.history_clusters_show_more_button_label));
        View progressSpinner = moreProgressButton.findViewById(R.id.progress_spinner);
        if (progressSpinner != null) {
            ((LayoutParams) progressSpinner.getLayoutParams()).gravity = Gravity.CENTER;
        }
        return moreProgressButton;
    }

    private View buildClusterView(ViewGroup parent) {
        SelectableItemView<HistoryCluster> clusterView =
                (SelectableItemView<HistoryCluster>) LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.history_cluster, parent, false);
        clusterView.setSelectionDelegate(mDisabledSelectionDelegate);
        return clusterView;
    }

    private View buildVisitView(ViewGroup parent) {
        SelectableItemView<ClusterVisit> itemView =
                (SelectableItemView<ClusterVisit>) LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.history_cluster_visit, parent, false);
        itemView.setSelectionDelegate(mSelectionDelegate);
        return itemView;
    }

    private View buildRelatedSearchesView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.history_clusters_related_searches_view, parent, false);
    }

    private View buildEmptyTextView(ViewGroup parent) {
        View wrapper = LayoutInflater.from(parent.getContext())
                               .inflate(R.layout.empty_text_view, parent, false);
        TextView innerView = wrapper.findViewById(R.id.empty_view);
        innerView.setText(R.string.history_manager_empty);
        return wrapper;
    }

    // OnMenuItemClickListener implementation.
    @Override
    public boolean onMenuItemClick(MenuItem menuItem) {
        if (menuItem.getItemId() == R.id.search_menu_id) {
            mMediator.setQueryState(QueryState.forQuery("", mDelegate.getSearchEmptyString()));
            return true;
        } else if (menuItem.getItemId() == R.id.close_menu_id && mDelegate.isSeparateActivity()) {
            mActivity.finish();
            return true;
        } else if (menuItem.getItemId() == R.id.selection_mode_open_in_incognito) {
            mMediator.openVisitsInNewTabs(mSelectionDelegate.getSelectedItemsAsList(), true, false);
            mSelectionDelegate.clearSelection();
            return true;
        } else if (menuItem.getItemId() == R.id.selection_mode_open_in_new_tab) {
            mMediator.openVisitsInNewTabs(
                    mSelectionDelegate.getSelectedItemsAsList(), false, false);
            mSelectionDelegate.clearSelection();
            return true;
        } else if (menuItem.getItemId() == R.id.info_menu_id) {
            mDelegate.toggleInfoHeaderVisibility();
            updateInfoMenuItem(mDelegate.shouldShowPrivacyDisclaimerSupplier().get());
        } else if (menuItem.getItemId() == R.id.selection_mode_delete_menu_id) {
            mMediator.deleteVisits(mSelectionDelegate.getSelectedItemsAsList());
            mSelectionDelegate.clearSelection();
            return true;
        } else if (menuItem.getItemId() == R.id.optout_menu_id) {
            mDelegate.onOptOut();
        } else if (menuItem.getItemId() == R.id.selection_mode_open_in_tab_group) {
            mMediator.openVisitsInNewTabs(mSelectionDelegate.getSelectedItemsAsList(), false, true);
            return true;
        } else if (menuItem.getItemId() == R.id.selection_mode_copy_link) {
            Clipboard.getInstance().setText(mSelectionDelegate.getSelectedItemsAsList()
                                                    .get(0)
                                                    .getNormalizedUrl()
                                                    .getSpec());
            mSelectionDelegate.clearSelection();
            Snackbar snackbar = Snackbar.make(mActivity.getString(R.string.copied), this,
                    Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_HISTORY_LINK_COPIED);
            mSnackbarManager.showSnackbar(snackbar);
        }
        return false;
    }

    private void updateInfoMenuItem(boolean showingDisclaimer) {
        boolean firstAdapterItemScrolledOff =
                ((LinearLayoutManager) mRecyclerView.getLayoutManager())
                        .findFirstVisibleItemPosition()
                > 0;

        boolean showItem = !firstAdapterItemScrolledOff
                && mDelegate.hasOtherFormsOfBrowsingHistory() && mModelList.size() > 0
                && !mToolbar.isSearching() && !mSelectionDelegate.isSelectionEnabled();

        mToolbar.updateInfoMenuItem(showItem, showingDisclaimer);
    }

    // SnackbarController implementation.
    @Override
    public void onAction(Object actionData) {}

    @Override
    public void onDismissNoAction(Object actionData) {}

    // OnScrollListener implementation.
    @Override
    public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
        updateInfoMenuItem(mDelegate.shouldShowPrivacyDisclaimerSupplier().get());
    }

    @VisibleForTesting
    public RecyclerView getRecyclerViewFortesting() {
        return mRecyclerView;
    }

    public SelectableListToolbar getToolbarForTesting() {
        return mToolbar;
    }
}
