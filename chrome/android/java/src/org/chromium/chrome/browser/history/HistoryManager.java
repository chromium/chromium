// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Rect;
import android.text.TextUtils;
import android.transition.AutoTransition;
import android.transition.Scene;
import android.transition.Transition;
import android.transition.TransitionManager;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayout.OnTabSelectedListener;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.history_clusters.ClusterVisit;
import org.chromium.chrome.browser.history_clusters.HistoryClustersCoordinator;
import org.chromium.chrome.browser.history_clusters.HistoryClustersDelegate;
import org.chromium.chrome.browser.history_clusters.QueryState;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.DateViewHolder;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.Clipboard;
import org.chromium.url.GURL;

import java.io.Serializable;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Combines and manages the different UI components of browsing history.
 */
public class HistoryManager implements OnMenuItemClickListener, SelectionObserver<HistoryItem>,
                                       SearchDelegate, SnackbarController,
                                       HistoryContentManager.Observer, BackPressHandler {
    private static final String METRICS_PREFIX = "Android.HistoryPage.";
    static final String HISTORY_CLUSTERS_VISIBLE_PREF = "history_clusters.visible";

    // Keep consistent with the UMA constants on the WebUI history page (history/constants.js).
    private static final int UMA_MAX_BUCKET_VALUE = 1000;
    private static final int UMA_MAX_SUBSET_BUCKET_VALUE = 100;

    // TODO(msramek): The WebUI counterpart computes the bucket count by
    // dividing by 10 until it gets under 100, reaching 10 for both
    // UMA_MAX_BUCKET_VALUE and UMA_MAX_SUBSET_BUCKET_VALUE, and adds +1
    // for overflow. How do we keep that in sync with this code?
    private static final int HISTORY_TAB_INDEX = 0;
    private static final int JOURNEYS_TAB_INDEX = 1;

    private final Activity mActivity;
    private final boolean mIsIncognito;
    private final boolean mIsSeparateActivity;
    private final HistoryProvider mHistoryProvider;
    private final ObservableSupplierImpl<Boolean> mShowHistoryClustersToggleSupplier =
            new ObservableSupplierImpl<>();
    private ViewGroup mRootView;
    private ViewGroup mContentView;
    @Nullable
    private final SelectableListLayout<HistoryItem> mSelectableListLayout;
    private HistoryContentManager mContentManager;
    private SelectionDelegate<HistoryItem> mSelectionDelegate;
    private HistoryManagerToolbar mToolbar;
    private TextView mEmptyView;
    private final SnackbarManager mSnackbarManager;
    private @Nullable HistoryClustersCoordinator mHistoryClustersCoordinator;
    private final ObservableSupplierImpl<Boolean> mShouldShowPrivacyDisclaimerSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mShouldShowClearBrowsingDataSupplier =
            new ObservableSupplierImpl<>();

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();

    private final PrefService mPrefService;
    private final Profile mProfile;
    private @Nullable TabLayout mHistoryTabToggle;
    private @Nullable TabLayout mJourneysTabToggle;

    private boolean mIsSearching;

    /**
     * Creates a new HistoryManager.
     * @param activity The Activity associated with the HistoryManager.
     * @param isSeparateActivity Whether the history UI will be shown in a separate activity than
     *                           the main Chrome activity.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param profile The profile launching History.
     * @param tabSupplier Supplies the current tab, null if the history UI will be shown in a
     *                    separate activity.
     * @param showHistoryClustersImmediately Whether the Journeys (history clusters) UI should be
     *         shown immediately instead of the normal history UI.
     * @param historyClustersQuery The preset query that the Journeys UI should use.
     * @param historyProvider Provider of methods for querying and managing browsing history.
     */
    @SuppressWarnings("unchecked") // mSelectableListLayout
    public HistoryManager(@NonNull Activity activity, boolean isSeparateActivity,
            @NonNull SnackbarManager snackbarManager, @NonNull Profile profile,
            @Nullable Supplier<Tab> tabSupplier, boolean showHistoryClustersImmediately,
            String historyClustersQuery, HistoryProvider historyProvider) {
        mActivity = activity;
        mIsSeparateActivity = isSeparateActivity;
        mSnackbarManager = snackbarManager;
        mHistoryProvider = historyProvider;
        assert profile != null;
        mProfile = profile;
        mIsIncognito = profile.isOffTheRecord();

        mPrefService = UserPrefs.get(mProfile);
        mBackPressStateSupplier.set(false);

        recordUserAction("Show");
        // If incognito placeholder is shown, we don't need to create History UI elements.
        if (mIsIncognito) {
            mSelectableListLayout = null;
            mRootView = getIncognitoHistoryPlaceholderView();
            return;
        }

        mRootView = new FrameLayout(mActivity);

        boolean historyClustersPrefIsManaged =
                mPrefService.isManagedPreference(HISTORY_CLUSTERS_VISIBLE_PREF);
        boolean historyClustersEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.HISTORY_JOURNEYS)
                && !(historyClustersPrefIsManaged
                        && !mPrefService.getBoolean(HISTORY_CLUSTERS_VISIBLE_PREF));
        if (historyClustersEnabled) {
            HistoryClustersDelegate historyClustersDelegate = new HistoryClustersDelegate() {
                @Override
                public boolean isSeparateActivity() {
                    return isSeparateActivity;
                }

                @Override
                public Tab getTab() {
                    return tabSupplier.get();
                }

                @Override
                public Intent getHistoryActivityIntent() {
                    return null;
                }

                @Override
                public <SerializableList extends List<String> & Serializable> Intent
                getOpenUrlIntent(GURL gurl, boolean inIncognito, boolean createNewTab,
                        boolean inTabGroup, @Nullable SerializableList additionalUrls) {
                    Intent intent =
                            mContentManager.getOpenUrlIntent(gurl, inIncognito, createNewTab);
                    if (additionalUrls != null) {
                        intent.putExtra(IntentHandler.EXTRA_ADDITIONAL_URLS, additionalUrls);
                        intent.putExtra(
                                IntentHandler.EXTRA_OPEN_ADDITIONAL_URLS_IN_TAB_GROUP, inTabGroup);
                    }

                    return intent;
                }

                @Override
                public ViewGroup getToggleView(ViewGroup parent) {
                    return buildToggleView(parent, JOURNEYS_TAB_INDEX);
                }

                @Override
                public TabCreator getTabCreator(boolean isIncognito) {
                    return new TabDelegate(isIncognito);
                }

                @Nullable
                @Override
                public ViewGroup getPrivacyDisclaimerView(ViewGroup parent) {
                    ViewGroup viewGroup =
                            mContentManager.getAdapter().getPrivacyDisclaimerContainer(parent);
                    viewGroup.findViewById(R.id.privacy_disclaimer_bottom_space)
                            .setVisibility(View.GONE);
                    return viewGroup;
                }

                @Nullable
                @Override
                public ObservableSupplier<Boolean> shouldShowPrivacyDisclaimerSupplier() {
                    return mShouldShowPrivacyDisclaimerSupplier;
                }

                @Override
                public void toggleInfoHeaderVisibility() {
                    HistoryManager.this.toggleInfoHeaderVisibility();
                }

                @Override
                public boolean hasOtherFormsOfBrowsingHistory() {
                    return mContentManager.hasPrivacyDisclaimers();
                }

                @Nullable
                @Override
                public ViewGroup getClearBrowsingDataView(ViewGroup parent) {
                    return mContentManager.getAdapter().getClearBrowsingDataButtonContainer(parent);
                }

                @Nullable
                @Override
                public ObservableSupplier<Boolean> shouldShowClearBrowsingDataSupplier() {
                    return mShouldShowClearBrowsingDataSupplier;
                }

                @Override
                public void markVisitForRemoval(ClusterVisit clusterVisit) {
                    HistoryItem item = new HistoryItem(clusterVisit.getRawUrl(), null, null,
                            clusterVisit.getTimestamp(), new long[] {clusterVisit.getTimestamp()},
                            false);
                    mHistoryProvider.markItemForRemoval(item);
                    for (int i = 0; i < clusterVisit.getDuplicateVisits().size(); i++) {
                        ClusterVisit.DuplicateVisit duplicateVisit =
                                clusterVisit.getDuplicateVisits().get(i);
                        item = new HistoryItem(duplicateVisit.getUrl(), null, null,
                                duplicateVisit.getTimestamp(),
                                new long[] {duplicateVisit.getTimestamp()}, false);
                        mHistoryProvider.markItemForRemoval(item);
                    }
                }

                @Override
                public void removeMarkedItems() {
                    mHistoryProvider.removeItems();
                }

                @Override
                public String getSearchEmptyString() {
                    return HistoryManager.this.getSearchEmptyString();
                }

                @Override
                public void onOptOut() {
                    onHistoryClustersOptOutChanged(false);
                }

                @Override
                public boolean isRenameEnabled() {
                    return ChromeFeatureList.isEnabled(ChromeFeatureList.RENAME_JOURNEYS);
                }
            };

            mHistoryClustersCoordinator = new HistoryClustersCoordinator(mProfile, activity,
                    TemplateUrlServiceFactory.getForProfile(mProfile), historyClustersDelegate,
                    mSnackbarManager);
        }

        // 1. Create selectable components.
        mSelectableListLayout =
                (SelectableListLayout<HistoryItem>) LayoutInflater.from(activity).inflate(
                        R.layout.history_main, null);
        mSelectionDelegate = new SelectionDelegate<>();
        mSelectionDelegate.addObserver(this);

        // 2. Create HistoryContentManager and initialize recycler view.
        boolean shouldShowInfoHeader = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO, true);
        mContentManager = new HistoryContentManager(mActivity, this, isSeparateActivity, profile,
                shouldShowInfoHeader, /* shouldShowClearData */ true,
                /* hostName */ null, mSelectionDelegate, tabSupplier,
                mShowHistoryClustersToggleSupplier,
                (vg) -> buildToggleView(vg, HISTORY_TAB_INDEX), historyProvider);
        mSelectableListLayout.initializeRecyclerView(
                mContentManager.getAdapter(), mContentManager.getRecyclerView());

        mShouldShowPrivacyDisclaimerSupplier.set(shouldShowInfoHeader);
        mShouldShowClearBrowsingDataSupplier.set(mContentManager.getShouldShowClearData());

        // 3. Initialize toolbar.
        mToolbar = (HistoryManagerToolbar) mSelectableListLayout.initializeToolbar(
                R.layout.history_toolbar, mSelectionDelegate, R.string.menu_history,
                R.id.normal_menu_group, R.id.selection_mode_menu_group, this, isSeparateActivity);
        mToolbar.setManager(this);
        mToolbar.setPrefService(UserPrefs.get(profile));
        mToolbar.initializeSearchView(this, R.string.history_manager_search, R.id.search_menu_id);
        mToolbar.setInfoMenuItem(R.id.info_menu_id);
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());
        if (historyClustersEnabled) {
            boolean historyClustersVisible = mPrefService.getBoolean(HISTORY_CLUSTERS_VISIBLE_PREF);
            mShowHistoryClustersToggleSupplier.set(historyClustersVisible);
            mToolbar.getMenu()
                    .findItem(R.id.optout_menu_id)
                    .setVisible(true)
                    .setTitle(historyClustersVisible
                                    ? R.string.history_clusters_disable_menu_item_label
                                    : R.string.history_clusters_enable_menu_item_label);
            // If the rename is enabled or in the unlikely event history clusters is force enabled
            // by policy, remove the menu option to turn it off.
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.RENAME_JOURNEYS)
                    || historyClustersPrefIsManaged) {
                mToolbar.getMenu().removeItem(R.id.optout_menu_id);
            }
        } else {
            mToolbar.getMenu().removeItem(R.id.optout_menu_id);
        }

        // 4. Width constrain the SelectableListLayout.
        mSelectableListLayout.configureWideDisplayStyle();

        // 5. Initialize empty view.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.EMPTY_STATES)) {
            mEmptyView = mSelectableListLayout.initializeEmptyStateView(
                    R.drawable.history_empty_state_illustration,
                    R.string.history_manager_empty_state,
                    R.string.history_manager_empty_state_view_or_clear_page_visited);
        } else {
            mEmptyView = mSelectableListLayout.initializeEmptyView(R.string.history_manager_empty);
        }

        // 6. Load items.
        mContentManager.startLoadingItems();

        if (showHistoryClustersImmediately) {
            setContentView(mHistoryClustersCoordinator.getActivityContentView());
            QueryState queryState = TextUtils.isEmpty(historyClustersQuery)
                    ? QueryState.forQueryless()
                    : QueryState.forQuery(historyClustersQuery, getSearchEmptyString());
            mHistoryClustersCoordinator.setInitialQuery(queryState);
        } else {
            setContentView(mSelectableListLayout);
        }
        mRootView.addView(mContentView);
        mSelectableListLayout.getHandleBackPressChangedSupplier().addObserver(
                (x) -> onBackPressStateChanged());
        if (mHistoryClustersCoordinator != null) {
            mHistoryClustersCoordinator.getBackPressHandler()
                    .getHandleBackPressChangedSupplier()
                    .addObserver((x) -> onBackPressStateChanged());
        }

        onBackPressStateChanged(); // Initialize back press State.
    }

    private void onHistoryClustersOptOutChanged(boolean isVisible) {
        mPrefService.setBoolean(HISTORY_CLUSTERS_VISIBLE_PREF, isVisible);
        if (isVisible) {
            mToolbar.getMenu()
                    .findItem(R.id.optout_menu_id)
                    .setTitle(R.string.history_clusters_disable_menu_item_label);
            mShowHistoryClustersToggleSupplier.set(true);
        } else {
            mToolbar.getMenu()
                    .findItem(R.id.optout_menu_id)
                    .setTitle(R.string.history_clusters_enable_menu_item_label);
            if (isHistoryClustersUIShowing()) {
                swapContentView();
            }
            mShowHistoryClustersToggleSupplier.set(false);
        }
    }

    private ViewGroup buildToggleView(ViewGroup parent, int selectedIndex) {
        ViewGroup viewGroup = (ViewGroup) LayoutInflater.from(mActivity).inflate(
                R.layout.history_toggle, parent, false);

        TabLayout tabLayout = viewGroup.findViewById(R.id.history_toggle_tab_layout);
        TabLayout.Tab selectedTab = tabLayout.getTabAt(selectedIndex);
        tabLayout.selectTab(selectedTab);

        if (selectedIndex == HISTORY_TAB_INDEX) {
            mHistoryTabToggle = tabLayout;
        } else {
            assert selectedIndex == JOURNEYS_TAB_INDEX;
            mJourneysTabToggle = tabLayout;
        }

        tabLayout.addOnTabSelectedListener(new OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                if (tab != selectedTab) {
                    swapContentView();
                }
            }

            @Override
            public void onTabUnselected(TabLayout.Tab tab) {}
            @Override
            public void onTabReselected(TabLayout.Tab tab) {}
        });

        TabLayout.Tab firstTab = tabLayout.getTabAt(0);
        TabLayout.Tab secondTab = tabLayout.getTabAt(1);
        int leftPadding = firstTab.view.getPaddingLeft();
        firstTab.view.setPadding(leftPadding, 0, leftPadding, 0);
        secondTab.view.setPadding(leftPadding, 0, leftPadding, 0);

        // The TabLayout is too short for the minimum touch target size (48dp) so we expand the true
        // touch target by adding a CompositeTouchDelegate. This will route touch events from the
        // full 48dp band to the correct tab.
        CompositeTouchDelegate compositeTouchDelegate = new CompositeTouchDelegate(viewGroup);
        viewGroup.setTouchDelegate(compositeTouchDelegate);
        firstTab.view.addOnLayoutChangeListener(
                (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom)
                        -> updateTouchDelegate(
                                compositeTouchDelegate, view, tabLayout, new AtomicReference<>()));
        secondTab.view.addOnLayoutChangeListener(
                (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom)
                        -> updateTouchDelegate(
                                compositeTouchDelegate, view, tabLayout, new AtomicReference<>()));
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.RENAME_JOURNEYS)) {
            firstTab.view.getTab().setText(R.string.history_clusters_by_date_tab_label);
            secondTab.view.getTab().setText(R.string.history_clusters_by_group_tab_label);
        }
        return viewGroup;
    }

    private void updateTouchDelegate(CompositeTouchDelegate compositeTouchDelegate, View tabView,
            View tabLayout, AtomicReference<TouchDelegate> touchDelegateRef) {
        Rect tabBounds = getTabViewBoundsRelativeToGrandparent(tabView, tabLayout);
        int addedTouchTargetHeight = tabView.getResources().getDimensionPixelSize(
                R.dimen.history_toggle_added_touch_target_height);
        tabBounds.top -= addedTouchTargetHeight;
        tabBounds.bottom += addedTouchTargetHeight;

        TouchDelegate oldTouchDelegate = touchDelegateRef.get();
        if (oldTouchDelegate != null) {
            compositeTouchDelegate.removeDelegateForDescendantView(oldTouchDelegate);
        }

        TouchDelegate newTouchDelegate = new TouchDelegate(tabBounds, tabView);
        compositeTouchDelegate.addDelegateForDescendantView(newTouchDelegate);
        touchDelegateRef.set(newTouchDelegate);
    }

    /**
     * Gets the bounds of a TabView relative to its grandparent by offsetting its HitRect by the
     * position of its parent TabLayout.
     */
    private Rect getTabViewBoundsRelativeToGrandparent(View tabView, View tabLayout) {
        Rect tabBounds = new Rect();
        tabView.getHitRect(tabBounds);
        tabBounds.offset(tabLayout.getLeft(), tabLayout.getTop());
        return tabBounds;
    }

    /**
     * @return Whether the history manager UI is displayed in a separate activity than the main
     *         Chrome activity.
     */
    public boolean isDisplayedInSeparateActivity() {
        return mIsSeparateActivity;
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        mToolbar.hideOverflowMenu();

        if (item.getItemId() == R.id.close_menu_id && isDisplayedInSeparateActivity()) {
            mActivity.finish();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_open_in_new_tab) {
            openItemsInNewTabs(mSelectionDelegate.getSelectedItemsAsList(), false);
            return true;
        } else if (item.getItemId() == R.id.selection_mode_copy_link) {
            recordUserActionWithOptionalSearch("CopyLink");
            Clipboard.getInstance().setText(
                    mSelectionDelegate.getSelectedItemsAsList().get(0).getUrl().getSpec());
            mSelectionDelegate.clearSelection();
            Snackbar snackbar = Snackbar.make(mActivity.getString(R.string.copied), this,
                    Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_HISTORY_LINK_COPIED);
            mSnackbarManager.showSnackbar(snackbar);
            return true;
        } else if (item.getItemId() == R.id.selection_mode_open_in_incognito) {
            openItemsInNewTabs(mSelectionDelegate.getSelectedItemsAsList(), true);
            return true;
        } else if (item.getItemId() == R.id.selection_mode_delete_menu_id) {
            recordUserActionWithOptionalSearch("RemoveSelected");

            int numItemsRemoved = 0;
            HistoryItem lastItemRemoved = null;
            for (HistoryItem historyItem : mSelectionDelegate.getSelectedItems()) {
                mContentManager.markItemForRemoval(historyItem);
                numItemsRemoved++;
                lastItemRemoved = historyItem;
            }

            mContentManager.removeItems();
            mSelectionDelegate.clearSelection();

            if (numItemsRemoved == 1) {
                assert lastItemRemoved != null;
                mContentManager.announceItemRemoved(lastItemRemoved);
            } else if (numItemsRemoved > 1) {
                mContentManager.getRecyclerView().announceForAccessibility(mActivity.getString(
                        R.string.multiple_history_items_deleted, numItemsRemoved));
            }

            notifyHistoryClustersCoordinatorOfDeletion();

            return true;
        } else if (item.getItemId() == R.id.search_menu_id) {
            mContentManager.removeHeader();
            mToolbar.showSearchView(true);
            String searchEmptyString = getSearchEmptyString();
            mSelectableListLayout.onStartSearch(searchEmptyString);
            recordUserAction("Search");
            mIsSearching = true;
            return true;
        } else if (item.getItemId() == R.id.info_menu_id) {
            toggleInfoHeaderVisibility();
        } else if (item.getItemId() == R.id.optout_menu_id) {
            onHistoryClustersOptOutChanged(!mPrefService.getBoolean(HISTORY_CLUSTERS_VISIBLE_PREF));
            return true;
        }
        return false;
    }

    private void toggleInfoHeaderVisibility() {
        boolean shouldShowInfoHeader =
                !mContentManager.getShouldShowPrivacyDisclaimersIfAvailable();
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO, shouldShowInfoHeader);
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeader);
        mContentManager.updatePrivacyDisclaimers(shouldShowInfoHeader);
        mShouldShowPrivacyDisclaimerSupplier.set(shouldShowInfoHeader);
    }

    private String getSearchEmptyString() {
        String defaultSearchEngineName = null;
        TemplateUrl dseTemplateUrl = TemplateUrlServiceFactory.getForProfile(mProfile)
                                             .getDefaultSearchEngineTemplateUrl();
        if (dseTemplateUrl != null) defaultSearchEngineName = dseTemplateUrl.getShortName();
        return defaultSearchEngineName == null
                ? mActivity.getString(R.string.history_manager_no_results_no_dse)
                : mActivity.getString(R.string.history_manager_no_results, defaultSearchEngineName);
    }

    /**
     * @return The view that shows the main browsing history UI.
     */
    public ViewGroup getView() {
        return mRootView;
    }

    /**
     * @return The placeholder view to be shown instead of history UI in incognito mode.
     */
    private ViewGroup getIncognitoHistoryPlaceholderView() {
        ViewGroup placeholderView = (ViewGroup) LayoutInflater.from(mActivity).inflate(
                R.layout.incognito_history_placeholder, null);
        ImageButton dismissButton =
                placeholderView.findViewById(R.id.close_history_placeholder_button);
        if (mIsSeparateActivity) {
            dismissButton.setOnClickListener(v -> mActivity.finish());
        } else {
            dismissButton.setVisibility(View.GONE);
        }
        placeholderView.setFocusable(true);
        placeholderView.setFocusableInTouchMode(true);
        return placeholderView;
    }

    private void swapContentView() {
        boolean toHistoryClusters;
        if (mIsIncognito) {
            return;
        } else if (isHistoryClustersUIShowing()) {
            toHistoryClusters = false;
            mHistoryClustersCoordinator.onToggled(false);
            setContentView(mSelectableListLayout);
            mContentManager.startLoadingItems();
            // Each page of content has a distinct TabLayout with independent selection state, but
            // should only ever display the selected tab corresponding to the owning page. i.e. Page
            // X's TabLayout should always show Tab X as selected. This means the selection state
            // becomes incorrect when toggling away. If this toggle field is not null, that means
            // we're coming back to an existing TabLayout that needs to have its selected tab reset.
            // Note that this cannot easily be done at selection time because there's a race
            // somewhere. Resetting at the last second seems to be more consistent.
            if (mHistoryTabToggle != null) {
                mHistoryTabToggle.selectTab(mHistoryTabToggle.getTabAt(HISTORY_TAB_INDEX));
            }
        } else {
            assert mHistoryClustersCoordinator
                    != null : "swapContentView() shouldn't be called if HistoryClusters is off";
            toHistoryClusters = true;
            setContentView(mHistoryClustersCoordinator.getActivityContentView());
            mHistoryClustersCoordinator.onToggled(true);
            if (mJourneysTabToggle != null) {
                mJourneysTabToggle.selectTab(mJourneysTabToggle.getTabAt(JOURNEYS_TAB_INDEX));
            }
        }

        Transition transition = makeContentSwapTransition(toHistoryClusters);
        Scene scene = new Scene(mRootView, mContentView);
        TransitionManager.go(scene, transition);
        mContentView.requestFocus();
    }

    private void setContentView(ViewGroup contentView) {
        mContentView = contentView;
        onBackPressStateChanged();
    }

    private Transition makeContentSwapTransition(boolean toHistoryClusters) {
        Transition transition = new AutoTransition();
        transition.addTarget(SelectableItemView.class);
        if (!toHistoryClusters) {
            HistoryAdapter adapter = mContentManager.getAdapter();
            RecyclerView recyclerView = mContentManager.getRecyclerView();
            int lastVisiblePosition = ((LinearLayoutManager) recyclerView.getLayoutManager())
                                              .findLastVisibleItemPosition();
            for (int i = 0; i < adapter.getItemCount() && i <= lastVisiblePosition; i++) {
                ViewHolder vh = recyclerView.findViewHolderForAdapterPosition(i);
                if (vh instanceof DateViewHolder) {
                    transition.addTarget(vh.itemView);
                }
            }
        }

        return transition;
    }

    private boolean isHistoryClustersUIShowing() {
        return mHistoryClustersCoordinator != null
                && mContentView == mHistoryClustersCoordinator.getActivityContentView();
    }

    /**
     * Called when the activity/native page is destroyed.
     */
    public void onDestroyed() {
        if (mIsIncognito) {
            // If Incognito placeholder is shown no need to call any destroy method.
            return;
        }
        if (mHistoryClustersCoordinator != null) {
            mHistoryClustersCoordinator.destroy();
        }

        if (mSelectableListLayout != null) {
            mSelectableListLayout.onDestroyed();
            mContentManager.onDestroyed();
        }
    }

    /**
     * Called when the user presses the back key. This is only going to be called
     * when the history UI is shown in a separate activity rather inside a tab.
     * @return True if manager handles this event, false if it decides to ignore.
     */
    public boolean onBackPressed() {
        if (mIsIncognito || mSelectableListLayout == null) {
            // If Incognito placeholder is shown, the back press should handled by HistoryActivity.
            return false;
        } else if (isHistoryClustersUIShowing()) {
            return mHistoryClustersCoordinator.onBackPressed();
        }
        return mSelectableListLayout.onBackPressed();
    }

    // BackPressHandler implementation.
    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressed() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    private void onBackPressStateChanged() {
        boolean shouldInterceptBackPress = isHistoryClustersUIShowing()
                ? mHistoryClustersCoordinator.getBackPressHandler()
                          .getHandleBackPressChangedSupplier()
                          .get()
                : mSelectableListLayout.getHandleBackPressChangedSupplier().get();
        mBackPressStateSupplier.set(shouldInterceptBackPress);
    }

    @Override
    public void onSearchTextChanged(String query) {
        mContentManager.search(query);
    }

    @Override
    public void onEndSearch() {
        mContentManager.onEndSearch();
        mSelectableListLayout.onEndSearch();
        mIsSearching = false;
    }

    /** @return The SelectableListLayout that displays HistoryItems. */
    public SelectableListLayout<HistoryItem> getSelectableListLayout() {
        return mSelectableListLayout;
    }

    private void openItemsInNewTabs(List<HistoryItem> items, boolean isIncognito) {
        recordUserActionWithOptionalSearch("OpenSelected" + (isIncognito ? "Incognito" : ""));
        mContentManager.openItemsInNewTab(items, isIncognito);
    }

    private void notifyHistoryClustersCoordinatorOfDeletion() {
        if (mHistoryClustersCoordinator == null) return;
        mHistoryClustersCoordinator.onHistoryDeletedExternally();
    }

    /**
     * @param action The user action string to record.
     */
    static void recordUserAction(String action) {
        RecordUserAction.record(METRICS_PREFIX + action);
    }

    /**
     * Records the user action with "Search" prepended if the user is currently searching.
     * @param action The user action string to record.
     */
    void recordUserActionWithOptionalSearch(String action) {
        recordUserAction((mIsSearching ? "Search." : "") + action);
    }

    private void recordClearBrowsingDataMetric() {
        @BrowserProfileType
        int type = mIsIncognito ? BrowserProfileType.INCOGNITO : BrowserProfileType.REGULAR;
        RecordHistogram.recordEnumeratedHistogram(
                METRICS_PREFIX + "ClearBrowsingData.PerProfileType", type,
                BrowserProfileType.MAX_VALUE + 1);
    }

    /**
     * @return True if info menu item should be shown on history toolbar, false otherwise.
     */
    boolean shouldShowInfoButton() {
        LinearLayoutManager layoutManager =
                (LinearLayoutManager) mContentManager.getRecyclerView().getLayoutManager();
        // Before the RecyclerView binds its items, LinearLayoutManager#firstVisibleItemPosition()
        // returns {@link RecyclerView#NO_POSITION}. If #findVisibleItemPosition() returns
        // NO_POSITION, the current adapter position should not prevent the info button from being
        // displayed if all of the other criteria is met. See crbug.com/756249#c3.
        boolean firstAdapterItemScrolledOff = layoutManager.findFirstVisibleItemPosition() > 0;

        return !firstAdapterItemScrolledOff && mContentManager.hasPrivacyDisclaimers()
                && mContentManager.getItemCount() > 0 && !mToolbar.isSearching()
                && !mSelectionDelegate.isSelectionEnabled();
    }

    /**
     * @return True if the available privacy disclaimers should be shown.
     * Note that this may return true even if there are currently no privacy disclaimers.
     */
    boolean shouldShowInfoHeaderIfAvailable() {
        return mContentManager.getShouldShowPrivacyDisclaimersIfAvailable();
    }

    @Override
    public void onSelectionStateChange(List<HistoryItem> selectedItems) {
        mContentManager.setSelectionActive(mSelectionDelegate.isSelectionEnabled());
    }

    @Override
    public void onAction(Object actionData) {
        // Handler for the link copied snackbar. Do nothing.
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        // Handler for the link copied snackbar. Do nothing.
    }

    // HistoryContentManager.Observer
    @Override
    public void onScrolledCallback(boolean loadedMore) {
        // Show info button if available if first visible position is close to info header;
        // otherwise hide info button.
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());
        if (loadedMore) {
            recordUserActionWithOptionalSearch("LoadMoreOnScroll");
        }
    }

    // HistoryContentManager.Observer
    @Override
    public void onItemClicked(HistoryItem item) {
        recordUserActionWithOptionalSearch("OpenItem");
    }

    // HistoryContentManager.Observer
    @Override
    public void onItemRemoved(HistoryItem item) {
        recordUserActionWithOptionalSearch("RemoveItem");
        if (mSelectionDelegate.isItemSelected(item)) {
            mSelectionDelegate.toggleSelectionForItem(item);
        }

        notifyHistoryClustersCoordinatorOfDeletion();
    }

    // HistoryContentManager.Observer
    @Override
    public void onClearBrowsingDataClicked() {
        // Opens the clear browsing data preference.
        recordUserAction("ClearBrowsingData");
        recordClearBrowsingDataMetric();
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(
                mActivity, SettingsLauncher.SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE);
    }

    // HistoryContentManager.Observer
    @Override
    public void onPrivacyDisclaimerHasChanged() {
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());
        mShouldShowPrivacyDisclaimerSupplier.set(
                mContentManager.getShouldShowPrivacyDisclaimersIfAvailable());
    }

    // HistoryContentManager.Observer
    @Override
    public void onUserAccountStateChanged() {
        mToolbar.onSignInStateChange();
        mShouldShowClearBrowsingDataSupplier.set(mContentManager.getShouldShowClearData());
    }

    // HistoryContentManager.Observer
    @Override
    public void onHistoryDeletedExternally() {
        notifyHistoryClustersCoordinatorOfDeletion();
    }

    TextView getEmptyViewForTests() {
        return mEmptyView;
    }

    public HistoryContentManager getContentManagerForTests() {
        return mContentManager;
    }

    SelectionDelegate<HistoryItem> getSelectionDelegateForTests() {
        return mSelectionDelegate;
    }

    HistoryManagerToolbar getToolbarForTests() {
        return mToolbar;
    }

    @VisibleForTesting
    @Nullable
    HistoryClustersCoordinator getHistoryClustersCoordinatorForTests() {
        return mHistoryClustersCoordinator;
    }
}
