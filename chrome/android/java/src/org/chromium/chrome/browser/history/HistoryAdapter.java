// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.AppFilterCoordinator.AppInfo;
import org.chromium.chrome.browser.history.HistoryProvider.BrowsingHistoryObserver;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.components.browser_ui.widget.DateDividedAdapter;
import org.chromium.components.browser_ui.widget.MoreProgressButton;
import org.chromium.components.browser_ui.widget.MoreProgressButton.State;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.ArrayList;
import java.util.List;

/** Bridges the user's browsing history and the UI used to display it. */
public class HistoryAdapter extends DateDividedAdapter implements BrowsingHistoryObserver {
    private static final String EMPTY_QUERY = "";

    private final HistoryContentManager mManager;
    private final ArrayList<HistoryItemView> mItemViews;
    private final DefaultFaviconHelper mFaviconHelper;
    private final boolean mShowAppFilter;

    private RecyclerView mRecyclerView;
    private @Nullable HistoryProvider mHistoryProvider;

    // Headers
    private TextView mPrivacyDisclaimerTextView;
    private View mPrivacyDisclaimerBottomSpace;
    private Button mHistoryOpenInChromeButton;
    private Button mClearBrowsingDataButton;
    private HeaderItem mPrivacyDisclaimerHeaderItem;
    private HeaderItem mClearBrowsingDataButtonHeaderItem;
    private HeaderItem mHistoryOpenInChromeHeaderItem;
    private HeaderItem mAppFilterHeaderItem;
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
    private String mQueryText = EMPTY_QUERY;
    private String mHostName;

    // ID of the App currently chosen for app filtering. If null, ignored when querying history.
    private String mAppId;
    private boolean mDisableScrollToLoadForTest;

    // Whether we show the source app for each entry. We show it in BrApp in full history UI, but
    // not in search mode when app filter is in effect.
    private boolean mShowSourceApp;

    public HistoryAdapter(HistoryContentManager manager, HistoryProvider provider) {
        setHasStableIds(true);
        mHistoryProvider = provider;
        mHistoryProvider.setObserver(this);
        mManager = manager;
        mFaviconHelper = new DefaultFaviconHelper();
        mItemViews = new ArrayList<>();
        mShowAppFilter = mManager.showAppFilter();
        mShowSourceApp = mShowAppFilter; // defaults to BrApp full history
    }

    /** Called when the activity/native page is destroyed. */
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
     * @param item The item to mark for removal.
     */
    public void markItemForRemoval(HistoryItem item) {
        removeItem(item);
        mHistoryProvider.markItemForRemoval(item);
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

        if (mClearOnNextQueryComplete) {
            clear(true);
            mClearOnNextQueryComplete = false;
        }
        if ((!mAreHeadersInitialized && items.size() > 0 && !mIsSearching)
                || (mIsSearching && mShowAppFilter)) {
            setHeaders();
            mAreHeadersInitialized = true;
        }

        removeFooter();

        loadItems(items);

        mIsLoadingItems = false;
        mHasMorePotentialItems = hasMorePotentialMatches;

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
        if (mIsSearching) setHeaders();
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
    void generateHeaderItems() {
        ViewGroup historyAppFilterContainer = getAppFilterContainer(null);
        ViewGroup privacyDisclaimerContainer = getPrivacyDisclaimerContainer(null);

        ViewGroup clearBrowsingDataButtonContainer = getClearBrowsingDataButtonContainer(null);

        mAppFilterHeaderItem = new HeaderItem(0, historyAppFilterContainer);
        mPrivacyDisclaimerHeaderItem = new HeaderItem(0, privacyDisclaimerContainer);
        mPrivacyDisclaimerBottomSpace =
                privacyDisclaimerContainer.findViewById(R.id.privacy_disclaimer_bottom_space);
        mClearBrowsingDataButtonHeaderItem = new HeaderItem(1, clearBrowsingDataButtonContainer);
        mClearBrowsingDataButton =
                clearBrowsingDataButtonContainer.findViewById(R.id.clear_browsing_data_button);

        if (mManager.launchedForApp()) {
            ViewGroup historyOpenInChromeButtonContainer = getCctOpenInChromeButtonContainer(null);

            mHistoryOpenInChromeHeaderItem = new HeaderItem(1, historyOpenInChromeButtonContainer);
            mHistoryOpenInChromeButton =
                    historyOpenInChromeButtonContainer.findViewById(
                            R.id.open_full_chrome_history_button);
        }

        updateClearBrowsingDataButtonVisibility();
        setPrivacyDisclaimer();
        updatePrivacyDisclaimerBottomSpace();
    }

    ViewGroup getClearBrowsingDataButtonContainer(ViewGroup parent) {
        ViewGroup viewGroup =
                (ViewGroup)
                        LayoutInflater.from(mManager.getContext())
                                .inflate(
                                        R.layout.history_clear_browsing_data_header, parent, false);
        Button clearBrowsingDataButton = viewGroup.findViewById(R.id.clear_browsing_data_button);
        clearBrowsingDataButton.setOnClickListener(v -> mManager.onClearBrowsingDataClicked());
        return viewGroup;
    }

    ViewGroup getCctOpenInChromeButtonContainer(ViewGroup parent) {
        ViewGroup viewGroup =
                (ViewGroup)
                        LayoutInflater.from(mManager.getContext())
                                .inflate(R.layout.open_full_chrome_history_header, parent, true);
        Button clearBrowsingDataButton =
                viewGroup.findViewById(R.id.open_full_chrome_history_button);
        clearBrowsingDataButton.setOnClickListener(v -> mManager.onOpenFullChromeHistoryClicked());
        return viewGroup;
    }

    ViewGroup getAppFilterContainer(ViewGroup parent) {
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

    void updateHistory(AppInfo appInfo) {
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

    ViewGroup getPrivacyDisclaimerContainer(ViewGroup parent) {
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
                    text = context.getResources().getString(R.string.android_app_history_open_full);
                }
            } else if (mManager.showAppFilter()) { // History UI in BrApp
                if (hasPrivacyDisclaimers()) {
                    int res = R.string.android_history_from_other_apps_other_forms_of_history;
                    text = getPrivacyDisclaimerClickableSpanString(context, res);
                } else {
                    int res = R.string.android_history_from_other_apps;
                    text = context.getResources().getString(res);
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
        var s = context.getResources().getString(resId);
        var link =
                new NoUnderlineClickableSpan(
                        context, (v) -> mManager.onPrivacyDisclaimerLinkClicked());
        return SpanApplier.applySpans(s, new SpanApplier.SpanInfo("<link>", "</link>", link));
    }

    /** Pass header items to {@link #setHeaders(HeaderItem...)} as parameters. */
    private void setHeaders() {
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

    /** @param hostName The hostName to retrieve history entries for. */
    public void setHostName(String hostName) {
        mHostName = hostName;
    }

    /**
     * @param appId The app ID to retrieve history entries for.
     */
    public void setAppId(String appId) {
        mAppId = appId;
    }

    ItemGroup getFirstGroupForTests() {
        return getGroupAt(0).first;
    }

    ItemGroup getLastGroupForTests() {
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

    void generateHeaderItemsForTest() {
        mPrivacyDisclaimerHeaderItem = new HeaderItem(0, null);
        mClearBrowsingDataButtonHeaderItem = new HeaderItem(1, null);
        mClearBrowsingDataButtonVisible = true;
        mAppFilterHeaderItem = new HeaderItem(0, null);
    }

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

    String getAppIdForTest() {
        return mAppId;
    }
}
