// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;

import java.util.List;

/**
 * An interface for classes that provide content suggestions.
 */
public interface SuggestionsSource {
    /**
     * An observer for events in the content suggestions service.
     */
    interface Observer {
        /** Called when a category has a new list of content suggestions. */
        void onNewSuggestions(@CategoryInt int category);

        /** Called when a category changed its status. */
        void onCategoryStatusChanged(@CategoryInt int category, @CategoryStatus int newStatus);

        /**
         * Called when a suggestion is invalidated, which means it needs to be removed from the UI
         * immediately. This event may be fired for a category or suggestion that does not
         * currently exist or has never existed and should be ignored in that case.
         */
        void onSuggestionInvalidated(@CategoryInt int category, String idWithinCategory);

        /** Called when the observer should discard the suggestions it has and pull new ones. */
        void onFullRefreshRequired();

        /**
         * Called when the visibility of the suggestions of the specified category is changed.
         * @param category The specified category.
         */
        void onSuggestionsVisibilityChanged(@CategoryInt int category);
    }

    /**
     * Destroys the resources associated with the source and all observers will be unregistered.
     * It should not be used after this is called.
     */
    void destroy();

    /**
     * Fetches new snippets for all remote categories.
     */
    void fetchRemoteSuggestions();

    /**
     * @return Whether remote suggestions are enabled.
     */
    boolean areRemoteSuggestionsEnabled();

    /**
     * Gets the categories in the order in which they should be displayed.
     * @return The categories.
     */
    int[] getCategories();

    /**
     * Gets the status of a category, possibly indicating the reason why it is disabled.
     */
    @CategoryStatus
    int getCategoryStatus(int category);

    /**
     * Gets the meta information of a category.
     */
    SuggestionsCategoryInfo getCategoryInfo(int category);

    /**
     * Gets the current content suggestions for a category, in the order in which they should be
     * displayed. If the status of the category is not one of the available statuses, this will
     * return an empty list.
     */
    List<SnippetArticle> getSuggestionsForCategory(int category);

    /**
     * Fetches the thumbnail image for a content suggestion. A null Bitmap is returned if no image
     * is available. The callback is never called synchronously.
     */
    void fetchSuggestionImage(SnippetArticle suggestion, Callback<Bitmap> callback);

    /**
     * Fetches the favicon for a content suggestion. A null Bitmap is returned if no good favicon is
     * available. The callback is never called synchronously.
     * @param suggestion The suggestion which the favicon should represent.
     * @param minimumSizePx Minimal required size, if only a smaller favicon is available, a null
     * Bitmap is returned.
     * @param desiredSizePx If set to 0, it denotes that the favicon should be returned in its
     * original size (as in favicon cache) without being resized. If not 0, it must be larger or
     * equal to the minimum size and the favicon will be returned resized to this size.
     * @param callback The callback that receives the favicon image.
     */
    void fetchSuggestionFavicon(SnippetArticle suggestion, int minimumSizePx, int desiredSizePx,
            Callback<Bitmap> callback);

    /**
     * Fetches new suggestions.
     * @param category the category to fetch new suggestions for.
     * @param displayedSuggestionIds ids of suggestions already known and that we want to keep.
     * @param successCallback The callback to run with the received suggestions.
     * @param failureRunnable The runnable to be run if the fetch fails.
     */
    void fetchSuggestions(@CategoryInt int category, String[] displayedSuggestionIds,
            Callback<List<SnippetArticle>> successCallback, Runnable failureRunnable);

    /**
     * Tells the source to dismiss the content suggestion.
     */
    void dismissSuggestion(SnippetArticle suggestion);

    /**
     * Tells the source to dismiss the category.
     */
    void dismissCategory(@CategoryInt int category);

    /**
     * Restores all categories previously dismissed with {@link #dismissCategory}.
     */
    void restoreDismissedCategories();

    /**
     * Sets the recipient for update events from the source.
     */
    void addObserver(Observer observer);

    /**
     * Removes an observer. Is no-op if the observer was not already registered.
     */
    void removeObserver(Observer observer);

    /** No-op implementation of {@link SuggestionsSource.Observer}. */
    class EmptyObserver implements Observer {
        @Override
        public void onNewSuggestions(@CategoryInt int category) {}

        @Override
        public void onCategoryStatusChanged(
                @CategoryInt int category, @CategoryStatus int newStatus) {}

        @Override
        public void onSuggestionInvalidated(@CategoryInt int category, String idWithinCategory) {}

        @Override
        public void onFullRefreshRequired() {}

        @Override
        public void onSuggestionsVisibilityChanged(@CategoryInt int category) {}
    }
}
