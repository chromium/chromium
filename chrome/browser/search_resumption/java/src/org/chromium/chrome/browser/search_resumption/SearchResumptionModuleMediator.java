// Copyright 2022 The Chromium Authors
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
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_resumption.SearchResumptionModuleUtils.ModuleNotShownReason;
import org.chromium.chrome.browser.search_resumption.SearchResumptionUserData.SuggestionResult;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

import java.util.List;

/** This class holds querying search suggestions related business logic. */
public class SearchResumptionModuleMediator
        implements OnSuggestionsReceivedListener,
                SignInStateObserver,
                SyncService.SyncStateChangedListener {
    private final ViewStub mStub;
    private final Tab mTabToTrackSuggestion;
    private final Tab mCurrentTab;
    private final SearchResumptionTileBuilder mTileBuilder;
    private final SigninManager mSignInManager;
    private final SyncService mSyncService;
    private final TemplateUrlService mTemplateUrlService;
    private final TemplateUrlServiceObserver mTemplateUrlServiceObserver;
    private AutocompleteController mAutoComplete;
    private PropertyModel mModel;
    // Set the default values of these variable true since all of them have been checked before
    // creating the coordinator in SearchResumptionModuleUtils#shouldShowSearchResumptionModule.
    private boolean mIsDefaultSearchEngineGoogle = true;
    private boolean mIsSignedIn = true;
    private boolean mHasKeepEverythingSynced = true;
    private boolean mUseNewServiceEnabled;

    private @Nullable SearchResumptionModuleView mModuleLayoutView;
    private @Nullable SearchResumptionModuleBridge mSearchResumptionModuleBridge;

    SearchResumptionModuleMediator(
            ViewStub moduleStub,
            Tab tabToTrack,
            Tab currentTab,
            Profile profile,
            SearchResumptionTileBuilder tileBuilder,
            SuggestionResult cachedSuggestions) {
        mStub = moduleStub;
        mTabToTrackSuggestion = tabToTrack;
        mCurrentTab = currentTab;
        mTileBuilder = tileBuilder;
        mUseNewServiceEnabled =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID,
                        SearchResumptionModuleUtils.USE_NEW_SERVICE_PARAM,
                        false);
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mTemplateUrlServiceObserver = this::onTemplateURLServiceChanged;
        mTemplateUrlService.addObserver(mTemplateUrlServiceObserver);

        if (cachedSuggestions != null) {
            showCachedSuggestions(cachedSuggestions);
        } else {
            start(profile);
        }
        mSignInManager = IdentityServicesProvider.get().getSigninManager(profile);
        mSignInManager.addSignInStateObserver(this);
        mSyncService = SyncServiceFactory.getForProfile(profile);
        mSyncService.addSyncStateChangedListener(this);
    }

    @Override
    public void onSuggestionsReceived(AutocompleteResult autocompleteResult, boolean isFinal) {
        if (!isFinal || mModel != null) return;

        if (!shouldShowSuggestionModule(autocompleteResult.getSuggestionsList())) {
            SearchResumptionModuleUtils.recordModuleNotShownReason(
                    ModuleNotShownReason.NOT_ENOUGH_RESULT);
            return;
        }

        showSearchSuggestionModule(
                autocompleteResult.getSuggestionsList(), /* useCachedResults= */ false);
    }

    /** SyncService.SyncStateChangedListener implementation, listens to sync state changes. */
    @Override
    public void syncStateChanged() {
        mHasKeepEverythingSynced = mSyncService.hasKeepEverythingSynced();
        updateVisibility();
    }

    @Override
    public void onSignedIn() {
        mIsSignedIn = true;
        updateVisibility();
    }

    @Override
    public void onSignedOut() {
        mIsSignedIn = false;
        updateVisibility();
    }

    /**
     * Called when the search suggestions are available using the new service API.
     * @param suggestionTexts The display texts of the suggestions.
     * @param suggestionUrls The URLs of the suggestions.
     */
    void onSuggestionsAvailable(String[] suggestionTexts, GURL[] suggestionUrls) {
        if (mModel != null) return;

        if (!shouldShowSuggestionModule(suggestionUrls, suggestionTexts)) {
            SearchResumptionModuleUtils.recordModuleNotShownReason(
                    ModuleNotShownReason.NOT_ENOUGH_RESULT);
            return;
        }

        showSearchSuggestionModule(suggestionTexts, suggestionUrls, /* useCachedResults= */ false);
    }

    /**
     * Inflates the search_resumption_layout and shows the suggestions on the module.
     * @param autocompleteMatches The suggestions to show on the module.
     */
    void showSearchSuggestionModule(
            List<AutocompleteMatch> autocompleteMatches, boolean useCachedResults) {
        if (!initializeModule()) return;

        mTileBuilder.buildSuggestionTile(
                autocompleteMatches,
                mModuleLayoutView.findViewById(R.id.search_resumption_module_tiles_container));
        SearchResumptionModuleUtils.recordModuleShown(useCachedResults);
        if (!useCachedResults) {
            SearchResumptionUserData.getInstance()
                    .cacheSuggestions(
                            mCurrentTab, mTabToTrackSuggestion.getUrl(), autocompleteMatches);
        }
    }

    /** Inflates the search_resumption_layout and shows the suggestions on the module. */
    void showSearchSuggestionModule(String[] texts, GURL[] urls, boolean useCachedResults) {
        if (!initializeModule()) return;

        mTileBuilder.buildSuggestionTile(
                texts,
                urls,
                mModuleLayoutView.findViewById(R.id.search_resumption_module_tiles_container));
        SearchResumptionModuleUtils.recordModuleShown(useCachedResults);
        if (!useCachedResults) {
            SearchResumptionUserData.getInstance()
                    .cacheSuggestions(mCurrentTab, mTabToTrackSuggestion.getUrl(), texts, urls);
        }
    }

    void destroy() {
        if (mAutoComplete != null) {
            mAutoComplete.removeOnSuggestionsReceivedListener(this);
        }
        if (mModuleLayoutView != null) {
            mModuleLayoutView.destroy();
        }
        if (mSearchResumptionModuleBridge != null) {
            mSearchResumptionModuleBridge.destroy();
        }
        mTemplateUrlService.removeObserver(mTemplateUrlServiceObserver);
        mSignInManager.removeSignInStateObserver(this);
        mSyncService.removeSyncStateChangedListener(this);
    }

    /** Starts the querying the search suggestions based on the Tab to track. */
    private void start(Profile profile) {
        if (!mUseNewServiceEnabled) {
            AutocompleteController.getForProfile(profile)
                    .ifPresent(
                            controller -> {
                                mAutoComplete = controller;
                                mAutoComplete.addOnSuggestionsReceivedListener(this);
                                int pageClassification = getPageClassification();
                                mAutoComplete.startZeroSuggest(
                                        "",
                                        mTabToTrackSuggestion.getUrl(),
                                        pageClassification,
                                        mTabToTrackSuggestion.getTitle(),
                                        /* isOnFocusContext= */ false);
                            });
        } else {
            mSearchResumptionModuleBridge = new SearchResumptionModuleBridge(profile);
            mSearchResumptionModuleBridge.fetchSuggestions(
                    mTabToTrackSuggestion.getUrl().getSpec(), this::onSuggestionsAvailable);
        }
    }

    private void showCachedSuggestions(SuggestionResult cachedSuggestions) {
        if (mUseNewServiceEnabled) {
            showSearchSuggestionModule(
                    cachedSuggestions.getSuggestionTexts(),
                    cachedSuggestions.getSuggestionUrls(),
                    /* useCachedResults= */ true);
        } else {
            showSearchSuggestionModule(
                    cachedSuggestions.getSuggestions(), /* useCachedResults= */ true);
        }
    }

    /**
     * Gets the page classification based on whether the tracking Tab is a search results page or
     * not.
     */
    private int getPageClassification() {
        if (mTemplateUrlService.isSearchResultsPageFromDefaultSearchProvider(
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

    /**
     * Returns whether to show the search resumption module. Only showing the module if at least
     * {@link SearchResumptionTileBuilder#MAX_TILES_NUMBER} -1 suggestions are given.
     */
    private boolean shouldShowSuggestionModule(GURL[] urls, String[] texts) {
        if (urls.length != texts.length
                || urls.length < SearchResumptionTileBuilder.MAX_TILES_NUMBER - 1) {
            return false;
        }

        int count = 0;
        for (int i = 0; i < urls.length; i++) {
            if (SearchResumptionTileBuilder.isSuggestionValid(texts[i])) {
                count++;
            }
            if (count >= SearchResumptionTileBuilder.MAX_TILES_NUMBER - 1) {
                return true;
            }
        }
        return false;
    }

    /**
     * Inflates the module and initializes the property model.
     * @return Whether the module is inflated.
     */
    private boolean initializeModule() {
        if (mModel != null) return false;

        mModuleLayoutView = (SearchResumptionModuleView) mStub.inflate();
        mModel = new PropertyModel(SearchResumptionModuleProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mModuleLayoutView, new SearchResumptionModuleViewBinder());
        mModel.set(
                SearchResumptionModuleProperties.EXPAND_COLLAPSE_CLICK_CALLBACK,
                this::onExpandedOrCollapsed);
        return true;
    }

    private void updateVisibility() {
        if (mModel != null) {
            mModel.set(
                    SearchResumptionModuleProperties.IS_VISIBLE,
                    mIsDefaultSearchEngineGoogle && mIsSignedIn && mHasKeepEverythingSynced);
        }
    }

    /**
     * Saves the user's choice of expanding/collapsing of the suggestions in the SharedPreference.
     */
    @VisibleForTesting
    void onExpandedOrCollapsed(Boolean expanded) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP, !expanded);
        RecordUserAction.record(
                expanded
                        ? SearchResumptionModuleUtils.ACTION_EXPAND
                        : SearchResumptionModuleUtils.ACTION_COLLAPSE);
    }

    @VisibleForTesting
    void onTemplateURLServiceChanged() {
        mIsDefaultSearchEngineGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        updateVisibility();
    }
}
