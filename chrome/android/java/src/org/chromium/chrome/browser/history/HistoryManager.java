// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar.PrefObserver;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.PageTransition;

import java.util.List;

/**
 * Displays and manages the UI for browsing history.
 */
public class HistoryManager implements OnMenuItemClickListener, SignInStateObserver,
                                       SelectionObserver<HistoryItem>, SearchDelegate,
                                       SnackbarController, PrefObserver {
    private static final int FAVICON_MAX_CACHE_SIZE_BYTES =
            10 * ConversionUtils.BYTES_PER_MEGABYTE; // 10MB
    private static final String METRICS_PREFIX = "Android.HistoryPage.";

    // Keep consistent with the UMA constants on the WebUI history page (history/constants.js).
    private static final int UMA_MAX_BUCKET_VALUE = 1000;
    private static final int UMA_MAX_SUBSET_BUCKET_VALUE = 100;

    // TODO(msramek): The WebUI counterpart computes the bucket count by
    // dividing by 10 until it gets under 100, reaching 10 for both
    // UMA_MAX_BUCKET_VALUE and UMA_MAX_SUBSET_BUCKET_VALUE, and adds +1
    // for overflow. How do we keep that in sync with this code?
    private static final int UMA_BUCKET_COUNT = 11;

    // PageTransition value to use for all URL requests triggered by the history page.
    private static final int PAGE_TRANSITION_TYPE = PageTransition.AUTO_BOOKMARK;

    private static HistoryProvider sProviderForTests;
    private static Boolean sIsScrollToLoadDisabledForTests;

    private final Activity mActivity;
    private final boolean mIsIncognito;
    private final boolean mIsSeparateActivity;
    private final boolean mIsScrollToLoadDisabled;
    private final SelectableListLayout<HistoryItem> mSelectableListLayout;
    private final HistoryAdapter mHistoryAdapter;
    private final SelectionDelegate<HistoryItem> mSelectionDelegate;
    private final HistoryManagerToolbar mToolbar;
    private final TextView mEmptyView;
    private final RecyclerView mRecyclerView;
    private final SnackbarManager mSnackbarManager;
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<Tab> mTabSupplier;
    private LargeIconBridge mLargeIconBridge;

    private boolean mIsSearching;
    private boolean mShouldShowInfoHeader;

    /**
     * Creates a new HistoryManager.
     * @param activity The Activity associated with the HistoryManager.
     * @param isSeparateActivity Whether the history UI will be shown in a separate activity than
     *                           the main Chrome activity.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param isIncognito Whether the incognito tab model is currently selected.
     * @param tabCreatorManager Allows creation of tabs in different models, null if the history UI
     *                          will be shown in a separate activity.
     * @param tabSupplier Supplies the current tab, null if the history UI will be shown in a
     *                    separate activity.
     */
    @SuppressWarnings("unchecked") // mSelectableListLayout
    public HistoryManager(Activity activity, boolean isSeparateActivity,
            SnackbarManager snackbarManager, boolean isIncognito,
            @Nullable TabCreatorManager tabCreatorManager, @Nullable Supplier<Tab> tabSupplier) {
        mShouldShowInfoHeader = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO, true);
        mActivity = activity;
        mIsSeparateActivity = isSeparateActivity;
        mSnackbarManager = snackbarManager;
        mIsIncognito = isIncognito;
        mTabCreatorManager = tabCreatorManager;
        mTabSupplier = tabSupplier;
        mIsScrollToLoadDisabled = ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                || ChromeAccessibilityUtil.isHardwareKeyboardAttached(
                        mActivity.getResources().getConfiguration());

        mSelectionDelegate = new SelectionDelegate<>();
        mSelectionDelegate.addObserver(this);

        // History service is not keyed for Incognito profiles and {@link HistoryServiceFactory}
        // explicitly redirects to use regular profile for Incognito case.
        Profile profile = Profile.getLastUsedRegularProfile();
        mHistoryAdapter = new HistoryAdapter(mSelectionDelegate, this,
                sProviderForTests != null ? sProviderForTests : new BrowsingHistoryBridge(profile));

        // 1. Create SelectableListLayout.
        mSelectableListLayout =
                (SelectableListLayout<HistoryItem>) LayoutInflater.from(activity).inflate(
                        R.layout.history_main, null);

        // 2. Initialize RecyclerView.
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(mHistoryAdapter);

        // 3. Initialize toolbar.
        mToolbar = (HistoryManagerToolbar) mSelectableListLayout.initializeToolbar(
                R.layout.history_toolbar, mSelectionDelegate, R.string.menu_history,
                R.id.normal_menu_group, R.id.selection_mode_menu_group, this, true,
                isSeparateActivity);
        mToolbar.setManager(this);
        mToolbar.initializeSearchView(this, R.string.history_manager_search, R.id.search_menu_id);
        mToolbar.setInfoMenuItem(R.id.info_menu_id);
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());

        // 4. Width constrain the SelectableListLayout.
        mSelectableListLayout.configureWideDisplayStyle();

        // 5. Initialize empty view.
        mEmptyView = mSelectableListLayout.initializeEmptyView(
                R.string.history_manager_empty, R.string.history_manager_no_results);

        // 6. Create large icon bridge.
        mLargeIconBridge = new LargeIconBridge(Profile.getLastUsedRegularProfile());
        ActivityManager activityManager = ((ActivityManager) ContextUtils
                .getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize = Math.min(
                (activityManager.getMemoryClass() / 4) * ConversionUtils.BYTES_PER_MEGABYTE,
                FAVICON_MAX_CACHE_SIZE_BYTES);
        mLargeIconBridge.createCache(maxSize);

        // 7. Initialize the adapter to load items.
        mHistoryAdapter.generateHeaderItems();
        mHistoryAdapter.generateFooterItems();
        mHistoryAdapter.initialize();

        // 8. Add scroll listener to show/hide info button on scroll and page in more items
        // when necessary.
        mRecyclerView.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                LinearLayoutManager layoutManager =
                        (LinearLayoutManager) recyclerView.getLayoutManager();
                // Show info button if available if first visible position is close to info header;
                // otherwise hide info button.
                mToolbar.updateInfoMenuItem(
                        shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());

                if (!mHistoryAdapter.canLoadMoreItems() || isScrollToLoadDisabled()) {
                    return;
                }

                // Load more items if the scroll position is close to the bottom of the list.
                if (layoutManager.findLastVisibleItemPosition()
                        > (mHistoryAdapter.getItemCount() - 25)) {
                    mHistoryAdapter.loadMoreItems();
                    recordUserActionWithOptionalSearch("LoadMoreOnScroll");
                }
            }});

        // 9. Listen to changes in sign in state.
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .addSignInStateObserver(this);

        // 10. Create PrefChangeRegistrar to receive notifications on preference changes.
        mPrefChangeRegistrar = new PrefChangeRegistrar();
        mPrefChangeRegistrar.addObserver(Pref.ALLOW_DELETING_BROWSER_HISTORY, this);
        mPrefChangeRegistrar.addObserver(Pref.INCOGNITO_MODE_AVAILABILITY, this);

        recordUserAction("Show");
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
            mSelectionDelegate.clearSelection();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_copy_link) {
            recordUserActionWithOptionalSearch("CopyLink");
            Clipboard.getInstance().setText(
                    mSelectionDelegate.getSelectedItemsAsList().get(0).getUrl());
            mSelectionDelegate.clearSelection();
            Snackbar snackbar = Snackbar.make(mActivity.getString(R.string.copied), this,
                    Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_HISTORY_LINK_COPIED);
            mSnackbarManager.showSnackbar(snackbar);
            return true;
        } else if (item.getItemId() == R.id.selection_mode_open_in_incognito) {
            openItemsInNewTabs(mSelectionDelegate.getSelectedItemsAsList(), true);
            mSelectionDelegate.clearSelection();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_delete_menu_id) {
            recordUserActionWithOptionalSearch("RemoveSelected");

            int numItemsRemoved = 0;
            HistoryItem lastItemRemoved = null;
            for (HistoryItem historyItem : mSelectionDelegate.getSelectedItems()) {
                mHistoryAdapter.markItemForRemoval(historyItem);
                numItemsRemoved++;
                lastItemRemoved = historyItem;
            }

            mHistoryAdapter.removeItems();
            mSelectionDelegate.clearSelection();

            if (numItemsRemoved == 1) {
                assert lastItemRemoved != null;
                announceItemRemoved(lastItemRemoved);
            } else if (numItemsRemoved > 1) {
                mRecyclerView.announceForAccessibility(mRecyclerView.getContext().getString(
                        R.string.multiple_history_items_deleted, numItemsRemoved));
            }

            return true;
        } else if (item.getItemId() == R.id.search_menu_id) {
            mHistoryAdapter.removeHeader();
            mToolbar.showSearchView();
            mSelectableListLayout.onStartSearch();
            recordUserAction("Search");
            mIsSearching = true;
            return true;
        } else if (item.getItemId() == R.id.info_menu_id) {
            mShouldShowInfoHeader = !mShouldShowInfoHeader;
            SharedPreferencesManager.getInstance().writeBoolean(
                    ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO, mShouldShowInfoHeader);
            mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());
            mHistoryAdapter.setPrivacyDisclaimer();
        }
        return false;
    }

    /**
     * @return The view that shows the main browsing history UI.
     */
    public ViewGroup getView() {
        return mSelectableListLayout;
    }

    /**
     * Called when the activity/native page is destroyed.
     */
    public void onDestroyed() {
        mSelectableListLayout.onDestroyed();
        mHistoryAdapter.onDestroyed();
        mLargeIconBridge.destroy();
        mLargeIconBridge = null;
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .removeSignInStateObserver(this);
        mPrefChangeRegistrar.destroy();
    }

    /**
     * Called when the user presses the back key. This is only going to be called
     * when the history UI is shown in a separate activity rather inside a tab.
     * @return True if manager handles this event, false if it decides to ignore.
     */
    public boolean onBackPressed() {
        return mSelectableListLayout.onBackPressed();
    }

    /**
     * Removes the HistoryItem from the history backend and the HistoryAdapter.
     * @param item The HistoryItem to remove.
     */
    public void removeItem(HistoryItem item) {
        if (mSelectionDelegate.isItemSelected(item)) {
            mSelectionDelegate.toggleSelectionForItem(item);
        }
        mHistoryAdapter.markItemForRemoval(item);
        mHistoryAdapter.removeItems();
        announceItemRemoved(item);
    }

    private void announceItemRemoved(HistoryItem item) {
        mRecyclerView.announceForAccessibility(
                mRecyclerView.getContext().getString(R.string.delete_message, item.getTitle()));
    }

    /**
     * Open the provided url.
     * @param url The url to open.
     * @param isIncognito Whether to open the url in an incognito tab. If null, the tab
     *                    will open in the current tab model.
     * @param createNewTab Whether a new tab should be created. If false, the item will clobber the
     *                     the current tab.
     */
    public void openUrl(String url, Boolean isIncognito, boolean createNewTab) {
        if (isDisplayedInSeparateActivity()) {
            IntentHandler.startActivityForTrustedIntent(
                    getOpenUrlIntent(url, isIncognito, createNewTab));
            return;
        }

        assert mTabCreatorManager != null;
        assert mTabSupplier != null;

        Tab tab = mTabSupplier.get();
        assert tab != null;
        if (createNewTab) {
            TabCreator tabCreator =
                    mTabCreatorManager.getTabCreator(isIncognito != null && isIncognito);
            tabCreator.createNewTab(
                    new LoadUrlParams(url, PAGE_TRANSITION_TYPE), TabLaunchType.FROM_LINK, tab);
        } else {
            tab.loadUrl(new LoadUrlParams(url, PAGE_TRANSITION_TYPE));
        }
    }

    /**
     * @return Whether the HistoryManager is displaying history for the incognito profile.
     */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    @VisibleForTesting
    Intent getOpenUrlIntent(String url, Boolean isIncognito, boolean createNewTab) {
        // Construct basic intent.
        Intent viewIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        viewIntent.putExtra(Browser.EXTRA_APPLICATION_ID,
                mActivity.getApplicationContext().getPackageName());
        viewIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Determine component or class name.
        ComponentName component;
        if (mActivity instanceof HistoryActivity) { // phone
            component = IntentUtils.safeGetParcelableExtra(
                    mActivity.getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT);
        } else { // tablet
            component = mActivity.getComponentName();
        }
        if (component != null) {
            ChromeTabbedActivity.setNonAliasedComponent(viewIntent, component);
        } else {
            viewIntent.setClass(mActivity, ChromeLauncherActivity.class);
        }

        // Set other intent extras.
        if (isIncognito != null) {
            viewIntent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, isIncognito);
        }
        if (createNewTab) viewIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);

        viewIntent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PAGE_TRANSITION_TYPE);
        return viewIntent;
    }

    /**
     * Opens the clear browsing data preference.
     */
    public void openClearBrowsingDataPreference() {
        recordUserAction("ClearBrowsingData");
        recordClearBrowsingDataMetric();
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(mActivity, ClearBrowsingDataTabsFragment.class);
    }

    @Override
    public void onSearchTextChanged(String query) {
        mHistoryAdapter.search(query);
    }

    @Override
    public void onEndSearch() {
        mHistoryAdapter.onEndSearch();
        mSelectableListLayout.onEndSearch();
        mIsSearching = false;
    }

    /**
     * @return The {@link LargeIconBridge} used to fetch large favicons.
     */
    public LargeIconBridge getLargeIconBridge() {
        return mLargeIconBridge;
    }

    /**
     * @return The SelectableListLayout that displays HistoryItems.
     */
    public SelectableListLayout<HistoryItem> getSelectableListLayout() {
        return mSelectableListLayout;
    }

    private void openItemsInNewTabs(List<HistoryItem> items, boolean isIncognito) {
        recordUserActionWithOptionalSearch("OpenSelected" + (isIncognito ? "Incognito" : ""));

        for (HistoryItem item : items) {
            openUrl(item.getUrl(), isIncognito, true);
            recordOpenedItemMetrics(item);
        }
    }

    /**
     * Sets a {@link HistoryProvider} that is used in place of a real one.
     */
    @VisibleForTesting
    public static void setProviderForTests(HistoryProvider provider) {
        sProviderForTests = provider;
    }

    @VisibleForTesting
    SelectionDelegate<HistoryItem> getSelectionDelegateForTests() {
        return mSelectionDelegate;
    }

    @VisibleForTesting
    HistoryManagerToolbar getToolbarForTests() {
        return mToolbar;
    }

    @VisibleForTesting
    public HistoryAdapter getAdapterForTests() {
        return mHistoryAdapter;
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

    /**
     * Records metrics about the age of an opened history |item|.
     * @param item The item that has been opened.
     */
    void recordOpenedItemMetrics(HistoryItem item) {
        int ageInDays = 1
                + (int) ((System.currentTimeMillis() - item.getTimestamp())
                        / 1000 /* s/ms */ / 60 /* m/s */ / 60 /* h/m */ / 24 /* d/h */);

        RecordHistogram.recordCustomCountHistogram("HistoryPage.ClickAgeInDays",
                Math.min(ageInDays, UMA_MAX_BUCKET_VALUE), 1, UMA_MAX_BUCKET_VALUE,
                UMA_BUCKET_COUNT);

        if (ageInDays <= UMA_MAX_SUBSET_BUCKET_VALUE) {
            RecordHistogram.recordCustomCountHistogram("HistoryPage.ClickAgeInDaysSubset",
                    ageInDays, 1, UMA_MAX_SUBSET_BUCKET_VALUE, UMA_BUCKET_COUNT);
        }
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
        LinearLayoutManager layoutManager = (LinearLayoutManager) mRecyclerView.getLayoutManager();
        // Before the RecyclerView binds its items, LinearLayoutManager#firstVisibleItemPosition()
        // returns {@link RecyclerView#NO_POSITION}. If #findVisibleItemPosition() returns
        // NO_POSITION, the current adapter position should not prevent the info button from being
        // displayed if all of the other criteria is met. See crbug.com/756249#c3.
        boolean firstAdapterItemScrolledOff = layoutManager.findFirstVisibleItemPosition() > 0;

        return !firstAdapterItemScrolledOff && mHistoryAdapter.hasPrivacyDisclaimers()
                && mHistoryAdapter.getItemCount() > 0 && !mToolbar.isSearching()
                && !mSelectionDelegate.isSelectionEnabled();
    }

    /**
     * Called to notify when privacy disclaimers visibility has changed.
     */
    void onHasPrivacyDisclaimersChanged() {
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());
    }

    /**
     * @return True if the available privacy disclaimers should be shown.
     * Note that this may return true even if there are currently no privacy disclaimers.
     */
    boolean shouldShowInfoHeaderIfAvailable() {
        return mShouldShowInfoHeader;
    }

    /**
     * Check if we want to enable the scrolling to load for recycled view. Noting this function
     * will be called during testing with RecycledView == null. Will return False in such case.
     * @return True if accessibility is enabled or a hardware keyboard is attached.
     */
    boolean isScrollToLoadDisabled() {
        if (sIsScrollToLoadDisabledForTests != null) {
            return sIsScrollToLoadDisabledForTests.booleanValue();
        }

        return mIsScrollToLoadDisabled;
    }

    @Override
    public void onSignedIn() {
        mToolbar.onSignInStateChange();
        mHistoryAdapter.onSignInStateChange();
    }

    @Override
    public void onSignedOut() {
        mToolbar.onSignInStateChange();
        mHistoryAdapter.onSignInStateChange();
    }

    @Override
    public void onPreferenceChange() {
        mToolbar.onSignInStateChange();
        mHistoryAdapter.onSignInStateChange();
    }

    @Override
    public void onSelectionStateChange(List<HistoryItem> selectedItems) {
        mHistoryAdapter.onSelectionStateChange(mSelectionDelegate.isSelectionEnabled());
    }

    @Override
    public void onAction(Object actionData) {
        // Handler for the link copied snackbar. Do nothing.
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        // Handler for the link copied snackbar. Do nothing.
    }

    @VisibleForTesting
    TextView getEmptyViewForTests() {
        return mEmptyView;
    }

    @VisibleForTesting
    public RecyclerView getRecyclerViewForTests() {
        return mRecyclerView;
    }

    @VisibleForTesting
    public static void setScrollToLoadDisabledForTesting(boolean isScrollToLoadDisabled) {
        sIsScrollToLoadDisabledForTests = isScrollToLoadDisabled;
    }
}
