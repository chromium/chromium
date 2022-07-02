// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.view.View;
import android.view.ViewStub;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;

import java.util.List;

/**
 * This class holds querying search suggestions related business logic.
 */
public class SearchResumptionModuleMediator implements OnSuggestionsReceivedListener {
    static final String ACTION_CLICK = "SearchResumptionModule.NTP.Click";
    private static final String ACTION_SHOW = "SearchResumptionModule.NTP.Show";

    private final ViewStub mStub;
    private final Tab mTabToTrackSuggestion;
    private final SearchResumptionTileBuilder mTileBuilder;

    private AutocompleteController mAutoComplete;
    private View mModuleLayoutView;
    private SearchResumptionContainerView mSuggestionTilesContainerView;

    SearchResumptionModuleMediator(ViewStub moduleStub, Tab tabToTrack, Profile profile,
            SearchResumptionTileBuilder tileBuilder) {
        mStub = moduleStub;
        mTabToTrackSuggestion = tabToTrack;
        mTileBuilder = tileBuilder;
        start(profile);
    }

    @Override
    public void onSuggestionsReceived(
            AutocompleteResult autocompleteResult, String inlineAutocompleteText, boolean isFinal) {
        if (!isFinal || mModuleLayoutView != null
                || !shouldShowSuggestionModule(autocompleteResult.getSuggestionsList())) {
            return;
        }
        showSearchSuggestionModule(autocompleteResult);
    }

    /**
     * Inflates the search_resumption_layout and shows the suggestions on the module.
     * @param autocompleteResult The suggestions to show on the module.
     */
    void showSearchSuggestionModule(AutocompleteResult autocompleteResult) {
        if (mModuleLayoutView != null) return;

        mModuleLayoutView = mStub.inflate();
        mSuggestionTilesContainerView =
                mModuleLayoutView.findViewById(R.id.search_resumption_module_tiles_container);
        mTileBuilder.buildSuggestionTile(
                autocompleteResult.getSuggestionsList(), mSuggestionTilesContainerView);

        mModuleLayoutView.setVisibility(View.VISIBLE);
        RecordUserAction.record(ACTION_SHOW);
    }

    void destroy() {
        mAutoComplete.removeOnSuggestionsReceivedListener(this);
        if (mSuggestionTilesContainerView != null) {
            mSuggestionTilesContainerView.destroy();
        }
    }

    /**
     * Starts the querying the search suggestions based on the Tab to track.
     */
    private void start(Profile profile) {
        mAutoComplete = AutocompleteController.getForProfile(profile);
        mAutoComplete.addOnSuggestionsReceivedListener(this);
        int pageClassification = getPageClassification();
        mAutoComplete.startZeroSuggest("", mTabToTrackSuggestion.getUrl().getSpec(),
                pageClassification, mTabToTrackSuggestion.getTitle());
    }

    /**
     * Gets the page classification based on whether the tracking Tab is a search results page or
     * not.
     */
    private int getPageClassification() {
        if (TemplateUrlServiceFactory.get().isSearchResultsPageFromDefaultSearchProvider(
                    mTabToTrackSuggestion.getUrl())) {
            return PageClassification.SEARCH_RESUMPTION_SEARCH_RESULT_PAGE_VALUE;
        } else {
            return PageClassification.SEARCH_RESUMPTION_OTHER_VALUE;
        }
    }

    /**
     * Returns whether to show the search resumption module. Only showing the module if at least
     * {@link SearchResumptionTileBuilder#MAX_TILES_NUMBER} -1 suggestions are given.
     */
    private boolean shouldShowSuggestionModule(List<AutocompleteMatch> suggestions) {
        if (suggestions.size() < SearchResumptionTileBuilder.MAX_TILES_NUMBER - 1) return false;

        int count = 0;
        for (AutocompleteMatch suggestion : suggestions) {
            if (SearchResumptionTileBuilder.isSearchSuggestion(suggestion)) {
                count++;
            }
            if (count >= SearchResumptionTileBuilder.MAX_TILES_NUMBER - 1) {
                return true;
            }
        }
        return false;
    }
}