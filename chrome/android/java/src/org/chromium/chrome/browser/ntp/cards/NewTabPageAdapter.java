// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.Adapter;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.DestructionObserver;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

import java.util.List;
import java.util.Set;

/**
 * A class that handles merging above the fold elements and below the fold cards into an adapter
 * that will be used to back the NTP RecyclerView. The first element in the adapter should always be
 * the above-the-fold view (containing the logo, search box, and most visited tiles) and subsequent
 * elements will be the cards shown to the user
 */
public class NewTabPageAdapter extends Adapter<NewTabPageViewHolder>
        implements ListObservable.ListObserver<PartialBindCallback> {
    private final SuggestionsUiDelegate mUiDelegate;
    private final ContextMenuManager mContextMenuManager;
    private final OfflinePageBridge mOfflinePageBridge;

    private final @Nullable View mAboveTheFoldView;
    private final UiConfig mUiConfig;
    private SuggestionsRecyclerView mRecyclerView;

    private final InnerNode<NewTabPageViewHolder, PartialBindCallback> mRoot;

    private final SectionList mSections;
    private final @Nullable SignInPromo mSigninPromo;
    private final AllDismissedItem mAllDismissed;
    private final Footer mFooter;

    private final RemoteSuggestionsStatusObserver mRemoteSuggestionsStatusObserver;

    /**
     * Creates the adapter that will manage all the cards to display on the NTP.
     * @param uiDelegate used to interact with the rest of the system.
     * @param aboveTheFoldView the layout encapsulating all the above-the-fold elements
     *         (logo, search box, most visited tiles), or null if only suggestions should
     *         be displayed.
     * @param uiConfig the NTP UI configuration, to be passed to created views.
     * @param offlinePageBridge used to determine if articles are available.
     * @param contextMenuManager used to build context menus.
     */
    public NewTabPageAdapter(SuggestionsUiDelegate uiDelegate, @Nullable View aboveTheFoldView,
            UiConfig uiConfig, OfflinePageBridge offlinePageBridge,
            ContextMenuManager contextMenuManager) {
        mUiDelegate = uiDelegate;
        mContextMenuManager = contextMenuManager;

        mAboveTheFoldView = aboveTheFoldView;
        mUiConfig = uiConfig;
        mRoot = new InnerNode<>();
        mSections = new SectionList(mUiDelegate, offlinePageBridge);
        mAllDismissed = new AllDismissedItem();

        if (SignInPromo.shouldCreatePromo()) {
            mSigninPromo = new SignInPromo();
            mSigninPromo.setCanShowPersonalizedSuggestions(
                    mUiDelegate.getSuggestionsSource().areRemoteSuggestionsEnabled());
        } else {
            mSigninPromo = null;
        }

        if (mAboveTheFoldView != null) mRoot.addChildren(new AboveTheFoldItem());

        // Show the sign-in promo above suggested content.
        if (mSigninPromo != null) mRoot.addChildren(mSigninPromo);
        mRoot.addChildren(mAllDismissed, mSections);

        mFooter = new Footer();
        mRoot.addChildren(mFooter);

        mOfflinePageBridge = offlinePageBridge;

        mRemoteSuggestionsStatusObserver = new RemoteSuggestionsStatusObserver();
        mUiDelegate.addDestructionObserver(mRemoteSuggestionsStatusObserver);

        updateAllDismissedVisibility();
        mRoot.addObserver(this);
    }

    @Override
    @ItemViewType
    public int getItemViewType(int position) {
        return mRoot.getItemViewType(position);
    }

    @Override
    public NewTabPageViewHolder onCreateViewHolder(ViewGroup parent, @ItemViewType int viewType) {
        assert parent == mRecyclerView;

        switch (viewType) {
            case ItemViewType.ABOVE_THE_FOLD:
                return new NewTabPageViewHolder(mAboveTheFoldView);

            case ItemViewType.HEADER:
                return new SectionHeaderViewHolder(mRecyclerView, mUiConfig);

            case ItemViewType.SNIPPET:
                return new SnippetArticleViewHolder(mRecyclerView, mContextMenuManager, mUiDelegate,
                        mUiConfig, mOfflinePageBridge);

            case ItemViewType.STATUS:
                return new StatusCardViewHolder(mRecyclerView, mContextMenuManager, mUiConfig);

            case ItemViewType.PROGRESS:
                return new ProgressViewHolder(mRecyclerView);

            case ItemViewType.ACTION:
                return new ActionItem.ViewHolder(
                        mRecyclerView, mContextMenuManager, mUiDelegate, mUiConfig);

            case ItemViewType.PROMO:
                return new PersonalizedPromoViewHolder(
                        mRecyclerView, mContextMenuManager, mUiConfig);

            case ItemViewType.FOOTER:
                return new Footer.ViewHolder(mRecyclerView, mUiDelegate.getNavigationDelegate());

            case ItemViewType.ALL_DISMISSED:
                return new AllDismissedItem.ViewHolder(mRecyclerView, mSections);
        }

        assert false : viewType;
        return null;
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, int position, List<Object> payloads) {
        if (payloads.isEmpty()) {
            onBindViewHolder(holder, position);
            return;
        }

        for (Object payload : payloads) {
            mRoot.onBindViewHolder(holder, position, (PartialBindCallback) payload);
        }
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, final int position) {
        mRoot.onBindViewHolder(holder, position, null);
    }

    @Override
    public int getItemCount() {
        return mRoot.getItemCount();
    }

    /** Resets suggestions, pulling the current state as known by the backend. */
    public void refreshSuggestions() {
        mSections.refreshSuggestions();
    }

    public int getFirstHeaderPosition() {
        return getFirstPositionForType(ItemViewType.HEADER);
    }

    public int getFirstCardPosition() {
        for (int i = 0; i < getItemCount(); ++i) {
            if (CardViewHolder.isCard(getItemViewType(i))) return i;
        }
        return RecyclerView.NO_POSITION;
    }

    private void updateAllDismissedVisibility() {
        boolean areRemoteSuggestionsEnabled =
                mUiDelegate.getSuggestionsSource().areRemoteSuggestionsEnabled();
        boolean allDismissed = hasAllBeenDismissed() && !areArticlesLoading();
        boolean isArticleSectionVisible = mSections.getSection(KnownCategories.ARTICLES) != null;

        mAllDismissed.setVisible(areRemoteSuggestionsEnabled && allDismissed);
        mFooter.setVisible(!SuggestionsConfig.scrollToLoad() && !allDismissed
                && (areRemoteSuggestionsEnabled || isArticleSectionVisible));
    }

    private boolean areArticlesLoading() {
        for (int category : mUiDelegate.getSuggestionsSource().getCategories()) {
            if (category != KnownCategories.ARTICLES) continue;

            return mUiDelegate.getSuggestionsSource().getCategoryStatus(KnownCategories.ARTICLES)
                    == CategoryStatus.AVAILABLE_LOADING;
        }
        return false;
    }

    @Override
    public void onItemRangeChanged(ListObservable child, int itemPosition, int itemCount,
            @Nullable PartialBindCallback payload) {
        assert child == mRoot;
        notifyItemRangeChanged(itemPosition, itemCount, payload);
    }

    @Override
    public void onItemRangeInserted(ListObservable child, int itemPosition, int itemCount) {
        assert child == mRoot;
        notifyItemRangeInserted(itemPosition, itemCount);

        updateAllDismissedVisibility();
    }

    @Override
    public void onItemRangeRemoved(ListObservable child, int itemPosition, int itemCount) {
        assert child == mRoot;
        notifyItemRangeRemoved(itemPosition, itemCount);

        updateAllDismissedVisibility();
    }

    @Override
    public void onAttachedToRecyclerView(RecyclerView recyclerView) {
        super.onAttachedToRecyclerView(recyclerView);

        if (mRecyclerView == recyclerView) return;

        // We are assuming for now that the adapter is used with a single RecyclerView.
        // Getting the reference as we are doing here is going to be broken if that changes.
        assert mRecyclerView == null;

        mRecyclerView = (SuggestionsRecyclerView) recyclerView;

        if (SuggestionsConfig.scrollToLoad()) {
            mRecyclerView.setScrollToLoadListener(new ScrollToLoadListener(
                    this, mRecyclerView.getLinearLayoutManager(), mSections));
        }
    }

    @Override
    public void onDetachedFromRecyclerView(RecyclerView recyclerView) {
        super.onDetachedFromRecyclerView(recyclerView);

        if (SuggestionsConfig.scrollToLoad()) mRecyclerView.clearScrollToLoadListener();

        mRecyclerView = null;
    }

    @Override
    public void onViewRecycled(NewTabPageViewHolder holder) {
        holder.recycle();
    }

    /**
     * @return the set of item positions that should be dismissed simultaneously when dismissing the
     *         item at the given {@code position} (including the position itself), or an empty set
     *         if the item can't be dismissed.
     */
    public Set<Integer> getItemDismissalGroup(int position) {
        return mRoot.getItemDismissalGroup(position);
    }

    /**
     * Dismisses the item at the provided adapter position. Can also cause the dismissal of other
     * items or even entire sections.
     * @param position the position of an item to be dismissed.
     * @param itemRemovedCallback
     */
    public void dismissItem(int position, Callback<String> itemRemovedCallback) {
        mRoot.dismissItem(position, itemRemovedCallback);
    }

    private boolean hasAllBeenDismissed() {
        if (mSigninPromo != null && mSigninPromo.isVisible()) return false;

        return mSections.isEmpty();
    }

    @VisibleForTesting
    public int getFirstPositionForType(@ItemViewType int viewType) {
        int count = getItemCount();
        for (int i = 0; i < count; i++) {
            if (getItemViewType(i) == viewType) return i;
        }
        return RecyclerView.NO_POSITION;
    }

    public SectionList getSectionListForTesting() {
        return mSections;
    }

    public InnerNode getRootForTesting() {
        return mRoot;
    }

    @VisibleForTesting
    SuggestionsSource.Observer getSuggestionsSourceObserverForTesting() {
        return mRemoteSuggestionsStatusObserver;
    }

    @VisibleForTesting
    SignInPromo getSignInPromoForTesting() {
        return mSigninPromo;
    }

    private class RemoteSuggestionsStatusObserver
            extends SuggestionsSource.EmptyObserver implements DestructionObserver {
        public RemoteSuggestionsStatusObserver() {
            mUiDelegate.getSuggestionsSource().addObserver(this);
        }

        @Override
        public void onCategoryStatusChanged(
                @CategoryInt int category, @CategoryStatus int newStatus) {
            if (!SnippetsBridge.isCategoryRemote(category)) return;

            updateAllDismissedVisibility();

            // Checks whether the category is enabled first to avoid unnecessary
            // calls across JNI.
            if (mSigninPromo != null) {
                mSigninPromo.setCanShowPersonalizedSuggestions(
                        SnippetsBridge.isCategoryEnabled(newStatus)
                        || mUiDelegate.getSuggestionsSource().areRemoteSuggestionsEnabled());
            }
        }

        @Override
        public void onDestroy() {
            mUiDelegate.getSuggestionsSource().removeObserver(this);
            if (mSigninPromo != null) mSigninPromo.destroy();
        }
    }
}
