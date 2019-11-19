// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;

import java.util.List;

/**
 * An empty {@link SuggestionsSource}.
 */
public class EmptySuggestionsSource implements SuggestionsSource {
    @Override
    public void destroy() {}

    @Override
    public void fetchRemoteSuggestions() {}

    @Override
    public boolean areRemoteSuggestionsEnabled() {
        return false;
    }

    @Override
    public int[] getCategories() {
        return new int[0];
    }

    @Override
    public int getCategoryStatus(int category) {
        return CategoryStatus.NOT_PROVIDED;
    }

    @Override
    public SuggestionsCategoryInfo getCategoryInfo(int category) {
        return null;
    }

    @Override
    public List<SnippetArticle> getSuggestionsForCategory(int category) {
        return null;
    }

    @Override
    public void fetchSuggestionImage(SnippetArticle suggestion, Callback<Bitmap> callback) {}

    @Override
    public void fetchSuggestionFavicon(SnippetArticle suggestion, int minimumSizePx,
            int desiredSizePx, Callback<Bitmap> callback) {}

    @Override
    public void fetchSuggestions(int category, String[] displayedSuggestionIds,
            Callback<List<SnippetArticle>> successCallback, Runnable failureRunnable) {}

    @Override
    public void dismissSuggestion(SnippetArticle suggestion) {}

    @Override
    public void dismissCategory(int category) {}

    @Override
    public void restoreDismissedCategories() {}

    @Override
    public void addObserver(Observer observer) {}

    @Override
    public void removeObserver(Observer observer) {}
}
