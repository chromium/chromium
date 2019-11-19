// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import androidx.annotation.LayoutRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.metrics.ImpressionTracker;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.cards.CardViewHolder;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.SectionList;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.SuggestionsBinder;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsOfflineModelObserver;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.ui.widget.displaystyle.DisplayStyleObserverAdapter;
import org.chromium.chrome.browser.ui.widget.displaystyle.UiConfig;
import org.chromium.ui.mojom.WindowOpenDisposition;

/**
 * A class that represents the view for a single card snippet.
 */
public class SnippetArticleViewHolder extends CardViewHolder {
    private final SuggestionsUiDelegate mUiDelegate;
    private final SuggestionsBinder mSuggestionsBinder;
    private final OfflinePageBridge mOfflinePageBridge;
    private SuggestionsCategoryInfo mCategoryInfo;
    private SnippetArticle mArticle;

    private final DisplayStyleObserverAdapter mDisplayStyleObserver;
    private final ImpressionTracker mExposureTracker;

    /**
     * Constructs a {@link SnippetArticleViewHolder} item used to display snippets.
     * @param parent The SuggestionsRecyclerView that is going to contain the newly created view.
     * @param contextMenuManager The manager responsible for the context menu.
     * @param uiDelegate The delegate object used to open an article, fetch thumbnails, etc.
     * @param uiConfig The NTP UI configuration object used to adjust the article UI.
     * @param offlinePageBridge used to determine if article is prefetched.
     */
    public SnippetArticleViewHolder(SuggestionsRecyclerView parent,
            ContextMenuManager contextMenuManager, SuggestionsUiDelegate uiDelegate,
            UiConfig uiConfig, OfflinePageBridge offlinePageBridge) {
        this(parent, contextMenuManager, uiDelegate, uiConfig, offlinePageBridge, getLayout());
    }

    /**
     * Constructs a {@link SnippetArticleViewHolder} item used to display snippets.
     * @param parent The SuggestionsRecyclerView that is going to contain the newly created view.
     * @param contextMenuManager The manager responsible for the context menu.
     * @param uiDelegate The delegate object used to open an article, fetch thumbnails, etc.
     * @param uiConfig The NTP UI configuration object used to adjust the article UI.
     * @param offlinePageBridge used to determine if article is prefetched.
     * @param layoutId The layout resource reference for this card.
     */
    protected SnippetArticleViewHolder(SuggestionsRecyclerView parent,
            ContextMenuManager contextMenuManager, SuggestionsUiDelegate uiDelegate,
            UiConfig uiConfig, OfflinePageBridge offlinePageBridge, int layoutId) {
        super(layoutId, parent, uiConfig, contextMenuManager);

        mUiDelegate = uiDelegate;
        mSuggestionsBinder = createBinder(uiDelegate);
        mDisplayStyleObserver = new DisplayStyleObserverAdapter(
                itemView, uiConfig, newDisplayStyle -> updateLayout());

        mOfflinePageBridge = offlinePageBridge;

        mExposureTracker = new ImpressionTracker(itemView);
        mExposureTracker.setImpressionThreshold(/* impressionThresholdPx */ 1);
    }

    @Override
    public void onCardTapped() {
        SuggestionsMetrics.recordCardTapped();
        int windowDisposition = WindowOpenDisposition.CURRENT_TAB;
        mUiDelegate.getEventReporter().onSuggestionOpened(
                mArticle, windowDisposition, mUiDelegate.getSuggestionsRanker());
        mUiDelegate.getNavigationDelegate().openSnippet(windowDisposition, mArticle);
    }

    @Override
    public void openItem(int windowDisposition) {
        mUiDelegate.getEventReporter().onSuggestionOpened(
                mArticle, windowDisposition, mUiDelegate.getSuggestionsRanker());
        mUiDelegate.getNavigationDelegate().openSnippet(windowDisposition, mArticle);
    }

    @Override
    public String getUrl() {
        return mArticle.mUrl;
    }

    @Override
    public String getContextMenuTitle() {
        return mArticle.mTitle;
    }

    @Override
    public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
        Boolean isSupported = mCategoryInfo.isContextMenuItemSupported(menuItemId);
        if (isSupported != null) return isSupported;

        return super.isItemSupported(menuItemId);
    }

    @Override
    public void onContextMenuCreated() {
        mUiDelegate.getEventReporter().onSuggestionMenuOpened(mArticle);
    }

    /**
     * Updates ViewHolder with data.
     * @param article The snippet to take the data from.
     * @param categoryInfo The info of the category which the snippet belongs to.
     */
    public void onBindViewHolder(
            final SnippetArticle article, SuggestionsCategoryInfo categoryInfo) {
        super.onBindViewHolder();

        mArticle = article;
        mCategoryInfo = categoryInfo;

        updateLayout();

        mDisplayStyleObserver.attach();
        mSuggestionsBinder.updateViewInformation(mArticle);
        setImpressionListener(this::onImpression);
        mExposureTracker.setListener(this::onExposure);

        refreshOfflineBadgeVisibility();
    }

    @Override
    public void recycle() {
        mDisplayStyleObserver.detach();
        mSuggestionsBinder.recycle();
        mExposureTracker.setListener(null);
        super.recycle();
    }

    /**
     * Triggers a refresh of the offline badge visibility. Intended to be used as
     * {@link NewTabPageViewHolder.PartialBindCallback}
     */
    public static void refreshOfflineBadgeVisibility(NewTabPageViewHolder holder) {
        ((SnippetArticleViewHolder) holder).refreshOfflineBadgeVisibility();
    }

    protected SuggestionsBinder createBinder(SuggestionsUiDelegate uiDelegate) {
        return new SuggestionsBinder(itemView, uiDelegate);
    }

    /**
     * Updates the layout taking into account screen dimensions and the type of snippet displayed.
     */
    private void updateLayout() {
        final int layout = mCategoryInfo.getCardLayout();

        boolean showHeadline = shouldShowHeadline();
        boolean showThumbnail = shouldShowThumbnail(layout);
        boolean showThumbnailVideoBadge = shouldShowThumbnailVideoBadge(showThumbnail);
        boolean showSnippet = shouldShowSnippet();

        mSuggestionsBinder.updateFieldsVisibility(
                showHeadline, showThumbnail, showThumbnailVideoBadge, showSnippet);
    }

    /** If the title is empty (or contains only whitespace characters), we do not show it. */
    private boolean shouldShowHeadline() {
        return !mArticle.mTitle.trim().isEmpty();
    }

    /**
     * @return Whether a thumbnail should be shown for this suggestion.
     */
    private boolean shouldShowThumbnail(int layout) {
        // Minimal cards don't have a thumbnail
        if (layout == ContentSuggestionsCardLayout.MINIMAL_CARD) return false;

        return mArticle.mHasThumbnail;
    }

    private boolean shouldShowThumbnailVideoBadge(boolean showThumbnail) {
        return showThumbnail && mArticle.mIsVideoSuggestion;
    }

    /** Updates the visibility of the card's offline badge by checking the bound article's info. */
    private void refreshOfflineBadgeVisibility() {
        boolean visible = mArticle.getOfflinePageOfflineId() != null;
        mSuggestionsBinder.updateOfflineBadgeVisibility(visible);
    }

    /**
     * @return Whether a snippet should be shown for this suggestion.
     */
    private boolean shouldShowSnippet() {
        return mArticle.mSnippet.length() > 0;
    }

    /**
     * @return The layout resource reference for this card.
     */
    @LayoutRes
    private static int getLayout() {
        return R.layout.content_suggestions_card_modern_reversed;
    }

    private void onExposure() {
        if (mArticle == null || mArticle.mExposed) return;
        mArticle.mExposed = true;
    }

    private void onImpression() {
        if (mArticle == null || mArticle.mSeen) return;
        mArticle.mSeen = true;

        if (SectionList.shouldReportPrefetchedSuggestionsMetrics(mArticle.mCategory)
                && mOfflinePageBridge.isOfflinePageModelLoaded()) {
            // Before reporting prefetched suggestion impression, we ask the offline page model
            // whether the page is actually prefetched to avoid a race condition when the suggestion
            // surface is opened.

            // |tabId| is relevant only for recent tab offline pages, which we do not handle here,
            // so the value is irrelevant.
            int tabId = 0;
            mOfflinePageBridge.selectPageForOnlineUrl(
                    mArticle.getUrl(), tabId, item -> {
                        if (!SuggestionsOfflineModelObserver.isPrefetchedOfflinePage(item)) {
                            return;
                        }
                        NewTabPageUma.recordPrefetchedArticleSuggestionImpressionPosition(
                                mArticle.getPerSectionRank());
                    });
        }

        mUiDelegate.getEventReporter().onSuggestionShown(mArticle);
    }

    @VisibleForTesting
    public void setOfflineBadgeVisibilityForTesting(boolean visible) {
        mSuggestionsBinder.updateOfflineBadgeVisibility(visible);
    }
}
