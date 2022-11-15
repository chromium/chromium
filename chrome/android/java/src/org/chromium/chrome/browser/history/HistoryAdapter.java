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
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.HistoryProvider.BrowsingHistoryObserver;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.components.browser_ui.widget.DateDividedAdapter;
import org.chromium.components.browser_ui.widget.MoreProgressButton;
import org.chromium.components.browser_ui.widget.MoreProgressButton.State;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;

/**
 * Bridges the user's browsing history and the UI used to display it.
 */
public class HistoryAdapter extends DateDividedAdapter implements BrowsingHistoryObserver {
    private static final String EMPTY_QUERY = "";

    private final HistoryContentManager mManager;
    private final ArrayList<HistoryItemView> mItemViews;
    private final DefaultFaviconHelper mFaviconHelper;
    private RecyclerView mRecyclerView;
    private @Nullable HistoryProvider mHistoryProvider;

    // Headers
    private View mPrivacyDisclaimerBottomSpace;
    private Button mClearBrowsingDataButton;
    private HeaderItem mPrivacyDisclaimerHeaderItem;
    private HeaderItem mClearBrowsingDataButtonHeaderItem;
    private HeaderItem mHistoryToggleHeaderItem;

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

    private boolean mDisableScrollToLoadForTest;
    private ObservableSupplier<Boolean> mShowHistoryToggleSupplier;
    private Function<ViewGroup, ViewGroup> mToggleViewFactory;

    public HistoryAdapter(HistoryContentManager manager, HistoryProvider provider,
            ObservableSupplier<Boolean> showHistoryToggleSupplier,
            Function<ViewGroup, ViewGroup> toggleViewFactory) {
        mToggleViewFactory = toggleViewFactory;
        setHasStableIds(true);
        mHistoryProvider = provider;
        mHistoryProvider.setObserver(this);
        mManager = manager;
        mShowHistoryToggleSupplier = showHistoryToggleSupplier;
        mShowHistoryToggleSupplier.addObserver((unused) -> setHeaders());
        mFaviconHelper = new DefaultFaviconHelper();
        mItemViews = new ArrayList<>();
    }

    /**
     * Called when the activity/native page is destroyed.
     */
    public void onDestroyed() {
        mIsDestroyed = true;

        mHistoryProvider.destroy();
        mHistoryProvider = null;

        mRecyclerView = null;
        mFaviconHelper.clearCache();
    }

    /**
     * Starts loading the first set of browsing history items.
     */
    public void startLoadingItems() {
        mAreHeadersInitialized = false;
        mIsLoadingItems = true;
        mClearOnNextQueryComplete = true;
        if (mHostName != null) {
            mHistoryProvider.queryHistoryForHost(mHostName);
        } else {
            mHistoryProvider.queryHistory(mQueryText);
        }
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
     * @param query The text to search for.
     */
    public void search(String query) {
        mQueryText = query;
        mIsSearching = true;
        mClearOnNextQueryComplete = true;
        mHistoryProvider.queryHistory(mQueryText);
    }

    /**
     * Called when a search is ended.
     */
    public void onEndSearch() {
        mQueryText = EMPTY_QUERY;
        mIsSearching = false;

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

    /**
     * Removes all items that have been marked for removal through #markItemForRemoval().
     */
    public void removeItems() {
        mHistoryProvider.removeItems();
    }

    /**
     * Should be called when the user's sign in state changes.
     */
    public void onSignInStateChange() {
        for (HistoryItemView itemView : mItemViews) {
            itemView.onSignInStateChange();
        }
        startLoadingItems();
        updateClearBrowsingDataButtonVisibility();
    }

    /**
     * Sets the selectable item mode. Items only selectable if they have a SelectableItemViewHolder.
     * @param active Whether the selection mode is on or not.
     */
    public void setSelectionActive(boolean active) {
        if (mClearBrowsingDataButton != null) {
            mClearBrowsingDataButton.setEnabled(!active);
        }
        for (HistoryItemView item : mItemViews) {
            item.setRemoveButtonVisible(!active);
        }
    }

    @Override
    protected ViewHolder createViewHolder(ViewGroup parent) {
        View v = LayoutInflater.from(parent.getContext()).inflate(
                R.layout.history_item_view, parent, false);
        ViewHolder viewHolder = mManager.getHistoryItemViewHolder(v);
        HistoryItemView itemView = (HistoryItemView) viewHolder.itemView;
        itemView.setRemoveButtonVisible(mManager.shouldShowRemoveItemButton());
        itemView.setFaviconHelper(mFaviconHelper);
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

        if (!mAreHeadersInitialized && items.size() > 0 && !mIsSearching) {
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
        setPrivacyDisclaimer();
        mManager.onPrivacyDisclaimerHasChanged();
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
        mMoreProgressButton = (MoreProgressButton) View.inflate(
                mManager.getContext(), R.layout.more_progress_button, null);

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

    /**
     * Update footer when the content change.
     */
    private void updateFooter() {
        if (isScrollToLoadDisabled() || mIsLoadingItems) addFooter();
    }

    /**
     * Initialize clear browsing data and privacy disclaimer header views and generate header
     * items for them.
     */
    void generateHeaderItems() {
        ViewGroup privacyDisclaimerContainer = getPrivacyDisclaimerContainer(null);

        ViewGroup clearBrowsingDataButtonContainer = getClearBrowsingDataButtonContainer(null);

        mPrivacyDisclaimerHeaderItem = new HeaderItem(0, privacyDisclaimerContainer);
        mPrivacyDisclaimerBottomSpace =
                privacyDisclaimerContainer.findViewById(R.id.privacy_disclaimer_bottom_space);
        mClearBrowsingDataButtonHeaderItem = new HeaderItem(1, clearBrowsingDataButtonContainer);
        mClearBrowsingDataButton = (Button) clearBrowsingDataButtonContainer.findViewById(
                R.id.clear_browsing_data_button);

        ViewGroup toggleContainer = mToggleViewFactory.apply(null);
        if (toggleContainer != null) {
            mHistoryToggleHeaderItem = new HeaderItem(2, toggleContainer);
        }

        updateClearBrowsingDataButtonVisibility();
        setPrivacyDisclaimer();
    }

    ViewGroup getClearBrowsingDataButtonContainer(ViewGroup parent) {
        ViewGroup viewGroup =
                (ViewGroup) LayoutInflater.from(mManager.getContext())
                        .inflate(R.layout.history_clear_browsing_data_header, parent, false);
        Button clearBrowsingDataButton =
                (Button) viewGroup.findViewById(R.id.clear_browsing_data_button);
        clearBrowsingDataButton.setOnClickListener(v -> mManager.onClearBrowsingDataClicked());
        return viewGroup;
    }

    ViewGroup getPrivacyDisclaimerContainer(ViewGroup parent) {
        Context context = mManager.getContext();
        ViewGroup privacyDisclaimerContainer = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.history_privacy_disclaimer_header, parent, false);

        TextView privacyDisclaimerTextView =
                privacyDisclaimerContainer.findViewById(R.id.privacy_disclaimer);
        privacyDisclaimerTextView.setMovementMethod(LinkMovementMethod.getInstance());

        NoUnderlineClickableSpan link = new NoUnderlineClickableSpan(
                context, (view) -> mManager.onPrivacyDisclaimerLinkClicked());
        CharSequence disclaimerText = SpanApplier.applySpans(
                context.getResources().getString(R.string.android_history_other_forms_of_history),
                new SpanApplier.SpanInfo("<link>", "</link>", link));
        privacyDisclaimerTextView.setText(disclaimerText);
        return privacyDisclaimerContainer;
    }

    /**
     * Pass header items to {@link #setHeaders(HeaderItem...)} as parameters.
     */
    private void setHeaders() {
        ArrayList<HeaderItem> args = new ArrayList<>();
        if (mPrivacyDisclaimersVisible) args.add(mPrivacyDisclaimerHeaderItem);
        if (mClearBrowsingDataButtonVisible) args.add(mClearBrowsingDataButtonHeaderItem);
        boolean showHistoryToggle =
                mShowHistoryToggleSupplier.get() != null && mShowHistoryToggleSupplier.get();
        if (showHistoryToggle && mHistoryToggleHeaderItem != null) {
            args.add(mHistoryToggleHeaderItem);
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

    /**
     * Set text of privacy disclaimer and visibility of its container.
     */
    void setPrivacyDisclaimer() {
        boolean shouldShowPrivacyDisclaimers =
                hasPrivacyDisclaimers() && mManager.getShouldShowPrivacyDisclaimersIfAvailable();

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
        mPrivacyDisclaimerBottomSpace.setVisibility(shouldShowButton ? View.GONE : View.VISIBLE);

        if (mAreHeadersInitialized) setHeaders();
    }

    /** @param hostName The hostName to retrieve history entries for. */
    public void setHostName(String hostName) {
        mHostName = hostName;
    }

    @VisibleForTesting
    ItemGroup getFirstGroupForTests() {
        return getGroupAt(0).first;
    }

    @VisibleForTesting
    ItemGroup getLastGroupForTests() {
        final int itemCount = getItemCount();
        return itemCount > 0 ? getGroupAt(itemCount - 1).first : null;
    }

    @VisibleForTesting
    void setClearBrowsingDataButtonVisibilityForTest(boolean isVisible) {
        if (mClearBrowsingDataButtonVisible == isVisible) return;
        mClearBrowsingDataButtonVisible = isVisible;

        setHeaders();
    }

    @VisibleForTesting
    public ArrayList<HistoryItemView> getItemViewsForTests() {
        return mItemViews;
    }

    @VisibleForTesting
    void generateHeaderItemsForTest() {
        mPrivacyDisclaimerHeaderItem = new HeaderItem(0, null);
        mClearBrowsingDataButtonHeaderItem = new HeaderItem(1, null);
        mClearBrowsingDataButtonVisible = true;
    }

    @VisibleForTesting
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

    @VisibleForTesting
    void setScrollToLoadDisabledForTest(boolean isDisabled) {
        mDisableScrollToLoadForTest = isDisabled;
    }

    @VisibleForTesting
    MoreProgressButton getMoreProgressButtonForTest() {
        return mMoreProgressButton;
    }
}
