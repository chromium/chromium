// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.google.android.material.tabs.TabLayout;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.Clipboard;

import java.util.List;

/** Combines and manages the different UI components of browsing history. */
public class HistoryManager
        implements OnMenuItemClickListener,
                SelectionObserver<HistoryItem>,
                SearchDelegate,
                SnackbarController,
                HistoryContentManager.Observer,
                BackPressHandler {
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
    private ViewGroup mRootView;
    private ViewGroup mContentView;
    @Nullable private final SelectableListLayout<HistoryItem> mSelectableListLayout;
    private HistoryContentManager mContentManager;
    private SelectionDelegate<HistoryItem> mSelectionDelegate;
    private HistoryManagerToolbar mToolbar;
    private TextView mEmptyView;
    private final SnackbarManager mSnackbarManager;
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
     *
     * @param activity The Activity associated with the HistoryManager.
     * @param isSeparateActivity Whether the history UI will be shown in a separate activity than
     *     the main Chrome activity.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param profile The profile launching History.
     * @param tabSupplier Supplies the current tab, null if the history UI will be shown in a
     *     separate activity.
     * @param historyProvider Provider of methods for querying and managing browsing history.
     * @param clientPackageName Package name of the client the history UI is launched on top of.
     * @param shouldShowClearData Whether the 'Clear browsing data' button should be shown.
     */
    @SuppressWarnings("unchecked") // mSelectableListLayout
    public HistoryManager(
            @NonNull Activity activity,
            boolean isSeparateActivity,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Profile profile,
            @Nullable Supplier<Tab> tabSupplier,
            HistoryProvider historyProvider,
            @Nullable String clientPackageName,
            boolean shouldShowClearData) {
        mActivity = activity;
        mIsSeparateActivity = isSeparateActivity;
        mSnackbarManager = snackbarManager;
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

        // 1. Create selectable components.
        mSelectableListLayout =
                (SelectableListLayout<HistoryItem>)
                        LayoutInflater.from(activity).inflate(R.layout.history_main, null);
        mSelectionDelegate = new SelectionDelegate<>();
        mSelectionDelegate.addObserver(this);

        // 2. Create HistoryContentManager and initialize recycler view.
        boolean shouldShowInfoHeader =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO, true);
        mContentManager =
                new HistoryContentManager(
                        mActivity,
                        this,
                        isSeparateActivity,
                        profile,
                        shouldShowInfoHeader,
                        shouldShowClearData,
                        /* hostName= */ null,
                        mSelectionDelegate,
                        tabSupplier,
                        historyProvider,
                        clientPackageName);
        mSelectableListLayout.initializeRecyclerView(
                mContentManager.getAdapter(), mContentManager.getRecyclerView());

        mShouldShowPrivacyDisclaimerSupplier.set(
                shouldShowInfoHeader && mContentManager.hasPrivacyDisclaimers());
        mShouldShowClearBrowsingDataSupplier.set(mContentManager.getShouldShowClearData());

        // 3. Initialize toolbar.
        mToolbar =
                (HistoryManagerToolbar)
                        mSelectableListLayout.initializeToolbar(
                                R.layout.history_toolbar,
                                mSelectionDelegate,
                                R.string.menu_history,
                                R.id.normal_menu_group,
                                R.id.selection_mode_menu_group,
                                this,
                                isSeparateActivity);
        mToolbar.setManager(this);
        mToolbar.setPrefService(UserPrefs.get(profile));
        mToolbar.initializeSearchView(this, R.string.history_manager_search, R.id.search_menu_id);
        mToolbar.setInfoMenuItem(R.id.info_menu_id);
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());

        // 4. Width constrain the SelectableListLayout.
        mSelectableListLayout.configureWideDisplayStyle();

        // 5. Initialize empty view.
        mEmptyView =
                mSelectableListLayout.initializeEmptyStateView(
                        R.drawable.history_empty_state_illustration,
                        R.string.history_manager_empty_state,
                        R.string.history_manager_empty_state_view_or_clear_page_visited);

        // 6. Load items.
        mContentManager.startLoadingItems();

        setContentView(mSelectableListLayout);
        mRootView.addView(mContentView);
        mSelectableListLayout
                .getHandleBackPressChangedSupplier()
                .addObserver((x) -> onBackPressStateChanged());

        onBackPressStateChanged(); // Initialize back press State.
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
            Clipboard.getInstance()
                    .setText(mSelectionDelegate.getSelectedItemsAsList().get(0).getUrl().getSpec());
            mSelectionDelegate.clearSelection();
            Snackbar snackbar =
                    Snackbar.make(
                            mActivity.getString(R.string.copied),
                            this,
                            Snackbar.TYPE_NOTIFICATION,
                            Snackbar.UMA_HISTORY_LINK_COPIED);
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
                mContentManager
                        .getRecyclerView()
                        .announceForAccessibility(
                                mActivity.getString(
                                        R.string.multiple_history_items_deleted, numItemsRemoved));
            }

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
        }
        return false;
    }

    private void toggleInfoHeaderVisibility() {
        boolean shouldShowInfoHeader =
                !mContentManager.getShouldShowPrivacyDisclaimersIfAvailable();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO, shouldShowInfoHeader);
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeader);
        mContentManager.updatePrivacyDisclaimers(shouldShowInfoHeader);
        mShouldShowPrivacyDisclaimerSupplier.set(
                shouldShowInfoHeader && mContentManager.hasPrivacyDisclaimers());
    }

    private String getSearchEmptyString() {
        String defaultSearchEngineName = null;
        TemplateUrl dseTemplateUrl =
                TemplateUrlServiceFactory.getForProfile(mProfile)
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
        ViewGroup placeholderView =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.incognito_history_placeholder, null);
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

    private void setContentView(ViewGroup contentView) {
        mContentView = contentView;
        onBackPressStateChanged();
    }

    /** Called when the activity/native page is destroyed. */
    public void onDestroyed() {
        if (mIsIncognito) {
            // If Incognito placeholder is shown no need to call any destroy method.
            return;
        }

        if (mSelectableListLayout != null) {
            mSelectableListLayout.onDestroyed();
            mContentManager.onDestroyed();
        }
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
        mBackPressStateSupplier.set(
                mSelectableListLayout.getHandleBackPressChangedSupplier().get());
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

    /**
     * Called when the user presses the back key. This is only going to be called when the history
     * UI is shown in a separate activity rather inside a tab.
     *
     * @return True if manager handles this event, false if it decides to ignore.
     */
    private boolean onBackPressed() {
        if (mIsIncognito || mSelectableListLayout == null) {
            // If Incognito placeholder is shown, the back press should handled by HistoryActivity.
            return false;
        }
        return mSelectableListLayout.onBackPressed();
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
                METRICS_PREFIX + "ClearBrowsingData.PerProfileType",
                type,
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

        return !firstAdapterItemScrolledOff
                && mContentManager.hasPrivacyDisclaimers()
                && mContentManager.getItemCount() > 0
                && !mToolbar.isSearching()
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
                mContentManager.getShouldShowPrivacyDisclaimersIfAvailable()
                        && mContentManager.hasPrivacyDisclaimers());
    }

    // HistoryContentManager.Observer
    @Override
    public void onUserAccountStateChanged() {
        mToolbar.onSignInStateChange();
        mShouldShowClearBrowsingDataSupplier.set(mContentManager.getShouldShowClearData());
    }

    // HistoryContentManager.Observer
    @Override
    public void onHistoryDeletedExternally() {}

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
}
