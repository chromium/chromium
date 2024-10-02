// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Build;
import android.view.KeyEvent;
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

import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.history.HistoryManagerToolbar.InfoHeaderPref;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.ItemViewType;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.components.prefs.PrefService;
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
    private final boolean mLaunchedForApp;
    private final HistoryUmaRecorder mUmaRecorder;
    private final InfoHeaderPref mHeaderPref;
    private final String mAppId;

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

    public static boolean isAppSpecificHistoryEnabled() {
        return ChromeFeatureList.sAppSpecificHistory.isEnabled()
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
    }

    /**
     * Creates a new HistoryManager.
     *
     * @param activity The Activity associated with the HistoryManager.
     * @param isSeparateActivity Whether the history UI will be shown in a separate activity than
     *     the main Chrome activity.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param profile The profile launching History.
     * @param bottomSheetController Supplier of {@link BottomSheetController} to show app filter
     *     sheet in.
     * @param tabSupplier Supplies the current tab, null if the history UI will be shown in a
     *     separate activity.
     * @param historyProvider Provider of methods for querying and managing browsing history.
     * @param umaRecorder Records UMA user action/histograms.
     * @param clientPackageName Package name of the client the history UI is launched on top of.
     * @param shouldShowClearData Whether the 'Clear browsing data' button should be shown.
     * @param launchedForApp Whether history UI is launched for app-specific history.
     * @param showAppFilter Whether history page will show app filter UI.
     */
    @SuppressWarnings("unchecked") // mSelectableListLayout
    public HistoryManager(
            @NonNull Activity activity,
            boolean isSeparateActivity,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Profile profile,
            @Nullable Supplier<BottomSheetController> bottomSheetController,
            @Nullable Supplier<Tab> tabSupplier,
            HistoryProvider historyProvider,
            @NonNull HistoryUmaRecorder umaRecorder,
            @Nullable String clientPackageName,
            boolean shouldShowClearData,
            boolean launchedForApp,
            boolean showAppFilter) {
        mActivity = activity;
        mIsSeparateActivity = isSeparateActivity;
        mSnackbarManager = snackbarManager;
        assert profile != null;
        mProfile = profile;
        mIsIncognito = profile.isOffTheRecord();
        mUmaRecorder = umaRecorder;
        mLaunchedForApp = launchedForApp;
        mAppId = clientPackageName;

        mPrefService = UserPrefs.get(mProfile);
        mBackPressStateSupplier.set(false);

        // When launched for apps, info header always starts in hidden state.
        mHeaderPref =
                launchedForApp
                        ? new AppHistoryInfoHeaderPref()
                        : new BrowserHistoryInfoHeaderPref();

        mUmaRecorder.recordOpenHistory();
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
        boolean shouldShowInfoHeader = mHeaderPref.isVisible();

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
                        bottomSheetController,
                        tabSupplier,
                        () -> mToolbar.hideKeyboard(),
                        mUmaRecorder,
                        historyProvider,
                        clientPackageName,
                        launchedForApp,
                        showAppFilter);
        mSelectableListLayout.initializeRecyclerView(
                mContentManager.getAdapter(), mContentManager.getRecyclerView());
        if (mContentManager.showAppFilter()) {
            // Now the search mode can have a header. Let the layout ignore it to
            // return the right item count.
            mSelectableListLayout.ignoreItemTypeForEmptyState(ItemViewType.HEADER);
        }

        mShouldShowPrivacyDisclaimerSupplier.set(
                shouldShowInfoHeader && mContentManager.isInfoHeaderAvailable());
        mShouldShowClearBrowsingDataSupplier.set(mContentManager.getShouldShowClearData());

        // 3. Initialize toolbar.
        mToolbar =
                (HistoryManagerToolbar)
                        mSelectableListLayout.initializeToolbar(
                                R.layout.history_toolbar,
                                mSelectionDelegate,
                                launchedForApp ? R.string.chrome_history : R.string.menu_history,
                                R.id.normal_menu_group,
                                R.id.selection_mode_menu_group,
                                this,
                                isSeparateActivity,
                                launchedForApp
                                        ? R.menu.app_specific_history_manager_menu
                                        : R.menu.history_manager_menu,
                                launchedForApp);
        mToolbar.setManager(this);
        mToolbar.setMenuDelegate(
                new HistoryManagerToolbar.HistoryManagerMenuDelegate() {
                    @Override
                    public boolean supportsDeletingHistory() {
                        return mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY);
                    }

                    @Override
                    public boolean supportsIncognito() {
                        return IncognitoUtils.isIncognitoModeEnabled(profile);
                    }
                });
        mToolbar.initializeSearchView(this, R.string.history_manager_search, R.id.search_menu_id);
        mToolbar.setInfoMenuItem(R.id.info_menu_id);
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());

        // Make the toolbar focusable, so that focus transitions can move out from descendents of
        // the toolbar to the neighboring delete button, and automatically to other items on the
        // HistoryPage such as the list of HistoryItem(s).
        mToolbar.setFocusable(true);
        mToolbar.setNextFocusForwardId(R.id.clear_browsing_data_button);
        mToolbar.setOnKeyListener(
                (View view, int keyCode, KeyEvent event) -> {
                    if ((keyCode == KeyEvent.KEYCODE_ENTER || keyCode == KeyEvent.KEYCODE_DPAD_DOWN)
                            && event.getAction() == KeyEvent.ACTION_UP) {
                        mToolbar.getMenu().performIdentifierAction(R.id.search_menu_id, 0);
                        return true;
                    }
                    return false;
                });

        // 4. Width constrain the SelectableListLayout.
        mSelectableListLayout.configureWideDisplayStyle();

        // 5. Initialize empty view.
        initializeEmptyView();

        // 6. Load items.
        mContentManager.startLoadingItems();

        setContentView(mSelectableListLayout);
        mRootView.addView(mContentView);
        mSelectableListLayout
                .getHandleBackPressChangedSupplier()
                .addObserver((x) -> onBackPressStateChanged());

        onBackPressStateChanged(); // Initialize back press State.
        mContentManager.maybeQueryApps();
    }

    private void initializeEmptyView() {
        int imgResId =
                mLaunchedForApp
                        ? R.drawable.history_app_empty_state_illustration
                        : R.drawable.history_empty_state_illustration;
        int subjResId =
                mLaunchedForApp
                        ? R.string.history_manager_app_specific_empty_state_title
                        : R.string.history_manager_empty_state;
        Resources res = mActivity.getResources();
        String descText =
                mLaunchedForApp
                        ? res.getString(
                                R.string.history_manager_app_specific_empty_state_description,
                                mContentManager.getAppInfoCache().get(mAppId).label)
                        : res.getString(
                                R.string.history_manager_empty_state_view_or_clear_page_visited);
        mEmptyView = mSelectableListLayout.initializeEmptyStateView(imgResId, subjResId, descText);
    }

    /**
     * @return Whether the history manager UI is displayed in a separate activity than the main
     *     Chrome activity.
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
            mUmaRecorder.recordCopyLink(mIsSearching);
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
            mUmaRecorder.recordRemoveSelected(mIsSearching);

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
            mContentManager.maybeResetAppFilterChip();
            mContentManager.getAdapter().onSearchStart();
            mToolbar.showSearchView(true);
            String searchEmptyString = getSearchEmptyString();
            mSelectableListLayout.onStartSearch(
                    searchEmptyString,
                    R.string.history_manager_empty_state_view_or_open_more_history);
            mUmaRecorder.recordSearchHistory();
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
        mHeaderPref.setVisible(shouldShowInfoHeader);
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeader);
        mContentManager.updatePrivacyDisclaimers(shouldShowInfoHeader);
        mShouldShowPrivacyDisclaimerSupplier.set(
                shouldShowInfoHeader && mContentManager.isInfoHeaderAvailable());
    }

    private String getSearchEmptyString() {
        if (mLaunchedForApp) {
            return mActivity.getString(R.string.history_manager_app_specific_history_no_results);
        }
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

    protected void finish() {
        mActivity.finish();
    }

    private void openItemsInNewTabs(List<HistoryItem> items, boolean isIncognito) {
        mUmaRecorder.recordOpenInTabs(mIsSearching, isIncognito);
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
                && mContentManager.isInfoHeaderAvailable()
                && mContentManager.getItemCount() > 0
                && !mToolbar.isSearching()
                && !mSelectionDelegate.isSelectionEnabled();
    }

    void showIPH() {
        AppSpecificHistoryIPHController iphController =
                new AppSpecificHistoryIPHController(mActivity, () -> mProfile);
        iphController.maybeShowIPH();
    }

    /**
     * @return True if the available privacy disclaimers should be shown. Note that this may return
     *     true even if there are currently no privacy disclaimers.
     */
    boolean shouldShowInfoHeaderIfAvailable() {
        return mContentManager.getShouldShowPrivacyDisclaimersIfAvailable();
    }

    void recordSelectionEstablished() {
        mUmaRecorder.recordSelectionEstablished(mIsSearching);
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
            mUmaRecorder.recordLoadMoreOnScroll(mIsSearching);
        }
    }

    // HistoryContentManager.Observer
    @Override
    public void onItemClicked(HistoryItem item) {
        mUmaRecorder.recordOpenItem(mIsSearching);
    }

    // HistoryContentManager.Observer
    @Override
    public void onItemRemoved(HistoryItem item) {
        mUmaRecorder.recordRemoveItem(mIsSearching);
        if (mSelectionDelegate.isItemSelected(item)) {
            mSelectionDelegate.toggleSelectionForItem(item);
        }
    }

    // HistoryContentManager.Observer
    @Override
    public void onClearBrowsingDataClicked() {
        mUmaRecorder.recordClearBrowsingData(mIsIncognito);
        // Opens the clear browsing data preference.
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(
                mActivity, SettingsNavigation.SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE);
    }

    // HistoryContentManager.Observer
    @Override
    public void onPrivacyDisclaimerHasChanged() {
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());
        mShouldShowPrivacyDisclaimerSupplier.set(
                mContentManager.getShouldShowPrivacyDisclaimersIfAvailable()
                        && mContentManager.isInfoHeaderAvailable());
    }

    @Override
    public void onOpenFullChromeHistoryClicked() {
        Intent fullHistoryIntent = new Intent(Intent.ACTION_MAIN);
        fullHistoryIntent.setClass(mActivity, ChromeLauncherActivity.class);
        fullHistoryIntent.putExtra(IntentHandler.EXTRA_OPEN_HISTORY, true);
        IntentUtils.addTrustedIntentExtras(fullHistoryIntent);
        mActivity.startActivity(fullHistoryIntent);
        mUmaRecorder.recordOpenFullHistory();
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

    InfoHeaderPref getInfoHeaderPrefForTests() {
        return mHeaderPref;
    }
}
