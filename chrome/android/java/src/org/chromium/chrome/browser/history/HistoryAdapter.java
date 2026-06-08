// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.finds.FindsFeatures;
import org.chromium.chrome.browser.finds.FindsUtils;
import org.chromium.chrome.browser.history.AppFilterCoordinator.AppInfo;
import org.chromium.chrome.browser.history.HistoryProvider.BrowsingHistoryObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.components.browser_ui.widget.DateDividedAdapter;
import org.chromium.components.browser_ui.widget.MoreProgressButton;
import org.chromium.components.browser_ui.widget.MoreProgressButton.State;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.ArrayList;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Bridges the user's browsing history and the UI used to display it. */
@NullMarked
public class HistoryAdapter extends DateDividedAdapter implements BrowsingHistoryObserver {
    private static final String EMPTY_QUERY = "";
    private static final String CLUSTER_EXPANSION_KEY_PREFIX = "cluster_";

    private final HistoryContentManager mManager;
    private final ArrayList<HistoryItemView> mItemViews;
    private final DefaultFaviconHelper mFaviconHelper;
    private final boolean mShowAppFilter;
    private @Nullable final SigninPromoCoordinator mHistorySyncPromoCoordinator;
    private @Nullable final SnackbarManager mSnackbarManager;
    private @Nullable final Profile mProfile;

    private @Nullable RecyclerView mRecyclerView;
    private HistoryProvider mHistoryProvider;

    // Headers
    private TextView mPrivacyDisclaimerTextView;
    private View mPrivacyDisclaimerBottomSpace;
    private @Nullable Button mClearBrowsingDataButton;
    private @Nullable HeaderItem mPrivacyDisclaimerHeaderItem;
    private @Nullable HeaderItem mClearBrowsingDataButtonHeaderItem;
    private @Nullable HeaderItem mHistoryOpenInChromeHeaderItem;
    private @Nullable HeaderItem mHistorySyncPromoHeaderItem;
    private @Nullable HeaderItem mAppFilterHeaderItem;
    private @Nullable HeaderItem mFindsPromoHeaderItem;
    private ChipView mAppFilterChip;

    // Footers
    private MoreProgressButton mMoreProgressButton;
    private FooterItem mMoreProgressButtonFooterItem;

    private boolean mHasOtherFormsOfBrowsingData;
    private boolean mIsDestroyed;
    private boolean mAreHeadersInitialized;
    private boolean mIsLoadingItems;
    private boolean mIsSearching;
    private boolean mHasMorePotentialItems;
    private boolean mClearOnNextQueryComplete;
    private boolean mPrivacyDisclaimersVisible;
    private boolean mClearBrowsingDataButtonVisible;
    private boolean mHistorySyncPromoVisible;
    private boolean mFindsPromoVisible;
    private boolean mFindsPromoShowEligible;
    private String mQueryText = EMPTY_QUERY;
    private @Nullable String mHostName;

    // ID of the App currently chosen for app filtering. If null, ignored when querying history.
    private @Nullable String mAppId;
    private boolean mDisableScrollToLoadForTest;

    // Whether we show the source app for each entry. We show it in BrApp in full history UI, but
    // not in search mode when app filter is in effect.
    private boolean mShowSourceApp;

    private boolean mIsLargeFormFactorDevice;
    private final boolean mShouldClusterByDomain;
    private final Set<String> mExpandedClusterKeys = new HashSet<>();
    private final List<HistoryItem> mAllItems = new ArrayList<>();
    // Monotonically increasing ID for clustering.
    private long mNextClusterId = 1;

    /**
     * Creates a new instance of {@link HistoryAdapter}.
     *
     * @param manager The manager for history content.
     * @param provider The provider for history data.
     * @param historySyncPromoCoordinator The coordinator for the history sync promo, or null.
     * @param shouldClusterByDomain Whether history items should be clustered by domain.
     * @param snackbarManager The manager for snackbars, or null.
     * @param profile The current user profile, or null if off the record.
     */
    public HistoryAdapter(
            HistoryContentManager manager,
            HistoryProvider provider,
            @Nullable SigninPromoCoordinator historySyncPromoCoordinator,
            boolean shouldClusterByDomain,
            @Nullable SnackbarManager snackbarManager,
            @Nullable Profile profile) {
        setHasStableIds(true);
        mHistoryProvider = provider;
        mHistoryProvider.setObserver(this);
        mManager = manager;
        mSnackbarManager = snackbarManager;
        mProfile = profile;
        mFaviconHelper = new DefaultFaviconHelper();
        mItemViews = new ArrayList<>();
        mShowAppFilter = mManager.showAppFilter();
        mShowSourceApp = mShowAppFilter; // defaults to BrApp full history
        mHistorySyncPromoCoordinator = historySyncPromoCoordinator;
        mIsLargeFormFactorDevice = false;
        mShouldClusterByDomain = shouldClusterByDomain;
    }

    /** Called when the activity/native page is destroyed. */
    @SuppressWarnings("NullAway")
    public void onDestroyed() {
        mIsDestroyed = true;

        mHistoryProvider.destroy();
        mHistoryProvider = null;

        mRecyclerView = null;
        mFaviconHelper.clearCache();
    }

    /** Starts loading the first set of browsing history items. */
    public void startLoadingItems() {
        mAreHeadersInitialized = false;
        mIsLoadingItems = true;
        mClearOnNextQueryComplete = true;
        if (mHostName != null) {
            mHistoryProvider.queryHistoryForHost(mHostName);
        } else {
            mHistoryProvider.queryHistory(mQueryText, mAppId);
        }
    }

    void onSearchStart() {
        mIsSearching = true;
        setHeaders();
    }

    void queryApps() {
        mHistoryProvider.queryApps();
    }

    @Override
    public void onAttachedToRecyclerView(RecyclerView recyclerView) {
        // This adapter should only ever be attached to one RecyclerView.
        assert mRecyclerView == null;

        mRecyclerView = recyclerView;
    }

    @Override
    public void onDetachedFromRecyclerView(RecyclerView recyclerView) {
        mRecyclerView = null;
    }

    /**
     * Load more browsing history items. Returns early if more items are already being loaded or
     * there are no more items to load.
     */
    public void loadMoreItems() {
        if (!canLoadMoreItems()) {
            return;
        }
        mIsLoadingItems = true;
        updateFooter();
        notifyDataSetChanged();
        mHistoryProvider.queryHistoryContinuation();
    }

    /**
     * @return Whether more items can be loaded right now.
     */
    public boolean canLoadMoreItems() {
        return !mIsLoadingItems && mHasMorePotentialItems;
    }

    /**
     * Called to perform a search.
     *
     * @param query The text to search for.
     */
    public void search(String query) {
        mQueryText = query;
        onSearchStart();
        mClearOnNextQueryComplete = true;
        mHistoryProvider.queryHistory(mQueryText, mAppId);
    }

    /** Called when a search is ended. */
    public void onEndSearch() {
        mQueryText = EMPTY_QUERY;
        mIsSearching = false;
        if (mShowAppFilter) setAppId(null);
        mShowSourceApp = mShowAppFilter;

        // Re-initialize the data in the adapter.
        startLoadingItems();
    }

    /**
     * Adds the HistoryItem to the list of items being removed and removes it from the adapter. The
     * removal will not be committed until #removeItems() is called.
     *
     * @param item The item to mark for removal.
     */
    public void markItemForRemoval(HistoryItem item) {
        mAllItems.remove(item);
        if (item.isClusterHead() && item.getSubItems() != null) {
            mAllItems.removeAll(item.getSubItems());
            for (HistoryItem subItem : item.getSubItems()) {
                mHistoryProvider.markItemForRemoval(subItem);
            }
        } else {
            mHistoryProvider.markItemForRemoval(item);
        }
        rebuildItemList();
    }

    /** Removes all items that have been marked for removal through #markItemForRemoval(). */
    public void removeItems() {
        mHistoryProvider.removeItems();

        // Removing app-specific entries could require refreshing the apps list.
        mManager.maybeQueryApps();
    }

    /** Should be called when the user's sign in state changes. */
    public void onSignInStateChange() {
        int visibility = mManager.getRemoveItemButtonVisibility();
        for (HistoryItemView itemView : mItemViews) {
            itemView.setRemoveButtonVisiblity(visibility);
        }
        startLoadingItems();
        updateClearBrowsingDataButtonVisibility();
        updatePrivacyDisclaimerBottomSpace();
    }

    /**
     * Sets the selectable item mode. Items only selectable if they have a SelectableItemViewHolder.
     * @param active Whether the selection mode is on or not.
     */
    public void setSelectionActive(boolean active) {
        if (mClearBrowsingDataButton != null) {
            mClearBrowsingDataButton.setEnabled(!active);
        }

        // While the selection is active, we temporarily disable the app filter button.
        if (mShowAppFilter) mAppFilterChip.setEnabled(!active);

        int visibility = mManager.getRemoveItemButtonVisibility();
        if (active) {
            assert visibility != View.VISIBLE : "Removal is not allowed when selection is active";
        }
        for (HistoryItemView item : mItemViews) {
            item.setRemoveButtonVisiblity(visibility);
        }
    }

    @Override
    protected ViewHolder createViewHolder(ViewGroup parent) {
        var v =
                (HistoryItemView)
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.history_item_view, parent, false);
        v.initialize(mManager.getAppInfoCache(), () -> mShowSourceApp);
        ViewHolder viewHolder = mManager.getHistoryItemViewHolder(v);
        HistoryItemView itemView = (HistoryItemView) viewHolder.itemView;
        itemView.setFaviconHelper(mFaviconHelper);
        itemView.setRemoveButtonVisiblity(mManager.getRemoveItemButtonVisibility());
        mItemViews.add(itemView);
        return viewHolder;
    }

    @Override
    protected void bindViewHolderForTimedItem(ViewHolder current, TimedItem timedItem) {
        final HistoryItem item = (HistoryItem) timedItem;

        HistoryItemView itemView = (HistoryItemView) current.itemView;
        if (item.isClusterHead()) {
            itemView.setClusterToggleHandler(() -> toggleCluster(item));
        } else {
            itemView.setClusterToggleHandler(null);
        }

        mManager.bindViewHolderForHistoryItem(current, item);
    }

    @Override
    protected int getTimedItemViewResId() {
        return R.layout.date_view;
    }

    @SuppressWarnings("unchecked")
    @Override
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {
        // Return early if the results are returned after the activity/native page is
        // destroyed to avoid unnecessary work.
        if (mIsDestroyed) return;

        removeFooter();

        if (mClearOnNextQueryComplete) {
            mAllItems.clear();
            clear(true);
            mClearOnNextQueryComplete = false;
        }

        mAllItems.addAll(items);

        mIsLoadingItems = false;
        mHasMorePotentialItems = hasMorePotentialMatches;

        rebuildItemList();

        boolean isEmpty = items.size() > 0 || mHistorySyncPromoVisible;
        if ((!mAreHeadersInitialized && isEmpty && !mIsSearching)
                || (mIsSearching && mShowAppFilter)
                || mIsLargeFormFactorDevice) {
            setHeaders();
            mAreHeadersInitialized = true;
        }

        if (mHasMorePotentialItems) updateFooter();
    }

    @Override
    public void onHistoryDeleted() {
        // Return early if this call comes in after the activity/native page is destroyed.
        if (mIsDestroyed) return;

        mManager.onHistoryDeletedExternally();
        // TODO(twellington): Account for items that have been paged in due to infinite scroll.
        //                    This currently removes all items and re-issues a query.
        startLoadingItems();
    }

    @Override
    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {
        mHasOtherFormsOfBrowsingData = hasOtherForms;
        updatePrivacyDisclaimerText();
        setPrivacyDisclaimer();
        mManager.onPrivacyDisclaimerHasChanged();
    }

    @Override
    public void onQueryAppsComplete(List<String> items) {
        mManager.onQueryAppsComplete(items);

        // Querying apps was completed after the search mode is entered (or within search mode).
        // Set the headers again to show/hide the header item for the app filter button.
        if (mIsSearching) {
            setHeaders();
        }
    }

    @Override
    protected BasicViewHolder createFooter(ViewGroup parent) {
        // Create the same frame layout as place holder for more footer items.
        return createHeader(parent);
    }

    /**
     * Initialize a more progress button as footer items that will be re-used
     * during page loading.
     */
    @Initializer
    void generateFooterItems() {
        mMoreProgressButton =
                (MoreProgressButton)
                        View.inflate(mManager.getContext(), R.layout.more_progress_button, null);

        mMoreProgressButton.setOnClickRunnable(this::loadMoreItems);
        mMoreProgressButtonFooterItem = new FooterItem(-1, mMoreProgressButton);
    }

    @Override
    public void addFooter() {
        if (hasListFooter()) return;

        ItemGroup footer = new FooterItemGroup();
        footer.addItem(mMoreProgressButtonFooterItem);

        // When scroll to load is enabled, the footer just added should be set to spinner.
        // When scroll to load is disabled, the footer just added should first display the button.
        if (isScrollToLoadDisabled()) {
            mMoreProgressButton.setState(State.BUTTON);
        } else {
            mMoreProgressButton.setState(State.LOADING);
        }
        addGroup(footer);
    }

    /** Update footer when the content change. */
    private void updateFooter() {
        if (isScrollToLoadDisabled() || mIsLoadingItems) addFooter();
    }

    /**
     * Initialize clear browsing data and privacy disclaimer header views and generate header items
     * for them.
     */
    @Initializer
    void generateHeaderItems() {
        ViewGroup historyAppFilterContainer = getAppFilterContainer(null);
        ViewGroup privacyDisclaimerContainer = getPrivacyDisclaimerContainer(null);
        ViewGroup clearBrowsingDataButtonContainer = getClearBrowsingDataButtonContainer(null);

        mAppFilterHeaderItem = new StandardHeaderItem(0, historyAppFilterContainer);
        mPrivacyDisclaimerHeaderItem = new StandardHeaderItem(0, privacyDisclaimerContainer);
        mPrivacyDisclaimerBottomSpace =
                privacyDisclaimerContainer.findViewById(R.id.privacy_disclaimer_bottom_space);
        mClearBrowsingDataButtonHeaderItem =
                new StandardHeaderItem(1, clearBrowsingDataButtonContainer);
        mClearBrowsingDataButton =
                clearBrowsingDataButtonContainer.findViewById(R.id.clear_browsing_data_button);

        if (mManager.launchedForApp()) {
            ViewGroup historyOpenInChromeButtonContainer = getCctOpenInChromeButtonContainer(null);

            mHistoryOpenInChromeHeaderItem =
                    new StandardHeaderItem(1, historyOpenInChromeButtonContainer);
        }
        if (mHistorySyncPromoCoordinator != null) {
            View historySyncPromoView = getHistorySyncPromoView();

            mHistorySyncPromoHeaderItem = new PersistentHeaderItem(2, historySyncPromoView);
        }

        // Initialize the Finds Opt-In Promo Card with the same position as the History Sync Promo
        // as they are enforced to be mutually exclusive when showing.
        View findsPromoContainer = getFindsPromoContainer();
        mFindsPromoHeaderItem = new StandardHeaderItem(2, findsPromoContainer);

        updateClearBrowsingDataButtonVisibility();
        setPrivacyDisclaimer();
        updatePrivacyDisclaimerBottomSpace();
        updateHistorySyncPromoVisibility();

        // Only attempt to show the Finds promo if the Profile is not offTheRecord (set to be null
        // as a dependency) and if the SnackbarManager is not null as in certain flows such as
        // PageInfo and in the sidebar history page it can be null.
        if (mSnackbarManager != null
                && mProfile != null
                && FindsFeatures.sEnableHistoryPageOptIn.getValue()) {
            checkFindsPromoShowCriteriaAsync(mProfile);
        }
    }

    private ViewGroup getClearBrowsingDataButtonContainer(@Nullable ViewGroup parent) {
        ViewGroup viewGroup =
                (ViewGroup)
                        LayoutInflater.from(mManager.getContext())
                                .inflate(
                                        R.layout.history_clear_browsing_data_header, parent, false);
        Button clearBrowsingDataButton = viewGroup.findViewById(R.id.clear_browsing_data_button);
        clearBrowsingDataButton.setOnClickListener(v -> mManager.onClearBrowsingDataClicked());
        return viewGroup;
    }

    private ViewGroup getCctOpenInChromeButtonContainer(@Nullable ViewGroup parent) {
        ViewGroup viewGroup =
                (ViewGroup)
                        LayoutInflater.from(mManager.getContext())
                                .inflate(R.layout.open_full_chrome_history_header, parent, true);
        Button clearBrowsingDataButton =
                viewGroup.findViewById(R.id.open_full_chrome_history_button);
        clearBrowsingDataButton.setOnClickListener(v -> mManager.onOpenFullChromeHistoryClicked());
        return viewGroup;
    }

    @EnsuresNonNull("mAppFilterChip")
    private ViewGroup getAppFilterContainer(@Nullable ViewGroup parent) {
        ViewGroup historyAppFilterContainer =
                (ViewGroup)
                        LayoutInflater.from(mManager.getContext())
                                .inflate(R.layout.app_history_filter, parent, true);
        mAppFilterChip = historyAppFilterContainer.findViewById(R.id.app_history_filter_chip);
        mAppFilterChip.setOnClickListener(v -> mManager.onAppFilterClicked());
        mAppFilterChip.getPrimaryTextView().setText(R.string.history_filter_by_app);
        mAppFilterChip.addDropdownIcon();
        return historyAppFilterContainer;
    }

    private View getHistorySyncPromoView() {
        assumeNonNull(mHistorySyncPromoCoordinator);
        View promoView = mHistorySyncPromoCoordinator.buildPromoView(null);
        mHistorySyncPromoCoordinator.setView(promoView);
        return promoView;
    }

    void updateHistory(@Nullable AppInfo appInfo) {
        if (appInfo == null) {
            setAppId(null);
            resetAppFilterChip();
            mShowSourceApp = mShowAppFilter;
        } else {
            setAppId(appInfo.id);
            mAppFilterChip.getPrimaryTextView().setText(appInfo.label);
            mAppFilterChip.setSelected(true);
            mAppFilterChip.setIcon(R.drawable.ic_check_googblue_24dp, true);
            mShowSourceApp = false;
        }
        search(EMPTY_QUERY);
    }

    void resetAppFilterChip() {
        mAppFilterChip.getPrimaryTextView().setText(R.string.history_filter_by_app);
        mAppFilterChip.setSelected(false);
        mAppFilterChip.setIcon(ChipView.INVALID_ICON_ID, false);
    }

    private View getFindsPromoContainer() {
        Context context = mManager.getContext();
        View view =
                LayoutInflater.from(context)
                        .inflate(R.layout.finds_promo_view_history_page, null, false);
        Button positiveButton = view.findViewById(R.id.finds_promo_positive_button);
        Button negativeButton = view.findViewById(R.id.finds_promo_negative_button);
        View closeButton = view.findViewById(R.id.finds_promo_close_button);
        positiveButton.setOnClickListener(
                v -> {
                    FindsUtils.acceptOptIn(
                            context,
                            assumeNonNull(mProfile),
                            assumeNonNull(mSnackbarManager),
                            () -> dismissFindsOptInPromo());
                });
        negativeButton.setOnClickListener(v -> dismissFindsOptInPromo());
        closeButton.setOnClickListener(v -> dismissFindsOptInPromo());
        return view;
    }

    private void dismissFindsOptInPromo() {
        if (!mFindsPromoVisible) return;

        mFindsPromoVisible = false;
        setHeaders();

        if (mProfile != null) {
            FindsUtils.setOptInPromoInteracted(mProfile);
        }
    }

    @EnsuresNonNull("mPrivacyDisclaimerTextView")
    ViewGroup getPrivacyDisclaimerContainer(@Nullable ViewGroup parent) {
        Context context = mManager.getContext();
        ViewGroup privacyDisclaimerContainer =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(R.layout.history_privacy_disclaimer_header, parent, false);

        mPrivacyDisclaimerTextView =
                privacyDisclaimerContainer.findViewById(R.id.privacy_disclaimer);
        mPrivacyDisclaimerTextView.setMovementMethod(LinkMovementMethod.getInstance());
        updatePrivacyDisclaimerText();
        return privacyDisclaimerContainer;
    }

    private void updatePrivacyDisclaimerText() {
        Context context = mPrivacyDisclaimerTextView.getContext();
        CharSequence text = null;
        if (!HistoryManager.isAppSpecificHistoryEnabled()) {
            text =
                    getPrivacyDisclaimerClickableSpanString(
                            context, R.string.android_history_other_forms_of_history);
        } else {
            if (mManager.launchedForApp()) { // In-app History UI
                if (hasPrivacyDisclaimers()) {
                    int res = R.string.android_app_history_open_full_other_forms;
                    text = getPrivacyDisclaimerClickableSpanString(context, res);
                } else {
                    text = context.getString(R.string.android_app_history_open_full);
                }
            } else if (mManager.showAppFilter()) { // History UI in BrApp
                if (hasPrivacyDisclaimers()) {
                    int res = R.string.android_history_from_other_apps_other_forms_of_history;
                    text = getPrivacyDisclaimerClickableSpanString(context, res);
                } else {
                    int res = R.string.android_history_from_other_apps;
                    text = context.getString(res);
                }
            }
        }
        if (text != null) mPrivacyDisclaimerTextView.setText(text);
    }

    private void updatePrivacyDisclaimerBottomSpace() {
        boolean hideBottomSpace = mClearBrowsingDataButtonVisible || mManager.launchedForApp();
        mPrivacyDisclaimerBottomSpace.setVisibility(hideBottomSpace ? View.GONE : View.VISIBLE);
    }

    private CharSequence getPrivacyDisclaimerClickableSpanString(
            Context context, @StringRes int resId) {
        var s = context.getString(resId);
        var link =
                new ChromeClickableSpan(context, (v) -> mManager.onPrivacyDisclaimerLinkClicked());
        return SpanApplier.applySpans(s, new SpanApplier.SpanInfo("<link>", "</link>", link));
    }

    /** Pass header items to {@link #setHeaders(HeaderItem...)} as parameters. */
    private void setHeaders() {
        if (mIsLargeFormFactorDevice) {
            setLFFHeaders();
            return;
        }
        ArrayList<HeaderItem> args = new ArrayList<>();
        if (mIsSearching) {
            // Query for apps could be still pending. |setHeaders()| will be invoked
            // again when the query is completed in order to set the header accordingly.
            if (mShowAppFilter && mManager.hasFilterList()) args.add(mAppFilterHeaderItem);
        } else {
            if (mPrivacyDisclaimersVisible) {
                args.add(mPrivacyDisclaimerHeaderItem);
            }
            if (mClearBrowsingDataButtonVisible) {
                args.add(mClearBrowsingDataButtonHeaderItem);
            }
            if (mManager.launchedForApp()) {
                args.add(mHistoryOpenInChromeHeaderItem);
            }
            if (mHistorySyncPromoVisible) {
                args.add(mHistorySyncPromoHeaderItem);
            }
            if (mFindsPromoVisible) {
                args.add(mFindsPromoHeaderItem);
            }
        }
        setHeaders(args.toArray(new HeaderItem[args.size()]));
    }

    /** For LFF devices w/ physical keyboard attached, there's only search mode. */
    private void setLFFHeaders() {
        ArrayList<HeaderItem> args = new ArrayList<>();
        if (mShowAppFilter && mManager.hasFilterList()) args.add(mAppFilterHeaderItem);
        if (isNormalContentAvailable()) {
            if (mPrivacyDisclaimersVisible) {
                args.add(mPrivacyDisclaimerHeaderItem);
            }
            if (mClearBrowsingDataButtonVisible) {
                args.add(mClearBrowsingDataButtonHeaderItem);
            }
        }
        if (mManager.launchedForApp()) {
            args.add(mHistoryOpenInChromeHeaderItem);
        }
        if (mHistorySyncPromoVisible) {
            args.add(mHistorySyncPromoHeaderItem);
        }
        setHeaders(args.toArray(new HeaderItem[args.size()]));
    }

    /**
     * @return True if any privacy disclaimer should be visible, false otherwise.
     */
    boolean hasPrivacyDisclaimers() {
        return !mManager.isIncognito() && mHasOtherFormsOfBrowsingData;
    }

    /**
     * @return True if HistoryManager is not null, and scroll to load is disabled in HistoryManager
     */
    boolean isScrollToLoadDisabled() {
        return mDisableScrollToLoadForTest
                || (mManager != null && mManager.isScrollToLoadDisabled());
    }

    /** Set text of privacy disclaimer and visibility of its container. */
    void setPrivacyDisclaimer() {
        boolean shouldShowPrivacyDisclaimers;
        if (HistoryManager.isAppSpecificHistoryEnabled()) {
            shouldShowPrivacyDisclaimers =
                    !mManager.isIncognito()
                            && (mManager.launchedForApp() || mManager.showAppFilter())
                            && mManager.getShouldShowPrivacyDisclaimersIfAvailable();
        } else {
            shouldShowPrivacyDisclaimers =
                    hasPrivacyDisclaimers()
                            && mManager.getShouldShowPrivacyDisclaimersIfAvailable();
        }
        // Prevent from refreshing the recycler view if header visibility is not changed.
        if (mPrivacyDisclaimersVisible == shouldShowPrivacyDisclaimers) return;
        mPrivacyDisclaimersVisible = shouldShowPrivacyDisclaimers;
        if (mAreHeadersInitialized) setHeaders();
    }

    private void updateClearBrowsingDataButtonVisibility() {
        // If the history header is not showing (e.g. when there is no browsing history),
        // mClearBrowsingDataButton will be null.
        if (mClearBrowsingDataButton == null) return;

        boolean shouldShowButton = mManager.getShouldShowClearData();
        if (mClearBrowsingDataButtonVisible == shouldShowButton) return;
        mClearBrowsingDataButtonVisible = shouldShowButton;

        if (mAreHeadersInitialized) setHeaders();
    }

    /**
     * @param hostName The hostName to retrieve history entries for.
     */
    public void setHostName(@Nullable String hostName) {
        mHostName = hostName;
    }

    /**
     * @param appId The app ID to retrieve history entries for.
     */
    public void setAppId(@Nullable String appId) {
        mAppId = appId;
    }

    public void setIsLargeFormFactorDevice(boolean isLargeFormFactorDevice) {
        mIsLargeFormFactorDevice = isLargeFormFactorDevice;
    }

    void updateHistorySyncPromoVisibility() {
        if (mHistorySyncPromoCoordinator == null) {
            return;
        }

        boolean shouldHistorySyncPromoBeVisible = mHistorySyncPromoCoordinator.canShowPromo();
        if (shouldHistorySyncPromoBeVisible == mHistorySyncPromoVisible) {
            return;
        }

        mHistorySyncPromoVisible = shouldHistorySyncPromoBeVisible;
        if (!mAreHeadersInitialized) {
            return;
        }

        setHeaders();
        if (!shouldHistorySyncPromoBeVisible) {
            // When removing the history sync promo, other headers should be removed when there's
            // no history record.
            removeHeaderIfEmpty();
        }
    }

    /**
     * Checks whether the finds promo is eligible to show, through an async call for notification
     * channels. Update the eligibility cached tracker and if eligible, refresh the set of headers.
     * Setting the headers will update the visibility tracker separately.
     *
     * @param profile The current user profile.
     */
    private void checkFindsPromoShowCriteriaAsync(Profile profile) {
        FindsUtils.checkShowCriteriaOptInPromo(
                profile,
                (show) -> {
                    mFindsPromoShowEligible = show;
                    if (show || FindsFeatures.sAlwaysShowOptInPromo.getValue()) {
                        // Only update the Finds Promo visibility here since there is no need to
                        // rerun this logic if there are any dynamic changes to other promos.
                        updateFindsPromoVisibility();
                        setHeaders();
                    }
                });
    }

    private void updateFindsPromoVisibility() {
        // Ensure that the Finds Promo is mutually exclusive with the History Sync Promo.
        mFindsPromoVisible = !mHistorySyncPromoVisible;

        if (mFindsPromoVisible && mProfile != null) {
            FindsUtils.setOptInPromoSeen(mProfile);
        }
    }

    ItemGroup getFirstGroupForTests() {
        return getGroupAt(0).first;
    }

    @Nullable ItemGroup getLastGroupForTests() {
        final int itemCount = getItemCount();
        return itemCount > 0 ? getGroupAt(itemCount - 1).first : null;
    }

    void setClearBrowsingDataButtonVisibilityForTest(boolean isVisible) {
        if (mClearBrowsingDataButtonVisible == isVisible) return;
        mClearBrowsingDataButtonVisible = isVisible;

        setHeaders();
    }

    public ArrayList<HistoryItemView> getItemViewsForTests() {
        return mItemViews;
    }

    @SuppressWarnings("NullAway")
    void generateHeaderItemsForTest() {
        mPrivacyDisclaimerHeaderItem = new StandardHeaderItem(0, null);
        mClearBrowsingDataButtonHeaderItem = new StandardHeaderItem(1, null);
        mClearBrowsingDataButtonVisible = true;
        mAppFilterHeaderItem = new StandardHeaderItem(0, null);
        mHistorySyncPromoHeaderItem = new PersistentHeaderItem(2, null);
    }

    @SuppressWarnings("NullAway")
    void generateFooterItemsForTest(MoreProgressButton mockButton) {
        mMoreProgressButton = mockButton;
        mMoreProgressButtonFooterItem = new FooterItem(-1, null);
    }

    @VisibleForTesting
    boolean arePrivacyDisclaimersVisible() {
        return mPrivacyDisclaimersVisible;
    }

    @VisibleForTesting
    boolean isClearBrowsingDataButtonVisible() {
        return mClearBrowsingDataButtonVisible;
    }

    void setScrollToLoadDisabledForTest(boolean isDisabled) {
        mDisableScrollToLoadForTest = isDisabled;
    }

    MoreProgressButton getMoreProgressButtonForTest() {
        return mMoreProgressButton;
    }

    ChipView getAppFilterButtonForTest() {
        return mAppFilterChip;
    }

    void setAppFilterButtonForTest(ChipView appFilterChip) {
        mAppFilterChip = appFilterChip;
    }

    boolean showSourceAppForTest() {
        return mShowSourceApp;
    }

    @Nullable String getAppIdForTest() {
        return mAppId;
    }

    public void toggleCluster(HistoryItem item) {
        String key = getExpansionKey(item);
        boolean expanding = !mExpandedClusterKeys.contains(key);
        if (expanding) {
            mExpandedClusterKeys.add(key);
        } else {
            mExpandedClusterKeys.remove(key);
        }

        int pos = item.getPosition();
        if (pos == TimedItem.INVALID_POSITION) return;

        HistoryItem newHead = item.toBuilder().setIsExpanded(expanding).build();

        List<HistoryItem> subItems = item.getSubItems();
        if (subItems == null) return;

        if (expanding) {
            updateGroupItems(pos, newHead, 0, subItems);
        } else {
            updateGroupItems(pos, newHead, subItems.size(), null);
        }
    }

    private void rebuildItemList() {
        clear(true);
        List<HistoryItem> clustered = clusterItems(mAllItems);
        loadItems(clustered);
        if (mAreHeadersInitialized) {
            setHeaders();
        }
        if (mHasMorePotentialItems) {
            updateFooter();
        }
        if (mAreHeadersInitialized && clustered.isEmpty() && !mIsSearching) {
            removeHeaderIfEmpty();
        }
    }

    /**
     * Groups consecutive history items with the same domain and timestamp into clusters. For each
     * cluster, creates a virtual head item and optionally includes its sub-items if the cluster is
     * expanded.
     *
     * @param items The list of history items to cluster.
     * @return A new list of items with clustered items grouped under cluster heads.
     */
    private List<HistoryItem> clusterItems(List<HistoryItem> items) {
        if (!mShouldClusterByDomain) {
            return items;
        }

        List<HistoryItem> clusteredItems = new ArrayList<>();
        int i = 0;
        while (i < items.size()) {
            HistoryItem current = items.get(i);

            int j = i + 1;
            while (j < items.size() && isSameCluster(current, items.get(j))) {
                j++;
            }

            if (j > i + 1) {
                List<HistoryItem> cluster = items.subList(i, j);
                // Determine the cluster ID to use.
                @Nullable HistoryItem itemWithClusterId = null;
                for (HistoryItem item : cluster) {
                    if (item.getClusterId() != null) {
                        itemWithClusterId = item;
                        break;
                    }
                }
                Long clusterId =
                        itemWithClusterId != null
                                ? itemWithClusterId.getClusterId()
                                : mNextClusterId++;

                // Assign the cluster ID to all items in this cluster
                for (int k = 0; k < cluster.size(); k++) {
                    cluster.set(k, cluster.get(k).toBuilder().setClusterId(clusterId).build());
                }

                // Create a virtual head for the cluster
                HistoryItem head = createClusterHead(cluster, clusterId);
                clusteredItems.add(head);
                if (head.isExpanded()) {
                    clusteredItems.addAll(cluster);
                }
                i = j;
            } else {
                assert !current.isClusterHead();
                clusteredItems.add(current);
                i++;
            }
        }
        return clusteredItems;
    }

    private boolean isSameCluster(HistoryItem item1, HistoryItem item2) {
        return TextUtils.equals(item1.getDomain(), item2.getDomain())
                && compareDate(new Date(item1.getTimestamp()), new Date(item2.getTimestamp())) == 0;
    }

    private HistoryItem createClusterHead(List<HistoryItem> cluster, long clusterId) {
        HistoryItem template = cluster.get(0);
        String expansionKey = CLUSTER_EXPANSION_KEY_PREFIX + clusterId;
        HistoryItem head =
                template.toBuilder()
                        .setTitle(template.getDomain())
                        .setIsClusterHead(true)
                        .setSubItems(new ArrayList<>(cluster))
                        .setClusterId(clusterId)
                        .setIsExpanded(mExpandedClusterKeys.contains(expansionKey))
                        .setHistoryManager(mManager)
                        .build();
        return head;
    }

    private String getExpansionKey(HistoryItem item) {
        return CLUSTER_EXPANSION_KEY_PREFIX + item.getClusterId();
    }
}
