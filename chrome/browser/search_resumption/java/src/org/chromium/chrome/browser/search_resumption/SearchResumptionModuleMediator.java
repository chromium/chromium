// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.view.ViewStub;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * This class holds querying search suggestions related business logic.
 */
public class SearchResumptionModuleMediator
        implements OnSuggestionsReceivedListener, SignInStateObserver {
    static final String ACTION_CLICK = "SearchResumptionModule.NTP.Click";
    private static final String ACTION_SHOW = "SearchResumptionModule.NTP.Show";

    private final ViewStub mStub;
    private final Tab mTabToTrackSuggestion;
    private final SearchResumptionTileBuilder mTileBuilder;
    private final SigninManager mSignInManager;
    private AutocompleteController mAutoComplete;
    private PropertyModel mModel;

    private @Nullable SearchResumptionModuleView mModuleLayoutView;

    SearchResumptionModuleMediator(ViewStub moduleStub, Tab tabToTrack, Profile profile,
            SearchResumptionTileBuilder tileBuilder) {
        mStub = moduleStub;
        mTabToTrackSuggestion = tabToTrack;
        mTileBuilder = tileBuilder;
        start(profile);
        mSignInManager = IdentityServicesProvider.get().getSigninManager(profile);
        mSignInManager.addSignInStateObserver(this);
        TemplateUrlServiceFactory.get().addObserver(this::onTemplateURLServiceChanged);
    }

    @Override
    public void onSuggestionsReceived(
            AutocompleteResult autocompleteResult, String inlineAutocompleteText, boolean isFinal) {
        if (!isFinal || mModel != null
                || !shouldShowSuggestionModule(autocompleteResult.getSuggestionsList())) {
            return;
        }
        showSearchSuggestionModule(autocompleteResult);
    }

    @Override
    public void onSignedIn() {
        setVisibility(true);
    }

    @Override
    public void onSignedOut() {
        setVisibility(false);
    }

    /**
     * Inflates the search_resumption_layout and shows the suggestions on the module.
     * @param autocompleteResult The suggestions to show on the module.
     */
    void showSearchSuggestionModule(AutocompleteResult autocompleteResult) {
        if (mModel != null) return;

        mModuleLayoutView = (SearchResumptionModuleView) mStub.inflate();
        mModel = new PropertyModel(SearchResumptionModuleProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mModuleLayoutView, new SearchResumptionModuleViewBinder());

        mTileBuilder.buildSuggestionTile(autocompleteResult.getSuggestionsList(),
                mModuleLayoutView.findViewById(R.id.search_resumption_module_tiles_container));
        mModel.set(SearchResumptionModuleProperties.EXPAND_COLLAPSE_CLICK_CALLBACK,
                this::onExpandedOrCollapsed);
        RecordUserAction.record(ACTION_SHOW);
    }

    void destroy() {
        if (mAutoComplete != null) {
            mAutoComplete.removeOnSuggestionsReceivedListener(this);
        }
        if (mModuleLayoutView != null) {
            mModuleLayoutView.destroy();
        }
        TemplateUrlServiceFactory.get().removeObserver(this::onTemplateURLServiceChanged);
        mSignInManager.removeSignInStateObserver(this);
    }

    /**
     * Starts the querying the search suggestions based on the Tab to track.
     */
    private void start(Profile profile) {
        if (!ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID,
                    SearchResumptionModuleUtils.USE_NEW_SERVICE_PARAM, false)) {
            mAutoComplete = AutocompleteController.getForProfile(profile);
            mAutoComplete.addOnSuggestionsReceivedListener(this);
            int pageClassification = getPageClassification();
            mAutoComplete.startZeroSuggest("", mTabToTrackSuggestion.getUrl().getSpec(),
                    pageClassification, mTabToTrackSuggestion.getTitle());
        } else {
            // TODO(hanxi): Switches to use a new KeyedService instead of Autocomplete API.
        }
    }

    /**
     * Gets the page classification based on whether the tracking Tab is a search results page or
     * not.
     */
    private int getPageClassification() {
        if (TemplateUrlServiceFactory.get().isSearchResultsPageFromDefaultSearchProvider(
                    mTabToTrackSuggestion.getUrl())) {
            return PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE;
        } else {
            return PageClassification.OTHER_VALUE;
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

    private void setVisibility(boolean isVisible) {
        if (mModel != null) {
            mModel.set(SearchResumptionModuleProperties.IS_VISIBLE, isVisible);
        }
    }

    /**
     * Saves the user's choice of expanding/collapsing of the suggestions in the SharedPreference.
     */
    private void onExpandedOrCollapsed(Boolean expanded) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP, !expanded);
    }

    @VisibleForTesting
    void onTemplateURLServiceChanged() {
        setVisibility(TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle());
    }
}