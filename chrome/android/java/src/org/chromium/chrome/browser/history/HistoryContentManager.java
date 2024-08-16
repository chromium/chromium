// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static android.content.Intent.ACTION_VIEW;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.os.SystemClock;
import android.provider.Browser;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.history.AppFilterCoordinator.AppInfo;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar.PrefObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemViewHolder;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;

/** Displays and manages the content view / list UI for browsing history. */
public class HistoryContentManager implements SignInStateObserver, PrefObserver {
    /** Interface for a class that wants to receive updates from this Manager. */
    public interface Observer {
        /**
         * Called after the content view was scrolled.
         * @param loadedMore A boolean indicating if more items were loaded.
         */
        void onScrolledCallback(boolean loadedMore);

        /**
         * Called after a user clicked on a HistoryItem.
         * @param item The item that has been clicked.
         */
        void onItemClicked(HistoryItem item);

        /**
         * Called after a user removed this HistoryItem.
         * @param item The item that has been removed.
         */
        void onItemRemoved(HistoryItem item);

        /** Called after a user clicks the clear data button. */
        void onClearBrowsingDataClicked();

        /** Called to notify when the privacy disclaimer visibility has changed. */
        void onPrivacyDisclaimerHasChanged();

        /** Called after a user clicks the open full Chrome history button. */
        void onOpenFullChromeHistoryClicked();

        /** Called to notify when the user's sign in or pref state has changed. */
        void onUserAccountStateChanged();

        /**
         * Called when history has been deleted by some party external to the currently visible
         * history UI, e.g. via "clear browsing data."
         */
        void onHistoryDeletedExternally();
    }

    private static final int FAVICON_MAX_CACHE_SIZE_BYTES =
            10 * ConversionUtils.BYTES_PER_MEGABYTE; // 10MB
    // PageTransition value to use for all URL requests triggered by the history page.
    static final int PAGE_TRANSITION_TYPE = PageTransition.AUTO_BOOKMARK;

    private static HistoryProvider sProviderForTests;
    private static Boolean sIsScrollToLoadDisabledForTests;

    private final Activity mActivity;
    private final Observer mObserver;
    private final boolean mIsSeparateActivity;
    private final boolean mIsIncognito;
    private final Profile mProfile;
    private final boolean mIsScrollToLoadDisabled;
    private final boolean mShouldShowClearDataIfAvailable;
    private final HistoryUmaRecorder mUmaRecorder;
    private final String mHostName;
    private final Runnable mHideSoftKeyboard;
    private final boolean mShowAppFilter;
    private final List<AppInfo> mAppInfoList = new ArrayList<>();
    private final Supplier<BottomSheetController> mBottomSheetController;
    private final Supplier<Tab> mTabSupplier;
    private final AppInfoCache mAppInfoCache;
    private HistoryAdapter mHistoryAdapter;
    private RecyclerView mRecyclerView;
    private LargeIconBridge mLargeIconBridge;
    private SelectionDelegate<HistoryItem> mSelectionDelegate;
    private boolean mShouldShowPrivacyDisclaimers;
    private boolean mLaunchedForApp;
    private PrefChangeRegistrar mPrefChangeRegistrar;
    private String mAppId;
    private AppFilterCoordinator mAppFilterSheet;
    private AppInfo mCurrentApp;
    private long mAppQueryStartMs;

    /**
     * Creates a new HistoryContentManager.
     *
     * @param activity The Activity associated with the HistoryContentManager.
     * @param observer The Observer to receive updates from this manager.
     * @param isSeparateActivity Whether the history UI will be shown in a separate activity than
     *     the main Chrome activity.
     * @param profile The Profile associated with this history.
     * @param shouldShowPrivacyDisclaimers Whether the privacy disclaimers should be shown, if
     *     available.
     * @param shouldShowClearDataIfAvailable Whether the the clear history data button should be
     *     shown, if available.
     * @param hostName The hostName to retrieve history entries for, or null for all hosts.
     * @param selectionDelegate A class responsible for handling list item selection, null for
     *     unselectable items.
     * @param tabSupplier Supplies the current tab, null if the history UI will be shown in a
     *     separate activity. separate activity.
     * @param umaRecorder Records UMA user action/histograms.
     * @param historyProvider Provider of methods for querying and managing browsing history.
     * @param appId The ID of the application from which the history activity is launched, passed as
     *     the client package name.
     * @param launchedForApp Whether history UI is launched for app-specific history.
     */
    public HistoryContentManager(
            @NonNull Activity activity,
            @NonNull Observer observer,
            boolean isSeparateActivity,
            Profile profile,
            boolean shouldShowPrivacyDisclaimers,
            boolean shouldShowClearDataIfAvailable,
            @Nullable String hostName,
            @Nullable SelectionDelegate<HistoryItem> selectionDelegate,
            @Nullable Supplier<BottomSheetController> bottomSheetController,
            @Nullable Supplier<Tab> tabSupplier,
            @Nullable Runnable hideSoftKeyboard,
            HistoryUmaRecorder umaRecorder,
            HistoryProvider historyProvider,
            String appId,
            boolean launchedForApp,
            boolean showAppFilter) {
        mActivity = activity;
        mObserver = observer;
        mIsSeparateActivity = isSeparateActivity;
        mIsIncognito = profile.isOffTheRecord();
        mProfile = profile;
        mBottomSheetController = bottomSheetController;
        mHideSoftKeyboard = hideSoftKeyboard;
        mShowAppFilter = showAppFilter;
        mShouldShowPrivacyDisclaimers = shouldShowPrivacyDisclaimers;
        mShouldShowClearDataIfAvailable = shouldShowClearDataIfAvailable;
        mHostName = hostName;
        mUmaRecorder = umaRecorder;
        mIsScrollToLoadDisabled =
                ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                        || UiUtils.isHardwareKeyboardAttached();
        mAppId = appId;
        mLaunchedForApp = launchedForApp;
        mSelectionDelegate =
                selectionDelegate != null
                        ? selectionDelegate
                        : new SelectionDelegate<HistoryItem>() {
                            @Override
                            public boolean toggleSelectionForItem(HistoryItem bookmark) {
                                return false;
                            }

                            @Override
                            public boolean isItemSelected(HistoryItem item) {
                                return false;
                            }

                            @Override
                            public boolean isSelectionEnabled() {
                                return false;
                            }
                        };
        mTabSupplier = tabSupplier;

        // History service is not keyed for Incognito profiles and {@link HistoryServiceFactory}
        // explicitly redirects to use regular profile for Incognito case.
        mHistoryAdapter =
                new HistoryAdapter(
                        this, sProviderForTests != null ? sProviderForTests : historyProvider);

        // Create a recycler view.
        mRecyclerView =
                new RecyclerView(new ContextThemeWrapper(mActivity, R.style.VerticalRecyclerView));
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mActivity));
        mRecyclerView.setAdapter(mHistoryAdapter);
        mRecyclerView.setHasFixedSize(true);

        // Create icon bridge to get icons for each entry.
        mLargeIconBridge = new LargeIconBridge(profile);
        ActivityManager activityManager =
                ((ActivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize =
                Math.min(
                        (activityManager.getMemoryClass() / 4) * ConversionUtils.BYTES_PER_MEGABYTE,
                        FAVICON_MAX_CACHE_SIZE_BYTES);
        mLargeIconBridge.createCache(maxSize);

        // Add the scroll listener for the recycler view.
        mRecyclerView.addOnScrollListener(
                new OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        LinearLayoutManager layoutManager =
                                (LinearLayoutManager) recyclerView.getLayoutManager();

                        if (!mHistoryAdapter.canLoadMoreItems() || isScrollToLoadDisabled()) {
                            return;
                        }

                        // Load more items if the scroll position is close to the bottom of the
                        // list.
                        boolean loadedMore = false;
                        if (layoutManager.findLastVisibleItemPosition()
                                > (mHistoryAdapter.getItemCount() - 25)) {
                            mHistoryAdapter.loadMoreItems();
                            loadedMore = true;
                        }
                        mObserver.onScrolledCallback(loadedMore);
                    }
                });

        // Create header and footer.
        mHistoryAdapter.generateHeaderItems();
        mHistoryAdapter.generateFooterItems();

        // Listen to changes in sign in state.
        IdentityServicesProvider.get().getSigninManager(profile).addSignInStateObserver(this);

        // Create PrefChangeRegistrar to receive notifications on preference changes.
        mPrefChangeRegistrar = new PrefChangeRegistrar();
        mPrefChangeRegistrar.addObserver(Pref.ALLOW_DELETING_BROWSER_HISTORY, this);
        mPrefChangeRegistrar.addObserver(Pref.INCOGNITO_MODE_AVAILABILITY, this);

        mAppInfoCache = new AppInfoCache(mActivity.getPackageManager());
    }

    /** Tell the HistoryContentManager to start loading items. */
    public void startLoadingItems() {
        // Filtering the adapter to only the results from this particular host.
        mHistoryAdapter.setHostName(mHostName);
        mHistoryAdapter.setAppId(mAppId);
        mHistoryAdapter.startLoadingItems();
    }

    /** Query all app IDs from the history database if required. */
    void maybeQueryApps() {
        if (!showAppFilter()) return;

        mAppQueryStartMs = SystemClock.elapsedRealtime();
        mHistoryAdapter.queryApps();
    }

    void onQueryAppsComplete(List<String> items) {
        mUmaRecorder.recordQueryAppDuration(SystemClock.elapsedRealtime() - mAppQueryStartMs);
        buildAppInfoList(items);
    }

    /**
     * Build a list of {@link AppInfo} using the app query result.
     *
     * @param appIds List of app IDs found from the history database.
     */
    private void buildAppInfoList(List<String> appIds) {
        mAppInfoList.clear();
        for (String appId : appIds) {
            AppInfo appInfo = mAppInfoCache.get(appId);
            // Filter out the app whose info cannot be found. TODO: Consider keeping it with
            // a default app.
            if (appInfo.isValid()) mAppInfoList.add(appInfo);
        }
    }

    /**
     * @return Whether there is apps to show in the filter UI. Could be false if the query is not
     *     completed or the result indeed is empty.
     */
    boolean hasFilterList() {
        return !mAppInfoList.isEmpty();
    }

    /**
     * @return The RecyclerView for this HistoryContentManager.
     */
    public RecyclerView getRecyclerView() {
        return mRecyclerView;
    }

    /** @return The HistoryAdapter for this HistoryContentManager. */
    public HistoryAdapter getAdapter() {
        return mHistoryAdapter;
    }

    /** @return The Context for the associated history view. */
    public Context getContext() {
        return mActivity;
    }

    /** Called when the activity/native page is destroyed. */
    public void onDestroyed() {
        mHistoryAdapter.onDestroyed();
        mLargeIconBridge.destroy();
        mLargeIconBridge = null;
        IdentityServicesProvider.get().getSigninManager(mProfile).removeSignInStateObserver(this);
        mPrefChangeRegistrar.destroy();
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

    /** @return The ViewHolder for the HistoryItem. */
    ViewHolder getHistoryItemViewHolder(View v) {
        return new SelectableItemViewHolder<>(v, mSelectionDelegate);
    }

    /** Binds the ViewHolder with the given HistoryItem. */
    public void bindViewHolderForHistoryItem(ViewHolder holder, HistoryItem item) {
        item.setHistoryManager(this);
        SelectableItemViewHolder<HistoryItem> selectableHolder =
                (SelectableItemViewHolder<HistoryItem>) holder;
        selectableHolder.displayItem(item);
    }

    /**
     * @return Whether to show the remove button in a HistoryItemView.
     */
    int getRemoveItemButtonVisibility() {
        if (!UserPrefs.get(mProfile).getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)) {
            return View.GONE;
        }
        return !mSelectionDelegate.isSelectionEnabled() ? View.VISIBLE : View.INVISIBLE;
    }

    /**
     * Called to notify the content manager that history has been deleted by some party external to
     * the currently visible history UI, e.g. via "clear browsing data."
     */
    void onHistoryDeletedExternally() {
        mSelectionDelegate.clearSelection();
        mObserver.onHistoryDeletedExternally();
    }

    /**
     * Sets the selectable item mode. Items only selectable if they have a SelectableItemViewHolder.
     * @param active Whether the selection mode is on or not.
     */
    public void setSelectionActive(boolean active) {
        mHistoryAdapter.setSelectionActive(active);
    }

    /** @param shouldShow Whether the privacy disclaimers should be shown, if available. */
    public void updatePrivacyDisclaimers(boolean shouldShow) {
        mShouldShowPrivacyDisclaimers = shouldShow;
        mHistoryAdapter.setPrivacyDisclaimer();
    }

    /**
     * @return True if the available privacy disclaimers should be shown.
     * Note that this may return true even if there are currently no privacy disclaimers.
     */
    boolean getShouldShowPrivacyDisclaimersIfAvailable() {
        return mShouldShowPrivacyDisclaimers;
    }

    /** Called to notify when the privacy disclaimer visibility has changed. */
    void onPrivacyDisclaimerHasChanged() {
        mObserver.onPrivacyDisclaimerHasChanged();
    }

    /** @return True if any privacy disclaimer should be visible, false otherwise. */
    boolean hasPrivacyDisclaimers() {
        return mHistoryAdapter.hasPrivacyDisclaimers();
    }

    /**
     * @return True if history UI was launched for the app-specific mode.
     */
    boolean launchedForApp() {
        return HistoryManager.isAppSpecificHistoryEnabled() && mLaunchedForApp;
    }

    /**
     * @return True if history page needs to show app filter UI.
     */
    boolean showAppFilter() {
        return HistoryManager.isAppSpecificHistoryEnabled() && mShowAppFilter;
    }

    /** returns whether the info header will be available for user upon request. */
    boolean isInfoHeaderAvailable() {
        // Info header becomes available when history was launched for app-specific mode
        // or a set of conditions for privacy disclaimer are all met.
        return launchedForApp() || hasPrivacyDisclaimers();
    }

    /** Called after a user clicks the privacy disclaimer link. */
    void onPrivacyDisclaimerLinkClicked() {
        openUrl(new GURL(UrlConstants.MY_ACTIVITY_URL_IN_HISTORY), null, true);
    }

    /**
     * @return True if the clear history data button should be shown.
     */
    boolean getShouldShowClearData() {
        return mShouldShowClearDataIfAvailable
                && UserPrefs.get(mProfile).getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY);
    }

    /** Opens the url of each of the visits in the provided list in a new tab. */
    public void openItemsInNewTab(List<HistoryItem> items, boolean isIncognito) {
        if (mIsSeparateActivity && items.size() > 1) {
            ArrayList<String> additionalUrls = new ArrayList<>(items.size() - 1);
            for (int i = 1; i < items.size(); i++) {
                additionalUrls.add(items.get(i).getUrl().getSpec());
            }

            Intent intent = getOpenUrlIntent(items.get(0).getUrl(), isIncognito, true);
            intent.putExtra(IntentHandler.EXTRA_ADDITIONAL_URLS, additionalUrls);
            IntentHandler.startActivityForTrustedIntent(intent);
        } else {
            for (HistoryItem item : items) {
                openUrl(item.getUrl(), isIncognito, true);
            }
        }
    }

    /**
     * Open the provided url.
     *
     * @param url The url to open.
     * @param isIncognito Whether to open the url in an incognito tab. If null, the tab will open in
     *     the current tab model.
     * @param createNewTab Whether a new tab should be created. If false, the item will clobber the
     *     the current tab.
     */
    public void openUrl(GURL url, Boolean isIncognito, boolean createNewTab) {
        if (mIsSeparateActivity) {
            // Only history entries are loaded into the existing tab.
            if (launchedForApp() && !createNewTab) {
                Intent intent = new Intent(ACTION_VIEW, Uri.parse(url.getSpec()));
                intent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PAGE_TRANSITION_TYPE);
                mActivity.setResult(Activity.RESULT_OK, intent);
                mActivity.finish();
            } else {
                IntentHandler.startActivityForTrustedIntent(
                        getOpenUrlIntent(url, isIncognito, createNewTab));
            }

            return;
        }

        assert mTabSupplier != null;
        Tab tab = mTabSupplier.get();
        assert tab != null;

        if (createNewTab) {
            new ChromeAsyncTabLauncher(isIncognito != null ? isIncognito : mIsIncognito)
                    .launchNewTab(
                            new LoadUrlParams(url, PAGE_TRANSITION_TYPE),
                            TabLaunchType.FROM_LINK,
                            tab);
        } else {
            tab.loadUrl(new LoadUrlParams(url, PAGE_TRANSITION_TYPE));
        }
    }

    Intent getOpenUrlIntent(GURL url, Boolean isIncognito, boolean createNewTab) {
        // Construct basic intent.
        Intent viewIntent = createOpenUrlIntent(url, mActivity);

        // Set other intent extras.
        if (isIncognito != null) {
            viewIntent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, isIncognito);
        }
        if (createNewTab) viewIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);

        return viewIntent;
    }

    /** @return Whether this class is displaying history for the incognito profile. */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    /** @return The total number of history items in the content view. */
    public int getItemCount() {
        return mHistoryAdapter.getItemCount();
    }

    /**
     * Adds the HistoryItem to the list of items being removed and removes it from the adapter. The
     * removal will not be committed until #removeItems() is called.
     * @param item The item to mark for removal.
     */
    public void markItemForRemoval(HistoryItem item) {
        mHistoryAdapter.markItemForRemoval(item);
    }

    /** Removes all items that have been marked for removal through #markItemForRemoval(). */
    public void removeItems() {
        mHistoryAdapter.removeItems();
    }

    /**
     * Called after a user removes this HistoryItem.
     * @param item The item that has been removed.
     */
    public void onItemRemoved(HistoryItem item) {
        mHistoryAdapter.markItemForRemoval(item);
        mHistoryAdapter.removeItems();
        announceItemRemoved(item);
        mObserver.onItemRemoved(item);
    }

    void announceItemRemoved(HistoryItem item) {
        mRecyclerView.announceForAccessibility(
                mActivity.getString(R.string.delete_message, item.getTitle()));
    }

    void maybeResetAppFilterChip() {
        if (showAppFilter()) mHistoryAdapter.resetAppFilterChip();
    }

    /**
     * Called to perform a search.
     *
     * @param query The text to search for.
     */
    public void search(String query) {
        mHistoryAdapter.search(query);
    }

    /** Called when a search is ended. */
    public void onEndSearch() {
        mCurrentApp = null;
        mHistoryAdapter.onEndSearch();
    }

    /**
     * Called after a user clicks this HistoryItem.
     * @param item The item that has been clicked.
     */
    public void onItemClicked(HistoryItem item) {
        mObserver.onItemClicked(item);
        openUrl(item.getUrl(), null, false);
    }

    /** @return The {@link LargeIconBridge} used to fetch large favicons. */
    public LargeIconBridge getLargeIconBridge() {
        return mLargeIconBridge;
    }

    /** Called after a user clicks the clear data button. */
    void onClearBrowsingDataClicked() {
        mObserver.onClearBrowsingDataClicked();
    }

    /** Called after a user clicks the open full Chrome history button. */
    void onOpenFullChromeHistoryClicked() {
        mObserver.onOpenFullChromeHistoryClicked();
    }

    /** Called after a user clicks the filter by app button. */
    void onAppFilterClicked() {
        // Search mode starts with the soft keyboard open. Hide it first for the sheet
        // to appear at the bottom as expected.
        mHideSoftKeyboard.run();
        if (mAppFilterSheet == null) {
            mAppFilterSheet =
                    new AppFilterCoordinator(
                            mActivity,
                            mActivity.getWindow().getDecorView(),
                            mBottomSheetController.get(),
                            this::onAppUpdated,
                            mAppInfoList);
        }
        mAppFilterSheet.openSheet(mCurrentApp);
        mUmaRecorder.recordAppFilterSheetOpened();
    }

    /** Callback from app filter sheet, with the newly chosen app to filter. */
    @VisibleForTesting
    void onAppUpdated(@Nullable AppInfo appInfo) {
        if (Objects.equals(mCurrentApp, appInfo)) return;
        mCurrentApp = appInfo;
        getAdapter().updateHistory(mCurrentApp);
    }

    /** Removes the list header. */
    public void removeHeader() {
        mHistoryAdapter.removeHeader();
    }

    @Override
    public void onSignedIn() {
        mObserver.onUserAccountStateChanged();
        mHistoryAdapter.onSignInStateChange();
    }

    @Override
    public void onSignedOut() {
        mObserver.onUserAccountStateChanged();
        mHistoryAdapter.onSignInStateChange();
    }

    @Override
    public void onPreferenceChange() {
        mObserver.onUserAccountStateChanged();
        mHistoryAdapter.onSignInStateChange();
    }

    /**
     * Creates a view intent for opening the given url that is trusted and targets the correct main
     * browsing activity.
     */
    static Intent createOpenUrlIntent(GURL url, Activity activity) {
        Intent viewIntent = new Intent(ACTION_VIEW, Uri.parse(url.getSpec()));
        viewIntent.putExtra(
                Browser.EXTRA_APPLICATION_ID, activity.getApplicationContext().getPackageName());
        viewIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        viewIntent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PAGE_TRANSITION_TYPE);
        // Determine component or class name.
        ComponentName component;
        if (activity instanceof HistoryActivity) { // phone
            component =
                    IntentUtils.safeGetParcelableExtra(
                            activity.getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT);
        } else { // tablet
            component = activity.getComponentName();
        }
        if (component != null) {
            ActivityUtils.setNonAliasedComponentForMainBrowsingActivity(viewIntent, component);
        } else {
            viewIntent.setClass(activity, ChromeLauncherActivity.class);
        }
        IntentUtils.addTrustedIntentExtras(viewIntent);
        return viewIntent;
    }

    static class AppInfoCache {
        private static final AppInfo EMPTY_INFO = new AppInfo(null, null, null);
        private HashMap<String, AppInfo> mAppInfoMap;
        private PackageManager mPackageManager;

        public AppInfoCache(PackageManager packageManager) {
            mPackageManager = packageManager;
        }

        public AppInfo get(String appId) {
            assert appId != null;
            if (mAppInfoMap == null) mAppInfoMap = new HashMap<String, AppInfo>();
            AppInfo appInfo = mAppInfoMap.get(appId);
            if (appInfo == null) {
                try {
                    PackageManager pm = mPackageManager;
                    var info = pm.getApplicationInfo(appId, PackageManager.GET_META_DATA);
                    var icon = pm.getApplicationIcon(info);
                    var label = pm.getApplicationLabel(info);
                    appInfo = new AppInfo(appId, icon, label);
                } catch (NameNotFoundException e) {
                    // Can happen if the corresponding app was uninstalled, or unavailable for any
                    // reason. Map it with an empty info so it won't be queried again till next
                    // time history UI is launched.
                    appInfo = EMPTY_INFO;
                }
                mAppInfoMap.put(appId, appInfo);
            }
            return appInfo;
        }

        void setPackageManagerForTesting(PackageManager packageManager) {
            mPackageManager = packageManager;
        }
    }

    AppInfoCache getAppInfoCache() {
        return mAppInfoCache;
    }

    /**
     * @param provider The {@link HistoryProvider} that is used in place of a real one.
     */
    public static void setProviderForTests(HistoryProvider provider) {
        sProviderForTests = provider;
        ResettersForTesting.register(() -> sProviderForTests = null);
    }

    /** @param isScrollToLoadDisabled Whether scrolling to load is disabled for tests. */
    public static void setScrollToLoadDisabledForTesting(boolean isScrollToLoadDisabled) {
        sIsScrollToLoadDisabledForTests = isScrollToLoadDisabled;
        ResettersForTesting.register(() -> sIsScrollToLoadDisabledForTests = null);
    }

    void setPackageManagerForTesting(PackageManager packageManager) {
        mAppInfoCache.setPackageManagerForTesting(packageManager); // IN-TEST
    }

    void setAppFilterSheetForTesting(AppFilterCoordinator appFilterSheet) {
        mAppFilterSheet = appFilterSheet;
    }

    AppInfo getAppInfoForTesting() {
        return mCurrentApp;
    }
}
